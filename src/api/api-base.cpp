#include "warp/api/api-base.h"

#include "api/api-utils.h"
#include "warp/log/log-utils.h"
#include "warp/types.h"

#include <format>
#include <string_view>

namespace warp
{
   ApiBase::ApiBase(const ApiBaseData& data)
      : Base(data.className, data.ansiiCode, data.name)
      , name_(data.name)
      , prettyName_(data.prettyName)
      , url_(data.url)
      , apiKey_(data.apiKey)
      , client_(url_)
   {
      constexpr time_t timeoutSec{5};
      client_.set_connection_timeout(timeoutSec);

      constexpr time_t readWritetimeoutSec{10};
      client_.set_read_timeout(readWritetimeoutSec);
      client_.set_write_timeout(readWritetimeoutSec);
   }

   std::optional<std::vector<ApiTask>> ApiBase::GetTaskList()
   {
      return std::nullopt;
   }

   httplib::Client& ApiBase::GetClient()
   {
      return client_;
   }

   const std::string& ApiBase::GetName() const
   {
      return name_;
   }

   const std::string& ApiBase::GetPrettyName() const
   {
      return prettyName_;
   }

   const std::string& ApiBase::GetUrl() const
   {
      return url_;
   }

   const std::string& ApiBase::GetApiKey() const
   {
      return apiKey_;
   }

   void ApiBase::AddApiParam(std::string& url, const ApiParams& params) const
   {
      if (params.empty()) return;

      bool hasQuery = (url.find('?') != std::string::npos);
      bool lastIsSeparator = !url.empty() && (url.back() == '?' || url.back() == '&');
      for (const auto& [key, value] : params)
      {
         if (!lastIsSeparator)
         {
            url += hasQuery ? '&' : '?';
         }
         url += key;
         url += '=';
         url += GetPercentEncoded(value);

         hasQuery = true;
         lastIsSeparator = false;
      }
   }

   std::string ApiBase::BuildApiPath(std::string_view path) const
   {
      auto apiTokenName = GetApiTokenName();
      char separator = (path.find('?') == std::string_view::npos) ? '?' : '&';
      if (apiTokenName.empty())
      {
         return std::format("{}{}", GetApiBase(), path);
      }
      else
      {
         return std::format("{}{}{}{}={}",
                            GetApiBase(),
                            path,
                            separator,
                            apiTokenName,
                            GetPercentEncoded(GetApiKey()));
      }
   }

   std::string ApiBase::BuildApiParamsPath(std::string_view path, const ApiParams& params) const
   {
      auto apiPath = BuildApiPath(path);
      AddApiParam(apiPath, params);
      return apiPath;
   }

   bool ApiBase::IsHttpSuccess(std::string_view name, const httplib::Result& result, bool log)
   {
      std::string error;

      if (result.error() != httplib::Error::Success)
      {
         error = httplib::to_string(result.error());
      }
      else if (result->status >= VALID_HTTP_RESPONSE_MAX)
      {
         error = std::format("Status {}: {} - {}", result->status, result->reason, result->body);
      }
      else
      {
         // Everything passed
         return true;
      }

      if (log) LogWarning("{} - HTTP error {}", name, warp::GetTag("error", error));
      return false;
   }
}