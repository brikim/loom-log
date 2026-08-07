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

   struct JsonTracearrHistoryUser
   {
      std::string userName;

      struct glaze
      {
         // Glaze knows how to handle chrono types automatically
         static constexpr auto value = glz::object(
            "username", &JsonTracearrHistoryUser::userName
         );
      };
   };

   struct JsonTracearrHistoryItem
   {
      std::string serverName;
      std::string state;
      std::string mediaTitle;
      std::string mediaType;
      std::optional<std::string> showTitle;
      std::optional<int> seasonNumber;
      std::optional<int> episodeNumber;
      std::string progressMs;
      std::optional<std::string> totalDurationMs;
      double percentComplete{0.0};
      std::string startedAt;
      std::optional<std::string> stoppedAt;
      bool watched{false};
      std::string serverRatingKey;
      JsonTracearrHistoryUser user;

      struct glaze
      {
         // Glaze knows how to handle chrono types automatically
         static constexpr auto value = glz::object(
            "server_name", &JsonTracearrHistoryItem::serverName,
            "state", &JsonTracearrHistoryItem::state,
            "media_title", &JsonTracearrHistoryItem::mediaTitle,
            "media_type", &JsonTracearrHistoryItem::mediaType,
            "show_title", &JsonTracearrHistoryItem::showTitle,
            "season_number", &JsonTracearrHistoryItem::seasonNumber,
            "episode_number", &JsonTracearrHistoryItem::episodeNumber,
            "progress_ms", &JsonTracearrHistoryItem::progressMs,
            "total_duration_ms", &JsonTracearrHistoryItem::totalDurationMs,
            "percent_complete", &JsonTracearrHistoryItem::percentComplete,
            "started_at", &JsonTracearrHistoryItem::startedAt,
            "stopped_at", &JsonTracearrHistoryItem::stoppedAt,
            "watched", &JsonTracearrHistoryItem::watched,
            "rating_key", &JsonTracearrHistoryItem::serverRatingKey,
            "user", &JsonTracearrHistoryItem::user
         );
      };
   };

   struct JsonTracearrHistoryItems
   {
      std::vector<JsonTracearrHistoryItem> items;

      struct glaze
      {
         // Glaze knows how to handle chrono types automatically
         static constexpr auto value = glz::object(
            "data", &JsonTracearrHistoryItems::items
         );
      };
   };
}