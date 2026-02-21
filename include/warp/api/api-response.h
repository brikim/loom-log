#pragma once

#include "warp/types.h"

#include <cstdint>
#include <string>
#include <unordered_map>

namespace warp
{
   enum class Error
   {
      Success,
      Unknown,
      Connection,
      BindIPAddress,
      Read,
      Write,
      ExceedRedirectCount,
      Canceled,
      SSLConnection,
      SSLLoadingCerts,
      SSLServerVerification,
      SSLServerHostnameVerification,
      UnsupportedMultipartBoundaryChars,
      Compression,
      ConnectionTimeout,
      ProxyConnection,
      ConnectionClosed,
      Timeout,
      ResourceExhaustion,
      TooManyFormDataFiles,
      ExceedMaxPayloadSize,
      ExceedUriMaxLength,
      ExceedMaxSocketDescriptorCount,
      InvalidRequestLine,
      InvalidHTTPMethod,
      InvalidHTTPVersion,
      InvalidHeaders,
      MultipartParsing,
      OpenFile,
      Listen,
      GetSockName,
      UnsupportedAddressFamily,
      HTTPParsing,
      InvalidRangeHeader,
      SSLPeerCouldBeClosed_,
   };

   using Headers =
      std::unordered_multimap<std::string, std::string, StringHash, std::equal_to<>>;

   struct Response
   {
      int32_t status;
      std::string reason;
      std::string body;
      Error error;
   };
}