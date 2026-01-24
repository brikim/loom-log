#include "warp/api/api-jellystat.h"

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

      const std::string APPLICATION_JSON{"application/json"};
   }

   JellystatApi::JellystatApi(std::string_view version, const ServerConfig& serverConfig)
      : ApiBase(ApiBaseData{.name = serverConfig.server_name,
            .url = serverConfig.tracker_url,
            .apiKey = serverConfig.tracker_api_key,
            .className = "JellystatApi",
            .ansiiCode = warp::ANSI_CODE_JELLYSTAT,
            .prettyName = warp::GetServerName(warp::GetFormattedJellystat(), serverConfig.server_name)})
   {
      headers_ = {
         {"x-api-token", GetApiKey()},
         {"Content-Type", APPLICATION_JSON},
         {"User-Agent", std::format("Loomis/{}", version)}
      };
   }

   std::string_view JellystatApi::GetApiBase() const
   {
      return API_BASE;
   }

   std::string_view JellystatApi::GetApiTokenName() const
   {
      // Jellystat api key is sent via header
      return "";
   }

   std::string JellystatApi::ParamsToJson(const std::list<std::pair<std::string_view, std::string_view>> params)
   {
      // Copy into a standard map so Glaze recognizes the structure
      std::map<std::string_view, std::string_view> m;
      for (const auto& [key, value] : params)
      {
         m[key] = value;
      }

      return glz::write_json(m).value_or("{}");
   }

   bool JellystatApi::GetValid()
   {
      auto res = GetClient().Get(BuildApiPath(API_GET_CONFIG), headers_);
      return res.error() == httplib::Error::Success && res.value().status < VALID_HTTP_RESPONSE_MAX;
   }

   std::optional<std::string> JellystatApi::GetServerReportedName()
   {
      // Jellystat api does not support server name
      return std::nullopt;
   }

   std::optional<JellystatHistoryItems> JellystatApi::GetWatchHistoryForUser(std::string_view userId)
   {
      auto payload = ParamsToJson({{ "userid", userId }});
      auto res = GetClient().Post(BuildApiPath(API_GET_USER_HISTORY), headers_, payload, APPLICATION_JSON);
      if (!IsHttpSuccess(__func__, res)) return std::nullopt;

      JellystatHistoryItems serverResponse;
      if (auto ec = glz::read < glz::opts{.error_on_unknown_keys = false} > (serverResponse, res.value().body))
      {
         LogWarning("{} - JSON Parse Error: {}",
                    __func__, glz::format_error(ec, res.value().body));
         return std::nullopt;
      }

      return serverResponse;
   }
}