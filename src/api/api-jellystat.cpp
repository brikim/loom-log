#include "warp/api/api-jellystat.h"

#include "api/api-jellystat-json-types.h"
#include "api/api-utils.h"
#include "warp/log/log-utils.h"
#include "warp/utils.h"

#include <glaze/glaze.hpp>

#include <format>
#include <ranges>

namespace warp
{
   namespace
   {
      const std::string API_BASE{"/api"};
      const std::string API_GET_CONFIG{"/getconfig"};
      const std::string API_GET_USER_HISTORY{"/getUserHistory"};
      const std::string API_GET_LIBRARY_HISTORY{"/getLibraryHistory"};

      const std::string APPLICATION_JSON{"application/json"};
   }

   struct JellystatApi::JellystatApiImpl
   {
      JellystatApi& parent;
      Headers headers;

      JellystatApiImpl(JellystatApi& p, std::string_view appName, std::string_view version);
   };

   JellystatApi::JellystatApiImpl::JellystatApiImpl(JellystatApi& p, std::string_view appName, std::string_view version)
      : parent(p)
   {
      headers = {
         {"x-api-token", parent.GetApiKey()},
         {"Content-Type", APPLICATION_JSON},
         {"User-Agent", std::format("{}/{}", appName, version)}
      };
   }

   JellystatApi::JellystatApi(std::string_view appName, std::string_view version, const ServerConfig& serverConfig)
      : ApiBase(ApiBaseData{.name = serverConfig.serverName,
            .url = serverConfig.trackerUrl,
            .apiKey = serverConfig.trackerApiKey,
            .className = "JellystatApi",
            .ansiiCode = ANSI_CODE_JELLYSTAT,
            .prettyName = GetServerName(GetFormattedJellystat(), serverConfig.serverName)})
      , pimpl_(std::make_unique<JellystatApiImpl>(*this, appName, version))
   {
   }

   JellystatApi::~JellystatApi() = default;

   std::string_view JellystatApi::GetApiBase() const
   {
      return API_BASE;
   }

   std::string_view JellystatApi::GetApiTokenName() const
   {
      // Jellystat api key is sent via header
      return "";
   }

   bool JellystatApi::GetValid()
   {
      auto res = Get(BuildApiPath(API_GET_CONFIG), pimpl_->headers);
      return res.error == Error::Success && res.status < VALID_HTTP_RESPONSE_MAX;
   }

   std::optional<std::string> JellystatApi::GetServerReportedName()
   {
      // Jellystat api does not support server name
      return std::nullopt;
   }

   std::optional<JellystatHistoryItems> JellystatApi::GetWatchHistoryForUser(std::string_view userId)
   {
      auto payload = ParamsToJson({{ "userid", userId }});
      auto res = Post(BuildApiPath(API_GET_USER_HISTORY), pimpl_->headers, payload, APPLICATION_JSON);
      if (!IsHttpSuccess(__func__, res)) return std::nullopt;

      JsonJellystatHistoryItems serverResponse;
      if (auto ec = glz::read < glz::opts{.error_on_unknown_keys = false} > (serverResponse, res.body))
      {
         LogWarning("{} - JSON Parse Error: {}",
                    __func__, glz::format_error(ec, res.body));
         return std::nullopt;
      }

      JellystatHistoryItems returnResponse;
      for (auto& item : serverResponse.items)
      {
         returnResponse.items.emplace_back(JellystatHistoryItem{
            .name = std::move(item.name),
            .id = std::move(item.id),
            .user = std::move(item.user),
            .watchTime = std::move(item.watchTime),
            .seriesName = std::move(item.seriesName),
            .episodeId = std::move(item.episodeId)
         });
      }
      return returnResponse;
   }

   std::optional<JellystatHistoryItems> JellystatApi::GetWatchHistoryForLibrary(std::string_view libraryId)
   {
      auto payload = ParamsToJson({{ "libraryid", libraryId }});
      auto res = Post(BuildApiPath(API_GET_LIBRARY_HISTORY), pimpl_->headers, payload, APPLICATION_JSON);
      if (!IsHttpSuccess(__func__, res)) return std::nullopt;

      JsonJellystatHistoryItems serverResponse;
      if (auto ec = glz::read < glz::opts{.error_on_unknown_keys = false} > (serverResponse, res.body))
      {
         LogWarning("{} - JSON Parse Error: {}",
                    __func__, glz::format_error(ec, res.body));
         return std::nullopt;
      }

      JellystatHistoryItems returnResponse;
      for (auto& item : serverResponse.items)
      {
         returnResponse.items.emplace_back(JellystatHistoryItem{
            .name = std::move(item.name),
            .id = std::move(item.id),
            .user = std::move(item.user),
            .watchTime = std::move(item.watchTime),
            .seriesName = std::move(item.seriesName),
            .episodeId = std::move(item.episodeId)
         });
      }
      return returnResponse;
   }
}