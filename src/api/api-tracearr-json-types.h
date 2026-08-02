#pragma once

#include <glaze/core/common.hpp>

namespace warp
{
   struct JsonHealthResponse
   {
      std::string status;
      std::string version;
      std::string timestamp;
   };
}