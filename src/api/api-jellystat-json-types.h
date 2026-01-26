#pragma once

#include <glaze/core/common.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace warp
{
   struct JsonJellystatHistoryItem
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
            "NowPlayingItemName", &JsonJellystatHistoryItem::name,
            "NowPlayingItemId", &JsonJellystatHistoryItem::id,
            "UserName", &JsonJellystatHistoryItem::user,
            "ActivityDateInserted", &JsonJellystatHistoryItem::watchTime,
            "SeriesName", &JsonJellystatHistoryItem::seriesName,
            "EpisodeId", &JsonJellystatHistoryItem::episodeId
         );
      };
   };

   struct JsonJellystatHistoryItems
   {
      std::vector<JsonJellystatHistoryItem> items;

      struct glaze
      {
         // Glaze knows how to handle chrono types automatically
         static constexpr auto value = glz::object(
            "results", &JsonJellystatHistoryItems::items
         );
      };
   };
}