#pragma once

#include <cstdint>
#include <string>

#include <glaze/glaze.hpp>

namespace warp
{
   // File Part Structs
   // {
   struct JsonPlexPart
   {
      std::string file;

      static constexpr auto value = glz::object(
            "file", &JsonPlexPart::file
      );
   };
   struct JsonPlexMedia
   {
      std::vector<JsonPlexPart> part;

      struct glaze
      {
         static constexpr auto value = glz::object(
            "Part", &JsonPlexMedia::part
         );
      };
   };
   // } File Part Structs

   // Search Result Structs
   // {
   struct JsonPlexSearchMetadata
   {
      // Basic Info
      std::string library;
      std::string ratingKey;
      std::string title;
      std::optional<std::string> showTitle;

      // Playback/Stats
      uint64_t duration{0};
      std::optional<int> viewCount;
      std::optional<int64_t> viewOffset;

      std::vector<JsonPlexMedia> media;

      struct glaze
      {
         static constexpr auto value = glz::object(
            "librarySectionTitle", &JsonPlexSearchMetadata::library,
            "ratingKey", &JsonPlexSearchMetadata::ratingKey,
            "title", &JsonPlexSearchMetadata::title,
            "grandparentTitle", &JsonPlexSearchMetadata::showTitle,
            "duration", &JsonPlexSearchMetadata::duration,
            "viewCount", &JsonPlexSearchMetadata::viewCount,
            "viewOffset", &JsonPlexSearchMetadata::viewOffset,
            "Media", &JsonPlexSearchMetadata::media
         );
      };
   };
   struct JsonPlexSearchHub
   {
      std::string type;
      std::vector<JsonPlexSearchMetadata> data;

      struct glaze
      {
         static constexpr auto value = glz::object(
            "type", &JsonPlexSearchHub::type,
            "Metadata", &JsonPlexSearchHub::data
         );
      };
   };
   struct JsonPlexSearchResult
   {
      std::vector<JsonPlexSearchHub> hub;

      struct glaze
      {
         static constexpr auto value = glz::object(
            "Hub", &JsonPlexSearchResult::hub
         );
      };
   };
   // } Search Result Structs

   // Collection Item Structs
   // {
   struct JsonPlexCollectionItemData
   {
      std::string title;
      std::string key;
      std::vector<JsonPlexMedia> media;

      struct glaze
      {
         static constexpr auto value = glz::object(
            "title", &JsonPlexCollectionItemData::title,
            "key", &JsonPlexCollectionItemData::key,
            "Media", &JsonPlexCollectionItemData::media
         );
      };
   };
   struct JsonPlexCollectionResult
   {
      std::vector<JsonPlexCollectionItemData> data;

      struct glaze
      {
         static constexpr auto value = glz::object(
            "Metadata", &JsonPlexCollectionResult::data
         );
      };
   };
   // } Collection Item Structs

   // Library Structs
   // {
   struct JsonPlexLibrary
   {
      std::string title;
      std::string id;

      struct glaze
      {
         static constexpr auto value = glz::object(
            "title", &JsonPlexLibrary::title,
            "key", &JsonPlexLibrary::id
         );
      };
   };
   struct JsonPlexLibraryResult
   {
      std::vector<JsonPlexLibrary> libraries;

      struct glaze
      {
         static constexpr auto value = glz::object(
            "Directory", &JsonPlexLibraryResult::libraries
         );
      };
   };
   // } Library Structs

   // Meta data structs
   // {
   struct JsonPlexMetadata
   {
      std::string ratingKey;
      std::vector<JsonPlexMedia> media;

      struct glaze
      {
         static constexpr auto value = glz::object(
            "ratingKey", &JsonPlexMetadata::ratingKey,
            "Media", &JsonPlexMetadata::media
         );
      };
   };
   struct JsonPlexMetadataContainer
   {
      std::vector<JsonPlexMetadata> data;

      struct glaze
      {
         static constexpr auto value = glz::object(
            "Metadata", &JsonPlexMetadataContainer::data
         );
      };
   };
   // } Meta data structs

   // Server name structs
   // {
   struct JsonPlexServerName
   {
      std::string name;

      struct glaze
      {
         static constexpr auto value = glz::object(
            "name", &JsonPlexServerName::name
         );
      };
   };
   struct JsonPlexServerData
   {
      std::vector<JsonPlexServerName> data;

      struct glaze
      {
         static constexpr auto value = glz::object(
            "Server", &JsonPlexServerData::data
         );
      };
   };
   // } Server name structs

   template <typename T>
   struct JsonPlexResponse
   {
      T response;

      struct glaze
      {
         static constexpr auto value = glz::object(
            "MediaContainer", &JsonPlexResponse::response
         );
      };
   };
}