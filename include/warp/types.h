#pragma once

#include <functional>
#include <string>

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

   struct Task
   {
      bool service{false};
      std::string name;
      std::string cronExpression;
      std::function<void()> func;
   };
}