#pragma once

#include <glaze/core/common.hpp>

#include <string>
#include <vector>

namespace warp
{
   struct JsonHealthServers
   {
      std::string id;
      std::string name;
      std::string type;

      struct glaze
      {
         static constexpr auto value = glz::object(
            "id", &JsonHealthServers::id,
            "name", &JsonHealthServers::name,
            "type", &JsonHealthServers::type
         );
      };
   };

   struct JsonHealthResponse
   {
      std::string status;
      std::string version;
      std::string timestamp;
      std::vector<JsonHealthServers> servers;

      struct glaze
      {
         static constexpr auto value = glz::object(
            "status", &JsonHealthResponse::status,
            "version", &JsonHealthResponse::version,
            "timestamp", &JsonHealthResponse::timestamp,
            "servers", &JsonHealthResponse::servers
         );
      };
   };

   struct JsonTracearrHistoryUser
   {
      std::string id;
      std::string userName;

      struct glaze
      {
         static constexpr auto value = glz::object(
            "id", &JsonTracearrHistoryUser::id,
            "username", &JsonTracearrHistoryUser::userName
         );
      };
   };

   struct JsonTracearrHistoryItem
   {
      std::string serverId;
      std::string serverName;
      std::string serverType;
      std::string state;
      std::string mediaTitle;
      std::string mediaType;
      std::optional<std::string> showTitle;
      std::optional<int> seasonNumber;
      std::optional<int> episodeNumber;
      std::optional<double> percentComplete;
      std::string startedAt;
      std::optional<std::string> stoppedAt;
      bool watched{false};
      std::string serverRatingKey;
      JsonTracearrHistoryUser user;

      struct glaze
      {
         // Glaze knows how to handle chrono types automatically
         static constexpr auto value = glz::object(
            "server_id", &JsonTracearrHistoryItem::serverId,
            "server_name", &JsonTracearrHistoryItem::serverName,
            "server_type", &JsonTracearrHistoryItem::serverType,
            "state", &JsonTracearrHistoryItem::state,
            "media_title", &JsonTracearrHistoryItem::mediaTitle,
            "media_type", &JsonTracearrHistoryItem::mediaType,
            "show_title", &JsonTracearrHistoryItem::showTitle,
            "season_number", &JsonTracearrHistoryItem::seasonNumber,
            "episode_number", &JsonTracearrHistoryItem::episodeNumber,
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

   struct JsonTracearrUserAccount
   {
      std::string serverId;
      std::string serverType;
      std::string serverUserId;
      std::string externalUserId;
      std::string externalUserName;

      struct glaze
      {
         static constexpr auto value = glz::object(
               "server_id", &JsonTracearrUserAccount::serverId,
               "server_type", &JsonTracearrUserAccount::serverType,
               "server_user_id", &JsonTracearrUserAccount::serverUserId,
               "external_user_id", &JsonTracearrUserAccount::externalUserId,
               "username", &JsonTracearrUserAccount::externalUserName
         );
      };
   };

   struct JsonTracearrUser
   {
      std::string id;
      std::string name;
      std::vector<JsonTracearrUserAccount> accounts;

      struct glaze
      {
         static constexpr auto value = glz::object(
               "id", &JsonTracearrUser::id,
               "username", &JsonTracearrUser::name,
               "accounts", &JsonTracearrUser::accounts
         );
      };
   };

   struct JsonTracearrUsers
   {
      std::vector<JsonTracearrUser> users;

      struct glaze
      {
         static constexpr auto value = glz::object(
            "data", &JsonTracearrUsers::users
         );
      };
   };
}