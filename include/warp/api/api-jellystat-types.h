#pragma once

#include <glaze/glaze.hpp>

#include <chrono>
#include <cstdint>
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

      std::string GetFullName() const
      {
         return seriesName ? (*seriesName + " - " + name) : name;
      }

      struct glaze
      {
         // Glaze knows how to handle chrono types automatically
         static constexpr auto value = glz::object(
            "NowPlayingItemName", &JellystatHistoryItem::name,
            "NowPlayingItemId", &JellystatHistoryItem::id,
            "UserName", &JellystatHistoryItem::user,
            "ActivityDateInserted", &JellystatHistoryItem::watchTime,
            "SeriesName", &JellystatHistoryItem::seriesName,
            "EpisodeId", &JellystatHistoryItem::episodeId
         );
      };
   };

   struct JellystatHistoryItems
   {
      std::vector<JellystatHistoryItem> items;

      struct glaze
      {
         // Glaze knows how to handle chrono types automatically
         static constexpr auto value = glz::object(
            "results", &JellystatHistoryItems::items
         );
      };
   };
}