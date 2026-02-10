#pragma once

#include <cstdint>
#include <string>

#include <glaze/glaze.hpp>

namespace warp
{
   struct JsonTautulliMonitorInfo
   {
      int32_t movie_watched_percent{0};

      struct glaze
      {
         static constexpr auto value = glz::object(
             "movie_watched_percent", &JsonTautulliMonitorInfo::movie_watched_percent
         );
      };
   };

   struct JsonTautulliServerInfo
   {
      std::string pms_name;

      struct glaze
      {
         static constexpr auto value = glz::object(
             "pms_name", &JsonTautulliServerInfo::pms_name
         );
      };
   };

   struct JsonUserInfo
   {
      std::string username;
      int32_t user_id{0};
      std::string friendly_name;

      struct glaze
      {
         static constexpr auto value = glz::object(
             "username", &JsonUserInfo::username,
             "user_id", &JsonUserInfo::user_id,
             "friendly_name", &JsonUserInfo::friendly_name
         );
      };
   };

   struct JsonTautulliHistoryItem
   {
      std::string title;
      std::string full_title;
      int64_t date{0};
      int64_t rating_key{0};
      int64_t stopped{0};
      int32_t percent_complete{0};
      int32_t live{0};

      struct glaze
      {
         static constexpr auto value = glz::object(
             "title", &JsonTautulliHistoryItem::title,
             "full_title", &JsonTautulliHistoryItem::full_title,
             "date", &JsonTautulliHistoryItem::date,
             "rating_key", &JsonTautulliHistoryItem::rating_key,
             "stopped", &JsonTautulliHistoryItem::stopped,
             "percent_complete", &JsonTautulliHistoryItem::percent_complete,
             "live", &JsonTautulliHistoryItem::live
         );
      };
   };

   struct JsonTautulliHistoryData
   {
      std::vector<JsonTautulliHistoryItem> data;

      struct glaze
      {
         static constexpr auto value = glz::object(
             "data", &JsonTautulliHistoryData::data
         );
      };
   };

   template <typename T>
   struct JsonTautulliResponse
   {
      struct InternalData
      {
         std::string result; // "success" or "error"
         T data;              // This is the actual object/vector you want

         struct glaze
         {
            using T_int = InternalData;
            static constexpr auto value = glz::object(
               "result", &T_int::result,
               "data", &T_int::data
            );
         };
      };

      InternalData response;

      struct glaze
      {
         static constexpr auto value = glz::object(
            "response", &JsonTautulliResponse::response
         );
      };
   };
}