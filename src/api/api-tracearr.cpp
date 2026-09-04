#include "warp/api/api-tracearr.h"

#include "api/api-tracearr-json-types.h"
#include "api/api-utils.h"
#include "warp/log/log-utils.h"
#include "warp/utils.h"

#include <glaze/glaze.hpp>

#include <format>
#include <shared_mutex>
#include <vector>

namespace warp
{
   namespace
   {
      constexpr std::string_view API_V1_BASE{"/api/v1/public"};
      constexpr std::string_view API_V2_BASE{"/api/v2/public"};
      constexpr std::string_view API_GET_HEALTH{"/health"};
      constexpr std::string_view API_GET_HISTORY{"/history"};
      constexpr std::string_view API_GET_USERS{"/users"};

      constexpr std::string_view TRACEARR_MEDIA_TYPE_MOVIE{"movie"};
      constexpr std::string_view TRACEARR_MEDIA_TYPE_EPISODE{"episode"};
      constexpr std::string_view TRACEARR_STOPPED("stopped");

      struct ServerData
      {
         ApiType apiType{ApiType::UNKNOWN};
         std::string tracearrServerName;
         std::string serverName;
      };


      struct AccountData
      {
         std::string serverId;
         std::optional<TracearrServerType> serverType;
         std::string serverUserId;
         std::string externalUserId;
         std::string externalUserName;
      };

      struct UserData
      {
         std::string name;
         std::vector<AccountData> accounts;
      };
   }

   struct TracearrApi::TracearrApiImpl
   {
      TracearrApi& parent_;
      Headers headers_;
      mutable std::shared_mutex dataLock_;

      using TracearrIdToNameMap = std::unordered_map<std::string, std::string, StringHash, std::equal_to<>>;
      TracearrIdToNameMap servers_;

      using TracearrUserMap = std::unordered_map<std::string, UserData, StringHash, std::equal_to<>>;
      TracearrUserMap users_;

      TracearrApiImpl(TracearrApi& p, std::string_view appName, std::string_view version);

      std::optional<TracearrServerType> GetServerApiType(std::string_view serverId);

      [[nodiscard]] bool GetServerMapEmpty() const;
      [[nodiscard]] bool GetUseresMapEmpty() const;

      void RebuildServerMap();
      void RebuildUsersMap();

      void UpdateRequiredCache(bool forceRefresh);
      void RefreshCache(bool forceRefresh);
   };

   TracearrApi::TracearrApiImpl::TracearrApiImpl(TracearrApi& p, std::string_view appName, std::string_view version)
      : parent_(p)
   {
      headers_ = {
         {"Authorization", std::format("Bearer {}", parent_.GetApiKey())},
         {"Content-Type", APPLICATION_JSON},
         {"User-Agent", std::format("{}/{}", appName, version)}
      };

      UpdateRequiredCache(true);
   }

   TracearrApi::TracearrApi(std::string_view appName, std::string_view version, const TracearrConfig& serverConfig)
      : ApiBase(ApiBaseData{.name = serverConfig.serverName,
            .url = serverConfig.url,
            .apiKey = serverConfig.apiKey,
            .className = "TracearrApi",
            .ansiiCode = ANSI_CODE_TRACEARR,
            .prettyName = GetServerName(GetFormattedTracearr(), serverConfig.serverName)})
      , pimpl_(std::make_unique<TracearrApiImpl>(*this, appName, version))
   {}

   TracearrApi::~TracearrApi() = default;

   std::string_view TracearrApi::GetApiBase(std::optional<int32_t> version) const
   {
      if (!version)
      {
         LogWarning("{} - API version not set. Defaulting to v1", __func__);
         return API_V1_BASE;
      }

      switch (version.value())
      {
         case 1:
            return API_V1_BASE;
         case 2:
            return API_V2_BASE;
         default:
            LogWarning("{} - Unknown API version: {}. Defaulting to v1", __func__, version.value());
            return API_V1_BASE;
      }
   }

   std::string_view TracearrApi::GetApiTokenName() const
   {
      // Jellystat api key is sent via header
      return "";
   }

   bool TracearrApi::GetValid()
   {
      auto res = Get(BuildApiPath(API_GET_HEALTH, 1), pimpl_->headers_);
      return res.error == Error::Success && res.status < VALID_HTTP_RESPONSE_MAX;
   }

   std::optional<std::string> TracearrApi::GetServerReportedName()
   {
      auto res = Get(BuildApiPath(API_GET_HEALTH, 1), pimpl_->headers_);
      if (!IsHttpSuccess(__func__, res))
      {
         return std::nullopt;
      }

      JsonHealthResponse serverResponse;
      if (auto ec = glz::read < glz::opts{.error_on_unknown_keys = false} > (serverResponse, res.body))
      {
         LogWarning("{} - JSON Parse Error: {}",
                    __func__, glz::format_error(ec, res.body));
         return std::nullopt;
      }

      return std::format("Tracearr({})", serverResponse.version);
   }

   std::optional<std::vector<Task>> TracearrApi::GetTaskList()
   {
      std::vector<Task> tasks;

      auto& quickCheck = tasks.emplace_back();
      quickCheck.name = std::format("{} - Refresh Cache Quick", GetPrettyName());
      quickCheck.cronExpression = GetNextCronQuickTime();
      quickCheck.func = [this]() {pimpl_->RefreshCache(false); };

      auto& fullUpdate = tasks.emplace_back();
      fullUpdate.name = std::format("{} - Refresh Cache Full", GetPrettyName());
      fullUpdate.cronExpression = GetNextCronFullTime();
      fullUpdate.func = [this]() {pimpl_->RefreshCache(true); };

      return tasks;
   }

   std::optional<TracearrServerType> TracearrApi::TracearrApiImpl::GetServerApiType(std::string_view type)
   {
      if (type == "plex") return TracearrServerType::PLEX;
      if (type == "emby") return TracearrServerType::EMBY;
      if (type == "jellyfin") return TracearrServerType::JELLYFIN;
      return std::nullopt;
   }

   // Returns the watch history for all servers
   std::optional<TracearrHistoryItems> TracearrApi::GetWatchHistory(std::string_view dateForHistory)
   {
      ApiParams params = {
         {"since", dateForHistory},
         {"pageSize", "50"}
      };

      auto res = Get(BuildApiParamsPath(API_GET_HISTORY, params, 2), pimpl_->headers_);
      if (!IsHttpSuccess(__func__, res))
      {
         return std::nullopt;
      }

      JsonTracearrHistoryItems serverResponse;
      if (auto ec = glz::read < glz::opts{.error_on_unknown_keys = false} > (serverResponse, res.body))
      {
         LogWarning("{} - JSON Parse Error: {}", __func__, glz::format_error(ec, res.body));
         return std::nullopt;
      }

      // If no items received, return nullopt to signify no history instead of an empty list
      if (serverResponse.items.empty())
         return std::nullopt;

      TracearrHistoryItems returnResponse;
      returnResponse.items.reserve(serverResponse.items.size());
      for (auto& item : serverResponse.items)
      {
         // For now syncing will only work with movies and tv episodes. Ignore other media types for now.
         // Also only process items that are in the stopped state.
         bool isValidMedia = (item.mediaType == TRACEARR_MEDIA_TYPE_MOVIE || item.mediaType == TRACEARR_MEDIA_TYPE_EPISODE);
         bool isStopped = (item.state == TRACEARR_STOPPED);
         if (!isValidMedia || !isStopped)
         {
            continue;
         }

         auto serverType = pimpl_->GetServerApiType(item.serverType);
         if (serverType.has_value() == false)
         {
            LogWarning("{} - Unknown server type: {}. Skipping item.", __func__, item.serverType);
            continue;
         }

         auto fullName = item.showTitle.has_value()
            ? std::format("{} - {}", item.showTitle.value(), item.mediaTitle)
            : item.mediaTitle;
         auto id = std::format("{}-{}-{}",
            item.user.userName,
            item.serverName,
            item.serverRatingKey);

         returnResponse.items.emplace_back(TracearrHistoryItem{
            .id = std::move(id),
            .serverId = std::move(item.serverId),
            .serverName = std::move(item.serverName),
            .serverType = serverType.value(),
            .fullName = std::move(fullName),
            .mediaTitle = std::move(item.mediaTitle),
            .mediaType = std::move(item.mediaType),
            .showTitle = std::move(item.showTitle),
            .seasonNumber = item.seasonNumber,
            .episodeNumber = item.episodeNumber,
            .playbackPercentage = item.percentComplete.has_value() ? std::make_optional(static_cast<int32_t>(std::lround(item.percentComplete.value()))) : std::nullopt,
            .watchTime = item.stoppedAt.has_value() ? std::move(item.stoppedAt.value()) : std::move(item.startedAt),
            .watched = item.watched,
            .serverRatingKey = std::move(item.serverRatingKey),
            .user = TracearrHistoryUser{
               .id = std::move(item.user.id),
               .name = std::move(item.user.userName)
            }
         });
      }

      return returnResponse;
   }

   std::optional<std::string_view> TracearrApi::GetServerNameFromId(std::string_view id)
   {
      std::shared_lock lock(pimpl_->dataLock_);
      auto it = pimpl_->servers_.find(id);
      if (it != pimpl_->servers_.end())
      {
         return it->second;
      }
      return std::nullopt;
   }

   std::optional<TracearrUser> TracearrApi::GetUser(std::string_view id)
   {
      std::shared_lock lock(pimpl_->dataLock_);
      auto it = pimpl_->users_.find(id);
      if (it != pimpl_->users_.end())
      {
         TracearrUser returnUser;
         returnUser.id = it->first;
         returnUser.name = it->second.name;
         for (const auto& account : it->second.accounts)
         {
            returnUser.accounts.emplace_back(TracearrUserAccount{
               .serverId = account.serverId,
               .serverType = account.serverType,
               .serverUserId = account.serverUserId,
               .externalUserId = account.externalUserId,
               .externalUserName = account.externalUserName
            });
         }
         return returnUser;
      }
      return std::nullopt;
   }

   bool TracearrApi::TracearrApiImpl::GetServerMapEmpty() const
   {
      std::shared_lock lock(dataLock_);
      return servers_.empty();
   }

   bool TracearrApi::TracearrApiImpl::GetUseresMapEmpty() const
   {
      std::shared_lock lock(dataLock_);
      return users_.empty();
   }

   void TracearrApi::TracearrApiImpl::RebuildServerMap()
   {
      auto res = parent_.Get(parent_.BuildApiPath(API_GET_HEALTH, 1), headers_);
      if (!parent_.IsHttpSuccess(__func__, res))
      {
         return;
      }

      JsonHealthResponse serverResponse;
      if (auto ec = glz::read < glz::opts{.error_on_unknown_keys = false} > (serverResponse, res.body))
      {
         parent_.LogWarning("{} - JSON Parse Error: {}",
                    __func__, glz::format_error(ec, res.body));
         return;
      }

      TracearrIdToNameMap workingServers;
      workingServers.reserve(serverResponse.servers.size());

      for (auto& server : serverResponse.servers)
      {
         workingServers.emplace(std::move(server.id), std::move(server.name));
      }

      if (!workingServers.empty())
      {
         std::unique_lock lock(dataLock_);
         servers_ = std::move(workingServers);
      }
      else
      {
         parent_.LogWarning("{} - Keeping stale server data due to fetch failures", __func__);
      }
   }

   void TracearrApi::TracearrApiImpl::RebuildUsersMap()
   {
      auto res = parent_.Get(parent_.BuildApiPath(API_GET_USERS, 2), headers_);
      if (!parent_.IsHttpSuccess(__func__, res))
      {
         return;
      }

      JsonTracearrUsers serverResponse;
      if (auto ec = glz::read < glz::opts{.error_on_unknown_keys = false} > (serverResponse, res.body))
      {
         parent_.LogWarning("{} - JSON Parse Error: {}",
                    __func__, glz::format_error(ec, res.body));
         return;
      }

      TracearrUserMap workingUsers;
      workingUsers.reserve(serverResponse.users.size());

      for (auto& user : serverResponse.users)
      {
         UserData userData;
         userData.name = std::move(user.name);
         userData.accounts.reserve(user.accounts.size());
         for (auto& account : user.accounts)
         {
            userData.accounts.emplace_back(AccountData{
               .serverId = std::move(account.serverId),
               .serverType = GetServerApiType(account.serverType),
               .serverUserId = std::move(account.serverUserId),
               .externalUserId = std::move(account.externalUserId),
               .externalUserName = std::move(account.externalUserName)
            });
         }

         if (userData.accounts.empty())
         {
            parent_.LogWarning("{} - User {} has no valid accounts. Skipping user.", __func__, user.name);
            continue;
         }

         workingUsers.emplace(std::move(user.id), std::move(userData));
      }

      if (!workingUsers.empty())
      {
         std::unique_lock lock(dataLock_);
         users_ = std::move(workingUsers);
      }
      else
      {
         parent_.LogWarning("{} - Keeping stale user data due to fetch failures", __func__);
      }
   }

   void TracearrApi::TracearrApiImpl::UpdateRequiredCache(bool forceRefresh)
   {
      if (forceRefresh || GetServerMapEmpty())
         RebuildServerMap();

      if (forceRefresh || GetUseresMapEmpty())
         RebuildUsersMap();
   }

   void TracearrApi::TracearrApiImpl::RefreshCache(bool forceRefresh)
   {
      UpdateRequiredCache(forceRefresh);
   }
}