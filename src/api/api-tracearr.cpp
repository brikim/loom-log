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

      constexpr std::string_view TRACEARR_MEDIA_TYPE_MOVIE{"movie"};
      constexpr std::string_view TRACEARR_MEDIA_TYPE_EPISODE{"episode"};
      constexpr std::string_view TRACEARR_STOPPED("stopped");
   }

   struct ServerData
   {
      ApiType apiType;
      std::string tracearrServerName;
      std::string serverName;
   };

   struct TracearrApi::TracearrApiImpl
   {
      TracearrApi& parent_;
      Headers headers_;
      mutable std::shared_mutex dataLock_;

      std::vector<TracearrServerData> configServers_;
      std::vector<ServerData> servers_;

      TracearrApiImpl(TracearrApi& p, std::string_view appName, std::string_view version, const std::vector<TracearrServerData>& configServers);

      void RefreshServerData();
      void RefreshCache(bool forceRefresh);
      bool GetServersValid() const;
   };

   TracearrApi::TracearrApiImpl::TracearrApiImpl(TracearrApi& p, std::string_view appName, std::string_view version, const std::vector<TracearrServerData>& configServers)
      : parent_(p)
      , configServers_(configServers)
   {
      servers_.reserve(configServers.size());

      headers_ = {
         {"Authorization", std::format("Bearer {}", parent_.GetApiKey())},
         {"Content-Type", APPLICATION_JSON},
         {"User-Agent", std::format("{}/{}", appName, version)}
      };

      RefreshCache(true);
   }

   TracearrApi::TracearrApi(std::string_view appName, std::string_view version, const TracearrConfig& serverConfig)
      : ApiBase(ApiBaseData{.name = serverConfig.serverName,
            .url = serverConfig.url,
            .apiKey = serverConfig.apiKey,
            .className = "TracearrApi",
            .ansiiCode = ANSI_CODE_TRACEARR,
            .prettyName = GetServerName(GetFormattedTracearr(), serverConfig.serverName)})
      , pimpl_(std::make_unique<TracearrApiImpl>(*this, appName, version, serverConfig.servers))
   {}

   TracearrApi::~TracearrApi() = default;

   std::optional<std::vector<Task>> TracearrApi::GetTaskList()
   {
      std::vector<Task> tasks;

      auto& quickCheck = tasks.emplace_back();
      quickCheck.name = std::format("{} - Refresh Server Quick", GetPrettyName());
      quickCheck.cronExpression = GetNextCronQuickTime();
      quickCheck.func = [this]() {pimpl_->RefreshCache(false); };

      auto& fullUpdate = tasks.emplace_back();
      fullUpdate.name = std::format("{} - Server Update", GetPrettyName());
      fullUpdate.cronExpression = GetNextCronFullTime();
      fullUpdate.func = [this]() {pimpl_->RefreshCache(true); };

      return tasks;
   }

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

   bool TracearrApi::TracearrApiImpl::GetServersValid() const
   {
      std::shared_lock lock(dataLock_);
      return !servers_.empty();
   }

   std::optional<TracearrServerInfo> TracearrApi::GetServerData(std::string_view tracearrServerName) const
   {
      std::shared_lock lock(pimpl_->dataLock_);
      auto iter = std::ranges::find_if(pimpl_->servers_, [tracearrServerName](const auto& server) {
         return server.tracearrServerName == tracearrServerName;
      });
      if (iter != pimpl_->servers_.end())
      {
         return TracearrServerInfo{.serverName = iter->serverName, .apiType = iter->apiType};
      }
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
         if (item.mediaType != TRACEARR_MEDIA_TYPE_MOVIE && item.mediaType != TRACEARR_MEDIA_TYPE_EPISODE && item.state != TRACEARR_STOPPED)
         {
            continue;
         }

         int progressMs = 0;
         auto [ptr, ec] = std::from_chars(item.progressMs.data(), item.progressMs.data() + item.progressMs.size(), progressMs);

         int totalDurationMs = 0;
         if (item.totalDurationMs.has_value())
         {
            auto [ptr, ec] = std::from_chars(item.totalDurationMs->data(), item.totalDurationMs->data() + item.totalDurationMs->size(), totalDurationMs);
         }

         auto id = std::format("{}-{}-{}-{}-{}",
            item.serverName,
            item.mediaTitle,
            item.mediaType,
            item.showTitle.value_or(""),
            item.user.userName);

         auto fullName = item.showTitle.has_value() ? std::format("{} - {}", item.showTitle.value(), item.mediaTitle) : item.mediaTitle;

         returnResponse.items.emplace_back(TracearrHistoryItem{
            .id = std::move(id),
            .serverName = std::move(item.serverName),
            .fullName = std::move(fullName),
            .mediaTitle = std::move(item.mediaTitle),
            .mediaType = std::move(item.mediaType),
            .showTitle = std::move(item.showTitle),
            .seasonNumber = item.seasonNumber,
            .episodeNumber = item.episodeNumber,
            .progressMs = progressMs,
            .totalDurationMs = totalDurationMs,
            .playbackPercentage = static_cast<int32_t>(std::lround(item.percentComplete)),
            .startedAt = std::move(item.startedAt),
            .stoppedAt = std::move(item.stoppedAt),
            .watched = item.watched,
            .serverRatingKey = std::move(item.serverRatingKey),
            .user = std::move(item.user.userName)
         });
      }

      return returnResponse;
   }

   void TracearrApi::TracearrApiImpl::RefreshServerData()
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

      servers_.clear();
      for (const auto& configServer : configServers_)
      {
         auto iter = std::ranges::find_if(serverResponse.servers, [&configServer](const auto& s) {
            return s.name == configServer.tracearrServerName;
         });
         if (iter != serverResponse.servers.end())
         {
            ApiType apiType;
            if (iter->type == "plex")
               apiType = ApiType::PLEX;
            else if (iter->type == "emby")
               apiType = ApiType::EMBY;
            else
               continue;

            std::unique_lock lock(dataLock_);
            servers_.emplace_back(ServerData{
               .apiType = apiType,
               .tracearrServerName = std::move(iter->name),
               .serverName = configServer.serverName
            });
         }
         else
         {
            parent_.LogWarning("{} - Tracearr Server {} not reported by API",
                               __func__, configServer.tracearrServerName);
         }
      }
   }

   void TracearrApi::TracearrApiImpl::RefreshCache(bool forceRefresh)
   {
      if (forceRefresh || !GetServersValid())
         RefreshServerData();
   }
}