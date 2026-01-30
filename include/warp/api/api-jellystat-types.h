#pragma once

#include <optional>
#include <string>
#include <vector>

namespace warp
{
   struct JellystatHistoryItem
   {
      std::string name;
      std::string id;
      std::string user;
      std::string watchTime;
      std::optional<std::string> seriesName;
      std::optional<std::string> episodeId;
   };

   struct JellystatHistoryItems
   {
      std::vector<JellystatHistoryItem> items;
   };
}