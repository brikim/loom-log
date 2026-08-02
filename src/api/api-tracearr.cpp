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
      constexpr std::string_view API_BASE{"/api/v1/public"};
      constexpr std::string_view API_GET_HEALTH{"/health"};
      constexpr std::string_view API_GET_HISTORY{"/history"};
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

   std::string_view TracearrApi::GetApiBase() const
   {
      return API_BASE;
   }

   std::string_view TracearrApi::GetApiTokenName() const
   {
      // Jellystat api key is sent via header
      return "";
   }

   bool TracearrApi::GetValid()
   {
      auto res = Get(BuildApiPath(API_GET_HEALTH), pimpl_->headers_);
      return res.error == Error::Success && res.status < VALID_HTTP_RESPONSE_MAX;
   }

   std::optional<std::string> TracearrApi::GetServerReportedName()
   {
      auto res = Get(BuildApiPath(API_GET_HEALTH), pimpl_->headers_);
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

   void TracearrApi::TracearrApiImpl::RefreshServerData()
   {
      auto res = parent_.Get(parent_.BuildApiPath(API_GET_HEALTH), headers_);
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
      for (const auto& server : serverResponse.servers)
      {
         auto iter = std::ranges::find_if(configServers_, [&server](const auto& s) {
            return s.tracearrServerName == server.name;
         });
         if (iter != configServers_.end())
         {
            ApiType apiType;
            if (server.type == "plex")
               apiType = ApiType::PLEX;
            else if (server.type == "emby")
               apiType = ApiType::EMBY;
            else
               continue;

            std::unique_lock lock(dataLock_);
            servers_.emplace_back(ServerData{
               .apiType = apiType,
               .tracearrServerName = server.name,
               .serverName = iter->serverName
            });
         }
      }
   }

   void TracearrApi::TracearrApiImpl::RefreshCache(bool forceRefresh)
   {
      if (forceRefresh || !GetServersValid())
         RefreshServerData();
   }
}