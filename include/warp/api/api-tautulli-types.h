#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace warp
{
   struct TautulliUserInfo
   {
      int32_t id{0};
      std::string friendlyName;
   };

   // data representing an Tautulli History item
   struct TautulliHistoryItem
   {
      std::string name;
      std::string fullName;
      std::string id;
      bool watched{false};
      int64_t timeWatchedEpoch{0};
      int32_t playbackPercentage{0};
      bool live{false};
   };

   // data representing an Tautulli History items
   struct TautulliHistoryItems
   {
      std::vector<TautulliHistoryItem> items;
   };
}