#pragma once

#include <string>
#include <string_view>

namespace warp
{
   struct ApiBaseData
   {
      std::string_view name;
      std::string_view url;
      std::string_view apiKey;
      std::string_view className;
      std::string_view ansiiCode;
      std::string_view prettyName;
   };

   struct ServerConfig
   {
      std::string server_name;
      std::string url;
      std::string api_key;
      std::string tracker_url;
      std::string tracker_api_key;
      std::string media_path;
   };
}