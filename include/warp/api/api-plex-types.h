#pragma once

#include <string>
#include <vector>

namespace warp
{
   enum PlexSearchTypes
   {
      plex_search_movie = 1,
      plex_search_show = 2,
      plex_search_season = 3,
      plex_search_episode = 4,
      plex_search_trailer = 5,
      plex_search_comic = 6,
      plex_search_person = 7,
      plex_search_artist = 8,
      plex_search_album = 9,
      plex_search_track = 10,
      plex_search_picture = 11,
      plex_search_clip = 12,
      plex_search_photo = 13,
      plex_search_photoalbum = 14,
      plex_search_playlist = 15,
      plex_search_playlistFolder = 16,
      plex_search_collection = 18,
      plex_search_optimizedVersion = 42,
      plex_search_userPlaylistItem = 1001,
   };

   struct PlexCollectionItem
   {
      std::string title;
      std::vector<std::filesystem::path> paths;
   };

   struct PlexCollection
   {
      std::string name;
      std::vector<PlexCollectionItem> items;
   };

   struct PlexSearchResult
   {
      std::string ratingKey;
      std::vector<std::filesystem::path> paths;
      std::string title;
      std::string libraryName;
      int64_t durationMs{0};
      int32_t playbackPercentage{0};
      bool watched{false};
   };

   struct PlexSearchResults
   {
      std::vector<PlexSearchResult> items;
   };
}