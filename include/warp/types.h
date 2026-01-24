#pragma once

namespace warp
{
   static constexpr int VALID_HTTP_RESPONSE_MAX{300};

   enum class ApiType
   {
      PLEX,
      EMBY,
      TAUTULLI,
      JELLYSTAT
   };
}