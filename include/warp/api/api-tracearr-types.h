#pragma once

#include <optional>
#include <string>
#include <vector>

namespace warp
{
   struct TracearrHistoryItem
   {
      std::string id;
      std::string serverName;
      std::string mediaTitle;
      std::string mediaType;
      std::optional<std::string> showTitle;
      std::optional<int> seasonNumber;
      std::optional<int> episodeNumber;
      int progressMs;
      std::optional<int> totalDurationMs;
      std::string startedAt;
      std::optional<std::string> stoppedAt;
      bool watched{false};
      std::string user;
   };

   struct TracearrHistoryItems
   {
      std::vector<TracearrHistoryItem> items;
   };
}