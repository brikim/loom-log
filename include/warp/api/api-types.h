#pragma once

#include <filesystem>
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
      std::string serverName;
      std::string url;
      std::string apiKey;
      std::string trackerUrl;
      std::string trackerApiKey;
      std::filesystem::path mediaPath;
   };

   struct ServerPlexOptions
   {
      bool enableCacheCollection{false};
      bool enableUserTokens{false};

      // Enable Caching of Path to rating key map. This is required to look
      // up a rating key for a given path.
      bool enableCachePaths{false};
   };

   struct ServerEmbyOptions
   {
      bool enableCachePaths{false};
   };
}