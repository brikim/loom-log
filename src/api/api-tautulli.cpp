#include "warp/api/api-tautulli.h"

#include "api/api-tautulli-json-types.h"
#include "warp/log/log-utils.h"
#include "warp/utils.h"

#include <glaze/glaze.hpp>

#include <format>
#include <ranges>

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
   }

   TautulliApi::TautulliApi(std::string_view appName, std::string_view version, const ServerConfig& serverConfig)
      : ApiBase(ApiBaseData{.name = serverConfig.server_name,
            .url = serverConfig.tracker_url,
            .apiKey = serverConfig.tracker_api_key,
            .className = "TautulliApi",
            .ansiiCode = ANSI_CODE_TAUTULLI,
            .prettyName = GetServerName(GetFormattedTautulli(), serverConfig.server_name)})
   {
      // Standardize headers
      headers_.insert({"User-Agent", std::format("{}/{}", appName, version)});
      headers_.insert({"Accept", "application/json"});
   }

   std::optional<std::vector<Task>> TautulliApi::GetTaskList()
   {
      std::vector<Task> tasks;

      auto& fullUpdate = tasks.emplace_back();
      fullUpdate.name = std::format("TautulliApi({}) - Settings Update", GetName());
      fullUpdate.cronExpression = "0 50 3 * * *";
      fullUpdate.func = [this]() {this->RunSettingsUpdate(); };

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

   std::pair<std::string_view, std::string_view> TautulliApi::GetCmdParam(std::string_view cmd) const
   {
      return {API_COMMAND, cmd};
   }

   bool TautulliApi::GetValid()
   {
      auto apiPath = BuildApiParamsPath("", {GetCmdParam(CMD_GET_SERVER_FRIENDLY_NAME)});
      auto res = GetClient().Get(apiPath, headers_);
      return res.error() == httplib::Error::Success && res.value().status < VALID_HTTP_RESPONSE_MAX;
   }

   std::optional<std::string> TautulliApi::GetServerReportedName()
   {
      auto res = GetClient().Get(BuildApiParamsPath("", {GetCmdParam(CMD_SERVER_INFO)}), headers_);

      if (!IsHttpSuccess(__func__, res))
      {
         return std::nullopt;
      }

      JsonTautulliResponse<JsonTautulliServerInfo> serverResponse;
      if (auto ec = glz::read < glz::opts{.error_on_unknown_keys = false} > (serverResponse, res.value().body))
      {
         LogWarning("{} - JSON Parse Error: {}",
                    __func__, glz::format_error(ec, res.value().body));
         return std::nullopt;
      }

      if (serverResponse.response.data.pms_name.empty())
      {
         return std::nullopt;
      }
      return std::move(serverResponse.response.data.pms_name);
   }

   std::optional<TautulliUserInfo> TautulliApi::GetUserInfo(std::string_view name)
   {
      auto res = GetClient().Get(BuildApiParamsPath("", {GetCmdParam(CMD_GET_USERS)}), headers_);
      if (!IsHttpSuccess(__func__, res)) return std::nullopt;

      JsonTautulliResponse<std::vector<JsonUserInfo>> serverResponse;
      if (auto ec = glz::read < glz::opts{.error_on_unknown_keys = false} > (serverResponse, res.value().body))
      {
         LogWarning("{} - JSON Parse Error: {}",
                    __func__, glz::format_error(ec, res.value().body));
         return std::nullopt;
      }

      auto it = std::ranges::find_if(serverResponse.response.data, [&](const auto& item) {
         return item.username == name;
      });

      if (it == serverResponse.response.data.end())
      {
         return std::nullopt;
      }

      return TautulliUserInfo{it->user_id, std::move(it->friendly_name)};
   }

   bool TautulliApi::ReadMonitoringData()
   {
      auto apiPath = BuildApiParamsPath("", {
         GetCmdParam(CMD_GET_SETTINGS),
          {"key", "Monitoring"},
      });

      auto res = GetClient().Get(apiPath, headers_);
      if (!IsHttpSuccess(__func__, res, false)) return false;

      JsonTautulliResponse<JsonTautulliMonitorInfo> serverResponse;
      if (auto ec = glz::read < glz::opts{.error_on_unknown_keys = false} > (serverResponse, res.value().body))
      {
         LogWarning("{} - JSON Parse Error: {}",
                    __func__, glz::format_error(ec, res.value().body));
         return false;
      }

      watchedPercent_ = serverResponse.response.data.movie_watched_percent;
      return true;
   }

   int32_t TautulliApi::GetWatchedPercent()
   {
      if (watchedPercent_) return *watchedPercent_;

      constexpr int32_t defaultWatchedPercent = 85;
      return ReadMonitoringData() ? *watchedPercent_ : defaultWatchedPercent;
   }

   std::optional<TautulliHistoryItems> TautulliApi::GetWatchHistory(std::string_view user,
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

      auto res = GetClient().Get(BuildApiParamsPath("", params), headers_);
      if (!IsHttpSuccess(__func__, res)) return std::nullopt;

      JsonTautulliResponse<JsonTautulliHistoryData> serverResponse;
      if (auto ec = glz::read < glz::opts{.error_on_unknown_keys = false} > (serverResponse, res.value().body))
      {
         LogWarning("{} - JSON Parse Error: {}",
                    __func__, glz::format_error(ec, res.value().body));
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
             .playbackPercentage = item.percent_complete
         });
      }

      return history.items.size() > 0 ? std::make_optional(history) : std::nullopt;
   }

   std::optional<TautulliHistoryItems> TautulliApi::GetWatchHistoryForUser(std::string_view user,
                                                                           std::string_view dateForHistory,
                                                                           int64_t epochHistoryTime)
   {
      return GetWatchHistory(user, epochHistoryTime, {
         {AFTER, dateForHistory}
      });
   }

   void TautulliApi::RunSettingsUpdate()
   {
      ReadMonitoringData();
   }
}