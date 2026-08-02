#pragma once

#include <glaze/core/common.hpp>

#include <string>
#include <vector>

namespace warp
{
   struct JsonHealthServers
   {
      std::string name;
      std::string type;
   };

   struct JsonHealthResponse
   {
      std::string status;
      std::string version;
      std::string timestamp;
      std::vector<JsonHealthServers> servers;
   };
}