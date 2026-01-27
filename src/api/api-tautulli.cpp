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

   struct TautulliApiImpl
   {
      TautulliApi& parent;
      Headers headers;
      std::optional<int32_t> watchedPercent;

      TautulliApiImpl(TautulliApi& p, std::string_view appName, std::string_view version)
         : parent(p)
      {
         headers = {
            {"User-Agent", std::format("{}/{}", appName, version)},
            {"Accept", "application/json"}
         };
      }

      [[nodiscard]] std::pair<std::string_view, std::string_view> GetCmdParam(std::string_view cmd) const
      {
         return {API_COMMAND, cmd};
      }

      [[nodiscard]] std::optional<TautulliHistoryItems> GetWatchHistory(std::string_view user,
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

         auto res = parent.Get(parent.BuildApiParamsPath("", params), headers);
         if (!parent.IsHttpSuccess(__func__, res)) return std::nullopt;

         JsonTautulliResponse<JsonTautulliHistoryData> serverResponse;
         if (auto ec = glz::read < glz::opts{.error_on_unknown_keys = false} > (serverResponse, res.body))
         {
            parent.LogWarning("{} - JSON Parse Error: {}",
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
                .playbackPercentage = item.percent_complete
            });
         }

         return history.items.size() > 0 ? std::make_optional(history) : std::nullopt;
      }

      int32_t GetWatchedPercent()
      {
         if (watchedPercent) return *watchedPercent;

         constexpr int32_t defaultWatchedPercent = 85;
         return ReadMonitoringData() ? *watchedPercent : defaultWatchedPercent;
      }

      bool ReadMonitoringData()
      {
         parent.LogTrace("Updating Monitoring Data");

         auto apiPath = parent.BuildApiParamsPath("", {
            GetCmdParam(CMD_GET_SETTINGS),
             {"key", "Monitoring"},
         });

         auto res = parent.Get(apiPath, headers);
         if (!parent.IsHttpSuccess(__func__, res, false)) return false;

         JsonTautulliResponse<JsonTautulliMonitorInfo> serverResponse;
         if (auto ec = glz::read < glz::opts{.error_on_unknown_keys = false} > (serverResponse, res.body))
         {
            parent.LogWarning("{} - JSON Parse Error: {}",
                              __func__, glz::format_error(ec, res.body));
            return false;
         }

         watchedPercent = serverResponse.response.data.movie_watched_percent;
         return true;
      }

      void RunSettingsUpdate()
      {
         ReadMonitoringData();
      }
   };

   TautulliApi::TautulliApi(std::string_view appName, std::string_view version, const ServerConfig& serverConfig)
      : ApiBase(ApiBaseData{.name = serverConfig.server_name,
            .url = serverConfig.tracker_url,
            .apiKey = serverConfig.tracker_api_key,
            .className = "TautulliApi",
            .ansiiCode = ANSI_CODE_TAUTULLI,
            .prettyName = GetServerName(GetFormattedTautulli(), serverConfig.server_name)})
      , pimpl_(std::make_unique<TautulliApiImpl>(*this, appName, version))
   {
   }

   TautulliApi::~TautulliApi() = default;

   std::optional<std::vector<Task>> TautulliApi::GetTaskList()
   {
      std::vector<Task> tasks;

      auto& fullUpdate = tasks.emplace_back();
      fullUpdate.name = std::format("TautulliApi({}) - Settings Update", GetName());
      fullUpdate.cronExpression = GetNextCronFullTime();
      fullUpdate.func = [this]() {pimpl_->RunSettingsUpdate(); };

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

   bool TautulliApi::GetValid()
   {
      auto apiPath = BuildApiParamsPath("", {pimpl_->GetCmdParam(CMD_GET_SERVER_FRIENDLY_NAME)});
      auto res = Get(apiPath, pimpl_->headers);
      return res.error == Error::Success && res.status < VALID_HTTP_RESPONSE_MAX;
   }

   std::optional<std::string> TautulliApi::GetServerReportedName()
   {
      auto res = Get(BuildApiParamsPath("", {pimpl_->GetCmdParam(CMD_SERVER_INFO)}), pimpl_->headers);
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

   std::optional<TautulliUserInfo> TautulliApi::GetUserInfo(std::string_view name)
   {
      auto res = Get(BuildApiParamsPath("", {pimpl_->GetCmdParam(CMD_GET_USERS)}), pimpl_->headers);
      if (!IsHttpSuccess(__func__, res)) return std::nullopt;

      JsonTautulliResponse<std::vector<JsonUserInfo>> serverResponse;
      if (auto ec = glz::read < glz::opts{.error_on_unknown_keys = false} > (serverResponse, res.body))
      {
         LogWarning("{} - JSON Parse Error: {}",
                    __func__, glz::format_error(ec, res.body));
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

   std::optional<TautulliHistoryItems> TautulliApi::GetWatchHistoryForUser(std::string_view user,
                                                                           std::string_view dateForHistory,
                                                                           int64_t epochHistoryTime)
   {
      return pimpl_->GetWatchHistory(user, epochHistoryTime, {
         {AFTER, dateForHistory}
      });
   }
}