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

      TracearrApiImpl(TracearrApi& p, std::string_view appName, std::string_view version);
   };

   TracearrApi::TracearrApiImpl::TracearrApiImpl(TracearrApi& p, std::string_view appName, std::string_view version)
      : parent_(p)
   {
      headers_ = {
         {"Authorization", std::format("Bearer {}", parent_.GetApiKey())},
         {"Content-Type", APPLICATION_JSON},
         {"User-Agent", std::format("{}/{}", appName, version)}
      };
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
         if (!isValidMedia || !isStopped || !item.percentComplete.has_value())
         {
            continue;
         }

         static constexpr auto parseServerType = [](const std::string_view type) -> std::optional<TracearrServerType> {
            if (type == "plex") return TracearrServerType::PLEX;
            if (type == "emby") return TracearrServerType::EMBY;
            if (type == "jellyfin") return TracearrServerType::JELLYFIN;
            return std::nullopt;
         };
         auto serverType = parseServerType(item.serverType);
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
            .serverName = std::move(item.serverName),
            .serverType = serverType.value(),
            .fullName = std::move(fullName),
            .mediaTitle = std::move(item.mediaTitle),
            .mediaType = std::move(item.mediaType),
            .showTitle = std::move(item.showTitle),
            .seasonNumber = item.seasonNumber,
            .episodeNumber = item.episodeNumber,
            .playbackPercentage = static_cast<int32_t>(std::lround(item.percentComplete.value())),
            .watchTime = item.stoppedAt.has_value() ? std::move(item.stoppedAt.value()) : std::move(item.startedAt),
            .watched = item.watched,
            .serverRatingKey = std::move(item.serverRatingKey),
            .user = std::move(item.user.userName)
         });
      }

      return returnResponse;
   }
}