#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace warp
{
   struct JsonServerResponse
   {
      std::string ServerName;
   };

   struct JsonEmbyItem
   {
      std::string Id;
      std::string Type;
      std::string Name;
      std::filesystem::path Path;
      std::string SeriesName;
      uint32_t ParentIndexNumber{0};
      uint32_t IndexNumber{0};
      uint64_t RunTimeTicks{0};
      std::vector<std::string> BackdropImageTags;
   };

   struct JsonEmbyItemsResponse
   {
      std::vector<JsonEmbyItem> Items;
   };

   struct PathRebuildItem
   {
      std::string Id;
      std::filesystem::path Path;
      std::string DateModified;
   };

   struct PathRebuildItems
   {
      std::vector<PathRebuildItem> Items;
   };

   struct JsonEmbyPlaylistItem
   {
      std::string Id;
      std::string Name;
      std::string PlaylistItemId;
   };

   struct JsonEmbyPlaylistItemsResponse
   {
      std::vector<JsonEmbyPlaylistItem> Items;
   };

   struct JsonEmbyUser
   {
      std::string Name;
      std::string Id;
   };

   struct JsonEmbyLibrary
   {
      std::string Name;
      std::string Id;
   };

   struct JsonTotalRecordCount
   {
      int32_t TotalRecordCount{0};
   };

   struct JsonEmbyPlaystateUserData
   {
      float PlayedPercentage{0.0f};
      int64_t PlaybackPositionTicks{0};
      int32_t PlayCount{0};
      bool Played{false};
   };

   struct JsonEmbyPlaystate
   {
      std::string Name;
      std::string Type;
      std::filesystem::path Path;
      int64_t RunTimeTicks{0};
      JsonEmbyPlaystateUserData UserData;
   };

   struct JsonEmbyPlayStates
   {
      std::vector<JsonEmbyPlaystate> Items;
   };

   struct JsonEmbyBackdrop
   {
      std::string ImageType;
      std::filesystem::path Path;
      std::optional<int32_t> ImageIndex;

   };
}