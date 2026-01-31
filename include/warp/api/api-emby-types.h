#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace warp
{
   using EmbyPathMap = std::unordered_map<std::string, std::string>;

   enum class EmbySearchType
   {
      id,
      name,
      path
   };

   struct EmbyItemSeries
   {
      std::string name;
      uint32_t seasonNum{0u};
      uint32_t episodeNum{0u};
   };

   struct EmbyItem
   {
      std::string name;
      std::string id;
      std::string path;
      std::string type;
      EmbyItemSeries series;
      uint64_t runTimeTicks{0u};
   };

   struct EmbyPlaylistItem
   {
      std::string name;
      std::string id;
      std::string playlistId;
   };

   struct EmbyPlaylist
   {
      std::string name;
      std::string id;
      std::vector<EmbyPlaylistItem> items;
   };

   struct EmbyUserData
   {
      std::string name;
      std::string id;
   };

   struct EmbyPlayState
   {
      std::string path;
      float percentage{0.0f};
      int64_t runTimeTicks{0};
      int64_t playbackPositionTicks{0};
      int32_t play_count{0};
      bool watched{false};
   };
}