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
      std::filesystem::path file;

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

   // Library Section Item Structs
   // {
   struct JsonPlexLibrarySectionItemData
   {
      std::string title;
      std::string ratingKey;
      std::string key;
      int64_t updatedAt;
      std::vector<JsonPlexMedia> media;

      struct glaze
      {
         static constexpr auto value = glz::object(
            "title", &JsonPlexLibrarySectionItemData::title,
            "ratingKey", &JsonPlexLibrarySectionItemData::ratingKey,
            "key", &JsonPlexLibrarySectionItemData::key,
            "updatedAt", &JsonPlexLibrarySectionItemData::updatedAt,
            "Media", &JsonPlexLibrarySectionItemData::media
         );
      };
   };
   struct JsonPlexLibrarySectionResult
   {
      int32_t totalSize;
      std::vector<JsonPlexLibrarySectionItemData> data;

      struct glaze
      {
         static constexpr auto value = glz::object(
            "totalSize", &JsonPlexLibrarySectionResult::totalSize,
            "Metadata", &JsonPlexLibrarySectionResult::data
         );
      };
   };
   // } Library Section Item Structs

   // Library Structs
   // {
   struct JsonPlexLibrary
   {
      std::string title;
      std::string id;
      std::string type;
      std::string agent;
      int64_t contentChangedAt{0};

      struct glaze
      {
         static constexpr auto value = glz::object(
            "title", &JsonPlexLibrary::title,
            "key", &JsonPlexLibrary::id,
            "type", &JsonPlexLibrary::type,
            "agent", &JsonPlexLibrary::agent,
            "contentChangedAt", &JsonPlexLibrary::contentChangedAt
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
      std::string title;
      std::optional<std::string> showTitle;
      uint64_t duration;
      std::optional<int> viewCount;
      std::optional<int64_t> viewOffset;
      std::vector<JsonPlexMedia> media;

      struct glaze
      {
         static constexpr auto value = glz::object(
            "ratingKey", &JsonPlexMetadata::ratingKey,
            "title", &JsonPlexMetadata::title,
            "grandparentTitle", &JsonPlexMetadata::showTitle,
            "duration", &JsonPlexMetadata::duration,
            "viewCount", &JsonPlexMetadata::viewCount,
            "viewOffset", &JsonPlexMetadata::viewOffset,
            "Media", &JsonPlexMetadata::media
         );
      };
   };
   struct JsonPlexMetadataContainer
   {
      std::string library;
      std::vector<JsonPlexMetadata> data;

      struct glaze
      {
         static constexpr auto value = glz::object(
            "librarySectionTitle", &JsonPlexMetadataContainer::library,
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