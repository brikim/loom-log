#include "warp/api/api-tracearr.h"

#include "api/api-tracearr-json-types.h"
#include "api/api-utils.h"
#include "warp/log/log-utils.h"
#include "warp/utils.h"

#include <glaze/glaze.hpp>

#include <format>

namespace warp
{
   namespace
   {
      constexpr std::string_view API_BASE{"/api/v1/public"};
      constexpr std::string_view API_GET_HEALTH{"/health"};
      constexpr std::string_view API_GET_HISTORY{"/history"};
   }

   struct TracearrApi::TracearrApiImpl
   {
      TracearrApi& parent_;
      Headers headers_;

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
}