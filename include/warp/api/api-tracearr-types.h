#pragma once

#include <optional>
#include <string>
#include <vector>

namespace warp
{
   enum class TracearrServerType
   {
      PLEX,
      EMBY,
      JELLYFIN
   };

   struct TracearrHistoryItem
   {
      std::string id;
      std::string serverName;
      TracearrServerType serverType;
      std::string fullName;
      std::string mediaTitle;
      std::string mediaType;
      std::optional<std::string> showTitle;
      std::optional<int32_t> seasonNumber;
      std::optional<int32_t> episodeNumber;
      int32_t playbackPercentage{0};
      std::string watchTime;
      bool watched{false};
      std::string serverRatingKey;
      std::string user;
   };

   struct TracearrHistoryItems
   {
      std::vector<TracearrHistoryItem> items;
   };
}