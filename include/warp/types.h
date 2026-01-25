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

   struct StringHash
   {
      using is_transparent = void;
      size_t operator()(std::string_view sv) const
      {
         return std::hash<std::string_view>{}(sv);
      }
      size_t operator()(const std::string& s) const
      {
         return std::hash<std::string>{}(s);
      }
   };
}