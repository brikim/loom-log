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
      std::string startedAt;
      std::optional<std::string> stoppedAt;
      bool watched{false};
      JsonTracearrHistoryUser user;

      struct glaze
      {
         // Glaze knows how to handle chrono types automatically
         static constexpr auto value = glz::object(
            "serverName", &JsonTracearrHistoryItem::serverName,
            "state", &JsonTracearrHistoryItem::state,
            "mediaTitle", &JsonTracearrHistoryItem::mediaTitle,
            "mediaType", &JsonTracearrHistoryItem::mediaType,
            "showTitle", &JsonTracearrHistoryItem::showTitle,
            "seasonNumber", &JsonTracearrHistoryItem::seasonNumber,
            "episodeNumber", &JsonTracearrHistoryItem::episodeNumber,
            "progressMs", &JsonTracearrHistoryItem::progressMs,
            "totalDurationMs", &JsonTracearrHistoryItem::totalDurationMs,
            "startedAt", &JsonTracearrHistoryItem::startedAt,
            "stoppedAt", &JsonTracearrHistoryItem::stoppedAt,
            "watched", &JsonTracearrHistoryItem::watched,
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