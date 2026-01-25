#include "warp/api/api-base.h"

#include "api/api-utils.h"
#include "warp/log/log-utils.h"
#include "warp/types.h"

#include <httplib.h>

#include <format>
#include <string_view>

namespace warp
{
   namespace
   {
      constexpr int32_t CRON_QUICK_CHECK_SECOND_START{5};
      constexpr int32_t CRON_QUICK_CHECK_SECOND_INCREMENT{5};
      constexpr int32_t CRON_QUICK_CHECK_MINUTES{5};

      constexpr int32_t CRON_FULL_CHECK_MINUTE_START{31};
      constexpr int32_t CRON_FULL_CHECK_MINUTE_INCREMENT{2};
      constexpr int32_t CRON_FULL_CHECK_HOUR{3};
   }

   class ApiBaseImpl
   {
   public:
      std::string name_;
      std::string prettyName_;
      std::string url_;
      std::string apiKey_;
      httplib::Client client_;

      ApiBaseImpl(const ApiBaseData& data)
         : name_(data.name)
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

      Error ConvertError(httplib::Error httpError) const
      {
         switch (httpError)
         {
            case httplib::Error::Success:
               return Error::Success;
            case httplib::Error::Unknown:
               return Error::Unknown;
            case httplib::Error::Connection:
               return Error::Connection;
            case httplib::Error::BindIPAddress:
               return Error::BindIPAddress;
            case httplib::Error::Read:
               return Error::Read;
            case httplib::Error::Write:
               return Error::Write;
            case httplib::Error::ExceedRedirectCount:
               return Error::ExceedRedirectCount;
            case httplib::Error::Canceled:
               return Error::Canceled;
            case httplib::Error::SSLConnection:
               return Error::SSLConnection;
            case httplib::Error::SSLLoadingCerts:
               return Error::SSLLoadingCerts;
            case httplib::Error::SSLServerVerification:
               return Error::SSLServerVerification;
            case httplib::Error::SSLServerHostnameVerification:
               return Error::SSLServerHostnameVerification;
            case httplib::Error::UnsupportedMultipartBoundaryChars:
               return Error::UnsupportedMultipartBoundaryChars;
            case httplib::Error::Compression:
               return Error::Compression;
            case httplib::Error::ConnectionTimeout:
               return Error::ConnectionTimeout;
            case httplib::Error::ProxyConnection:
               return Error::ProxyConnection;
            case httplib::Error::ConnectionClosed:
               return Error::ConnectionClosed;
            case httplib::Error::Timeout:
               return Error::Timeout;
            case httplib::Error::ResourceExhaustion:
               return Error::ResourceExhaustion;
            case httplib::Error::TooManyFormDataFiles:
               return Error::TooManyFormDataFiles;
            case httplib::Error::ExceedMaxPayloadSize:
               return Error::ExceedMaxPayloadSize;
            case httplib::Error::ExceedUriMaxLength:
               return Error::ExceedUriMaxLength;
            case httplib::Error::ExceedMaxSocketDescriptorCount:
               return Error::ExceedMaxSocketDescriptorCount;
            case httplib::Error::InvalidRequestLine:
               return Error::InvalidRequestLine;
            case httplib::Error::InvalidHTTPMethod:
               return Error::InvalidHTTPMethod;
            case httplib::Error::InvalidHTTPVersion:
               return Error::InvalidHTTPVersion;
            case httplib::Error::InvalidHeaders:
               return Error::InvalidHeaders;
            case httplib::Error::MultipartParsing:
               return Error::MultipartParsing;
            case httplib::Error::OpenFile:
               return Error::OpenFile;
            case httplib::Error::Listen:
               return Error::Listen;
            case httplib::Error::GetSockName:
               return Error::GetSockName;
            case httplib::Error::UnsupportedAddressFamily:
               return Error::UnsupportedAddressFamily;
            case httplib::Error::HTTPParsing:
               return Error::HTTPParsing;
            case httplib::Error::InvalidRangeHeader:
               return Error::InvalidRangeHeader;
            case httplib::Error::SSLPeerCouldBeClosed_:
               return Error::SSLPeerCouldBeClosed_;
            default:
               return Error::Unknown;
         }
      }

      std::string ErrorToString(const Error error) const
      {
         switch (error)
         {
            case Error::Success: return "Success (no error)";
            case Error::Unknown: return "Unknown";
            case Error::Connection: return "Could not establish connection";
            case Error::BindIPAddress: return "Failed to bind IP address";
            case Error::Read: return "Failed to read connection";
            case Error::Write: return "Failed to write connection";
            case Error::ExceedRedirectCount: return "Maximum redirect count exceeded";
            case Error::Canceled: return "Connection handling canceled";
            case Error::SSLConnection: return "SSL connection failed";
            case Error::SSLLoadingCerts: return "SSL certificate loading failed";
            case Error::SSLServerVerification: return "SSL server verification failed";
            case Error::SSLServerHostnameVerification:
               return "SSL server hostname verification failed";
            case Error::UnsupportedMultipartBoundaryChars:
               return "Unsupported HTTP multipart boundary characters";
            case Error::Compression: return "Compression failed";
            case Error::ConnectionTimeout: return "Connection timed out";
            case Error::ProxyConnection: return "Proxy connection failed";
            case Error::ConnectionClosed: return "Connection closed by server";
            case Error::Timeout: return "Read timeout";
            case Error::ResourceExhaustion: return "Resource exhaustion";
            case Error::TooManyFormDataFiles: return "Too many form data files";
            case Error::ExceedMaxPayloadSize: return "Exceeded maximum payload size";
            case Error::ExceedUriMaxLength: return "Exceeded maximum URI length";
            case Error::ExceedMaxSocketDescriptorCount:
               return "Exceeded maximum socket descriptor count";
            case Error::InvalidRequestLine: return "Invalid request line";
            case Error::InvalidHTTPMethod: return "Invalid HTTP method";
            case Error::InvalidHTTPVersion: return "Invalid HTTP version";
            case Error::InvalidHeaders: return "Invalid headers";
            case Error::MultipartParsing: return "Multipart parsing failed";
            case Error::OpenFile: return "Failed to open file";
            case Error::Listen: return "Failed to listen on socket";
            case Error::GetSockName: return "Failed to get socket name";
            case Error::UnsupportedAddressFamily: return "Unsupported address family";
            case Error::HTTPParsing: return "HTTP parsing failed";
            case Error::InvalidRangeHeader: return "Invalid Range header";
            default: return "Error condition Unknown";
         }

         return "Invalid";
      }

      httplib::Headers GetHttpLibHeaders(const Headers& headers)
      {
         httplib::Headers httpHeaders;
         for (const auto& header : headers)
         {
            httpHeaders.emplace(header.first, header.second);
         }
         return httpHeaders;
      }

      Response GetInvalidResponse(const httplib::Result& res) const
      {
         return Response{
             .status = VALID_HTTP_RESPONSE_MAX,
             .reason = "HTTP returned no result",
             .body = "",
             .error = ConvertError(res.error())
         };
      }
   };

   ApiBase::ApiBase(const ApiBaseData& data)
      : Base(data.className, data.ansiiCode, data.name)
      , pimpl_(std::make_unique<ApiBaseImpl>(data))
   {
   }

   ApiBase::~ApiBase() = default;

   std::optional<std::vector<Task>> ApiBase::GetTaskList()
   {
      return std::nullopt;
   }

   const std::string& ApiBase::GetName() const
   {
      return pimpl_->name_;
   }

   const std::string& ApiBase::GetPrettyName() const
   {
      return pimpl_->prettyName_;
   }

   const std::string& ApiBase::GetUrl() const
   {
      return pimpl_->url_;
   }

   const std::string& ApiBase::GetApiKey() const
   {
      return pimpl_->apiKey_;
   }

   std::string ApiBase::GetNextCronQuickTime() const
   {
      // Start at the 5 second mark
      static auto seconds{CRON_QUICK_CHECK_SECOND_START};

      auto useSeconds = seconds;
      seconds += CRON_QUICK_CHECK_SECOND_INCREMENT;

      return std::format("{} */{} * * * *", useSeconds, CRON_QUICK_CHECK_MINUTES);
   }

   std::string ApiBase::GetNextCronFullTime() const
   {
      static auto minutes{CRON_FULL_CHECK_MINUTE_START};

      auto minutesToUse = minutes;
      minutes += CRON_FULL_CHECK_MINUTE_INCREMENT;

      return std::format("0 {} {} * * *", minutesToUse, CRON_FULL_CHECK_HOUR);
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

   bool ApiBase::IsHttpSuccess(std::string_view name, const Response& response, bool log)
   {
      std::string error;

      if (response.error != Error::Success)
      {
         error = pimpl_->ErrorToString(response.error);
      }
      else if (response.status >= VALID_HTTP_RESPONSE_MAX)
      {
         error = std::format("Status {}: {} - {}", response.status, response.reason, response.body);
      }
      else
      {
         // Everything passed
         return true;
      }

      if (log) LogWarning("{} - HTTP error {}", name, warp::GetTag("error", error));
      return false;
   }

   Response ApiBase::Get(const std::string& path, const Headers& headers)
   {
      auto res = pimpl_->client_.Get(path, pimpl_->GetHttpLibHeaders(headers));

      if (!res) return pimpl_->GetInvalidResponse(res);

      return Response{
         .status = res->status,
         .reason = std::move(res->reason),
         .body = std::move(res->body),
         .error = pimpl_->ConvertError(res.error())
      };
   }

   Response ApiBase::Post(const std::string& path, const Headers& headers)
   {
      auto res = pimpl_->client_.Post(path, pimpl_->GetHttpLibHeaders(headers));

      if (!res) return pimpl_->GetInvalidResponse(res);

      return Response{
         .status = res->status,
         .reason = std::move(res->reason),
         .body = std::move(res->body),
         .error = pimpl_->ConvertError(res.error())
      };
   }

   Response ApiBase::Post(const std::string& path, const Headers& headers, const std::string& body, const std::string& contentType)
   {
      auto res = pimpl_->client_.Post(path, pimpl_->GetHttpLibHeaders(headers), body, contentType);

      if (!res) return pimpl_->GetInvalidResponse(res);

      return Response{
         .status = res->status,
         .reason = std::move(res->reason),
         .body = std::move(res->body),
         .error = pimpl_->ConvertError(res.error())
      };
   }
}