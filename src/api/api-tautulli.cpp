#include "warp/api/api-tautulli.h"

#include "api/api-tautulli-json-types.h"
#include "warp/log/log-utils.h"
#include "warp/utils.h"

#include <glaze/glaze.hpp>

#include <format>
#include <mutex>
#include <ranges>
#include <shared_mutex>

namespace warp
{
   namespace
   {
      constexpr std::string_view API_BASE{"/api/v2"};
      constexpr std::string_view API_TOKEN_NAME{"apikey"};
      constexpr std::string_view API_COMMAND{"cmd"};

      constexpr std::string_view CMD_GET_SERVER_FRIENDLY_NAME("get_server_friendly_name");
      constexpr std::string_view CMD_GET_SETTINGS{"get_settings"};
      constexpr std::string_view CMD_GET_USERS("get_users");
      constexpr std::string_view CMD_GET_HISTORY("get_history");
      constexpr std::string_view CMD_SERVER_INFO{"get_server_info"};

      constexpr std::string_view USER("user");
      constexpr std::string_view INCLUDE_ACTIVITY("include_activity");
      constexpr std::string_view AFTER("after");
      constexpr std::string_view SEARCH("search");
      constexpr std::string_view SECTION_ID("section_id");
   }

   struct TautulliApi::TautulliApiImpl
   {
      TautulliApi& parent_;
      Headers headers_;
      std::optional<int32_t> watchedPercent_;

      using NameToUserMap = std::unordered_map<std::string, TautulliUserInfo, StringHash, std::equal_to<>>;
      NameToUserMap users_;
      NameToUserMap workingUsers_;

      mutable std::shared_mutex dataLock_;

      TautulliApiImpl(TautulliApi& p, std::string_view appName, std::string_view version);

      [[nodiscard]] std::pair<std::string_view, std::string_view> GetCmdParam(std::string_view cmd) const;

      [[nodiscard]] std::optional<TautulliHistoryItems> GetWatchHistory(std::string_view user,
                                                                        int64_t epochHistoryTime,
                                                                        const ApiParams& extraParams);

      int32_t GetWatchedPercent();

      std::optional<TautulliUserInfo> GetUserInfo(std::string_view name);

      bool RefreshMonitoringData();
      void RefreshUserData();
      void RefreshCache(bool forceRefresh);

      bool GetWatchedPercentValid() const;
      bool GetUserMapEmpty() const;
   };

   TautulliApi::TautulliApiImpl::TautulliApiImpl(TautulliApi& p, std::string_view appName, std::string_view version)
      : parent_(p)
   {
      headers_ = {
         {"User-Agent", std::format("{}/{}", appName, version)},
         {"Accept", "application/json"}
      };

      RefreshCache(true);
   }

   TautulliApi::TautulliApi(std::string_view appName, std::string_view version, const ServerConfig& serverConfig)
      : ApiBase(ApiBaseData{.name = serverConfig.serverName,
            .url = serverConfig.trackerUrl,
            .apiKey = serverConfig.trackerApiKey,
            .className = "TautulliApi",
            .ansiiCode = ANSI_CODE_TAUTULLI,
            .prettyName = GetServerName(GetFormattedTautulli(), serverConfig.serverName)})
      , pimpl_(std::make_unique<TautulliApiImpl>(*this, appName, version))
   {
   }

   TautulliApi::~TautulliApi() = default;

   std::optional<std::vector<Task>> TautulliApi::GetTaskList()
   {
      std::vector<Task> tasks;

      auto& quickCheck = tasks.emplace_back();
      quickCheck.name = std::format("EmbyApi({}) - Refresh Cache Quick", GetName());
      quickCheck.cronExpression = GetNextCronQuickTime();
      quickCheck.func = [this]() {pimpl_->RefreshCache(false); };

      auto& fullUpdate = tasks.emplace_back();
      fullUpdate.name = std::format("TautulliApi({}) - Settings Update", GetName());
      fullUpdate.cronExpression = GetNextCronFullTime();
      fullUpdate.func = [this]() {pimpl_->RefreshCache(true); };

      return tasks;
   }

   std::string_view TautulliApi::GetApiBase() const
   {
      return API_BASE;
   }

   std::string_view TautulliApi::GetApiTokenName() const
   {
      return API_TOKEN_NAME;
   }

   std::pair<std::string_view, std::string_view> TautulliApi::TautulliApiImpl::GetCmdParam(std::string_view cmd) const
   {
      return {API_COMMAND, cmd};
   }

   bool TautulliApi::GetValid()
   {
      auto apiPath = BuildApiParamsPath("", {pimpl_->GetCmdParam(CMD_GET_SERVER_FRIENDLY_NAME)});
      auto res = Get(apiPath, pimpl_->headers_);
      return res.error == Error::Success && res.status < VALID_HTTP_RESPONSE_MAX;
   }

   std::optional<std::string> TautulliApi::GetServerReportedName()
   {
      auto res = Get(BuildApiParamsPath("", {pimpl_->GetCmdParam(CMD_SERVER_INFO)}), pimpl_->headers_);
      if (!IsHttpSuccess(__func__, res))
      {
         return std::nullopt;
      }

      JsonTautulliResponse<JsonTautulliServerInfo> serverResponse;
      if (auto ec = glz::read < glz::opts{.error_on_unknown_keys = false} > (serverResponse, res.body))
      {
         LogWarning("{} - JSON Parse Error: {}",
                    __func__, glz::format_error(ec, res.body));
         return std::nullopt;
      }

      if (serverResponse.response.data.pms_name.empty())
      {
         return std::nullopt;
      }
      return std::move(serverResponse.response.data.pms_name);
   }

   std::optional<TautulliUserInfo> TautulliApi::TautulliApiImpl::GetUserInfo(std::string_view name)
   {
      std::shared_lock lock(dataLock_);
      auto iter = users_.find(name);
      return iter == users_.end() ? std::nullopt : std::make_optional(iter->second);
   }

   std::optional<TautulliUserInfo> TautulliApi::GetUserInfo(std::string_view name)
   {
      return pimpl_->GetUserInfo(name);
   }

   int32_t TautulliApi::TautulliApiImpl::GetWatchedPercent()
   {
      constexpr int32_t defaultWatchedPercent = 85;

      std::shared_lock lock(dataLock_);
      return watchedPercent_ ? *watchedPercent_ : defaultWatchedPercent;
   }

   std::optional<TautulliHistoryItems> TautulliApi::TautulliApiImpl::GetWatchHistory(std::string_view user,
                                                                                     int64_t epochHistoryTime,
                                                                                     const ApiParams& extraParams)
   {
      ApiParams params = {
         GetCmdParam(CMD_GET_HISTORY),
         {INCLUDE_ACTIVITY, "0"},
         {USER, user}
      };
      params.reserve(params.size() + extraParams.size());
      params.insert(params.end(), extraParams.begin(), extraParams.end());

      auto res = parent_.Get(parent_.BuildApiParamsPath("", params), headers_);
      if (!parent_.IsHttpSuccess(__func__, res)) return std::nullopt;

      JsonTautulliResponse<JsonTautulliHistoryData> serverResponse;
      if (auto ec = glz::read < glz::opts{.error_on_unknown_keys = false} > (serverResponse, res.body))
      {
         parent_.LogWarning("{} - JSON Parse Error: {}",
                           __func__, glz::format_error(ec, res.body));
         return std::nullopt;
      }

      TautulliHistoryItems history;
      history.items.reserve(serverResponse.response.data.data.size());

      auto watchedPercent = GetWatchedPercent();
      for (auto& item : serverResponse.response.data.data)
      {
         if (item.date < epochHistoryTime) continue;

         history.items.emplace_back(TautulliHistoryItem{
             .name = std::move(item.title),
             .fullName = std::move(item.full_title),
             .id = std::format("{}", item.rating_key),
             .watched = item.percent_complete >= watchedPercent,
             .timeWatchedEpoch = item.stopped,
             .playbackPercentage = item.percent_complete,
             .live = item.live != 0
         });
      }

      return history.items.size() > 0 ? std::make_optional(history) : std::nullopt;
   }

   std::optional<TautulliHistoryItems> TautulliApi::GetWatchHistoryForUser(std::string_view user,
                                                                           std::string_view dateForHistory,
                                                                           int64_t epochHistoryTime)
   {
      return pimpl_->GetWatchHistory(user, epochHistoryTime, {
         {AFTER, dateForHistory}
      });
   }

   std::optional<TautulliHistoryItems> TautulliApi::GetWatchHistoryForUserAndLibrary(std::string_view user,
                                                                                     std::string_view libraryId,
                                                                                     std::string_view dateForHistory,
                                                                                     int64_t epochHistoryTime)
   {
      return pimpl_->GetWatchHistory(user, epochHistoryTime, {
         {AFTER, dateForHistory},
         {SECTION_ID, libraryId}
      });
   }

   bool TautulliApi::TautulliApiImpl::RefreshMonitoringData()
   {
      parent_.LogTrace("Updating Monitoring Data");

      auto apiPath = parent_.BuildApiParamsPath("", {
         GetCmdParam(CMD_GET_SETTINGS),
          {"key", "Monitoring"},
      });

      auto res = parent_.Get(apiPath, headers_);
      if (!parent_.IsHttpSuccess(__func__, res, false)) return false;

      JsonTautulliResponse<JsonTautulliMonitorInfo> serverResponse;
      if (auto ec = glz::read < glz::opts{.error_on_unknown_keys = false} > (serverResponse, res.body))
      {
         parent_.LogWarning("{} - JSON Parse Error: {}",
                           __func__, glz::format_error(ec, res.body));
         return false;
      }

      std::unique_lock lock(dataLock_);
      watchedPercent_ = serverResponse.response.data.movie_watched_percent;
      return true;
   }

   void TautulliApi::TautulliApiImpl::RefreshUserData()
   {
      auto res = parent_.Get(parent_.BuildApiParamsPath("", {GetCmdParam(CMD_GET_USERS)}), headers_);
      if (!parent_.IsHttpSuccess(__func__, res)) return;

      JsonTautulliResponse<std::vector<JsonUserInfo>> serverResponse;
      if (auto ec = glz::read < glz::opts{.error_on_unknown_keys = false} > (serverResponse, res.body))
      {
         parent_.LogWarning("{} - JSON Parse Error: {}",
                            __func__, glz::format_error(ec, res.body));
         return;
      }

      workingUsers_.reserve(serverResponse.response.data.size());
      std::ranges::for_each(serverResponse.response.data, [&](auto& user) {
         workingUsers_.emplace(std::move(user.username), TautulliUserInfo{
            .id = user.user_id,
            .friendlyName = std::move(user.friendly_name)
         });
      });

      if (!workingUsers_.empty())
      {
         std::unique_lock lock(dataLock_);
         std::swap(workingUsers_, users_);
         workingUsers_.clear();
      }
      else
      {
         parent_.LogWarning("{} - Keeping stale user data due to fetch failures", __func__);
      }
   }

   bool TautulliApi::TautulliApiImpl::GetWatchedPercentValid() const
   {
      std::shared_lock lock(dataLock_);
      return watchedPercent_.has_value();
   }

   bool TautulliApi::TautulliApiImpl::GetUserMapEmpty() const
   {
      std::shared_lock lock(dataLock_);
      return users_.empty();
   }

   void TautulliApi::TautulliApiImpl::RefreshCache(bool forceRefresh)
   {
      if (forceRefresh || !GetWatchedPercentValid()) RefreshMonitoringData();
      if (forceRefresh || GetUserMapEmpty()) RefreshUserData();
   }
}