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
      std::string fullName;
      std::string mediaTitle;
      std::string mediaType;
      std::optional<std::string> showTitle;
      std::optional<int32_t> seasonNumber;
      std::optional<int32_t> episodeNumber;
      int32_t progressMs;
      std::optional<int32_t> totalDurationMs;
      int32_t playbackPercentage{0};
      std::string startedAt;
      std::optional<std::string> stoppedAt;
      bool watched{false};
      std::string serverRatingKey;
      std::string user;
   };

   struct TracearrHistoryItems
   {
      std::vector<TracearrHistoryItem> items;
   };
}