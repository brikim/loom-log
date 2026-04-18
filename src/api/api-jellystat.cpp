#include "warp/api/api-jellystat.h"

#include "api/api-jellystat-json-types.h"
#include "api/api-utils.h"
#include "warp/log/log-utils.h"
#include "warp/utils.h"

#include <glaze/glaze.hpp>

#include <format>

namespace warp
{
   namespace
   {
      constexpr std::string_view API_BASE{"/api"};
      constexpr std::string_view API_GET_CONFIG{"/getconfig"};
      constexpr std::string_view API_GET_USER_HISTORY{"/getUserHistory"};
      constexpr std::string_view API_GET_LIBRARY_HISTORY{"/getLibraryHistory"};
   }

   struct JellystatApi::JellystatApiImpl
   {
      JellystatApi& parent_;
      Headers headers_;

      JellystatApiImpl(JellystatApi& p, std::string_view appName, std::string_view version);

      std::optional<JellystatHistoryItems> GetWatchHistory(std::string_view apiPath, std::string_view payloadKey, std::string_view id);
   };

   JellystatApi::JellystatApiImpl::JellystatApiImpl(JellystatApi& p, std::string_view appName, std::string_view version)
      : parent_(p)
   {
      headers_ = {
         {"x-api-token", parent_.GetApiKey()},
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
      auto res = Get(BuildApiPath(API_GET_CONFIG), pimpl_->headers_);
      return res.error == Error::Success && res.status < VALID_HTTP_RESPONSE_MAX;
   }

   std::optional<std::string> JellystatApi::GetServerReportedName()
   {
      // Jellystat api does not support server name
      return std::nullopt;
   }

   std::optional<JellystatHistoryItems> JellystatApi::JellystatApiImpl::GetWatchHistory(std::string_view apiPath, std::string_view payloadKey, std::string_view id)
   {
      auto payload = ParamsToJson({{ payloadKey, id }});
      auto res = parent_.Post(parent_.BuildApiPath(apiPath), headers_, payload, APPLICATION_JSON);
      if (!parent_.IsHttpSuccess(__func__, res))
         return std::nullopt;

      JsonJellystatHistoryItems serverResponse;
      if (auto ec = glz::read < glz::opts{.error_on_unknown_keys = false} > (serverResponse, res.body))
      {
         parent_.LogWarning("{} - Api: {} JSON Parse Error: {}", __func__, apiPath, glz::format_error(ec, res.body));
         return std::nullopt;
      }

      // If no items received, return nullopt to signify no history instead of an empty list
      if (serverResponse.items.empty())
         return std::nullopt;

      JellystatHistoryItems returnResponse;
      returnResponse.items.reserve(serverResponse.items.size());
      for (auto& item : serverResponse.items)
         returnResponse.items.emplace_back(JellystatHistoryItem{
            .name = std::move(item.name),
            .id = std::move(item.id),
            .user = std::move(item.user),
            .watchTime = std::move(item.watchTime),
            .seriesName = std::move(item.seriesName),
            .episodeId = std::move(item.episodeId)
         });

      return returnResponse;
   }

   std::optional<JellystatHistoryItems> JellystatApi::GetWatchHistoryForUser(std::string_view userId)
   {
      return pimpl_->GetWatchHistory(API_GET_USER_HISTORY, "userid", userId);
   }

   std::optional<JellystatHistoryItems> JellystatApi::GetWatchHistoryForLibrary(std::string_view libraryId)
   {
      return pimpl_->GetWatchHistory(API_GET_LIBRARY_HISTORY, "libraryid", libraryId);
   }
}