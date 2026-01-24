#include "warp/api/api-emby.h"

#include "api/api-emby-json-types.h"
#include "api/api-utils.h"
#include "warp/log/log-utils.h"
#include "warp/types.h"
#include "warp/utils.h"

#include <glaze/glaze.hpp>

#include <format>
#include <mutex>
#include <numeric>
#include <ranges>

namespace warp
{
   namespace
   {
      const std::string API_BASE{"/emby"};
      const std::string API_TOKEN_NAME{"api_key"};

      const std::string API_SYSTEM_INFO{"/System/Info"};
      const std::string API_MEDIA_FOLDERS{"/Library/SelectableMediaFolders"};
      const std::string API_ITEMS{"/Items"};
      const std::string API_PLAYLISTS{"/Playlists"};
      const std::string API_USERS{"/Users"};

      constexpr std::string_view NAME{"Name"};
      constexpr std::string_view IDS{"Ids"};
      constexpr std::string_view PATH{"Path"};
      constexpr std::string_view MEDIA_TYPE{"MediaType"};
      constexpr std::string_view MOVIES{"Movies"};
      constexpr std::string_view SEARCH_TERM{"SearchTerm"};
      constexpr std::string_view ENTRY_IDS{"EntryIds"};
   }

   EmbyApi::EmbyApi(std::string_view version, const ServerConfig& serverConfig)
      : ApiBase(ApiBaseData{.name = serverConfig.server_name,
            .url = serverConfig.url,
            .apiKey = serverConfig.api_key,
            .className = "EmbyApi",
            .ansiiCode = warp::ANSI_CODE_EMBY,
            .prettyName = warp::GetServerName(warp::GetFormattedEmby(), serverConfig.server_name)})
      , mediaPath_(serverConfig.media_path)
   {
      std::string auth = std::format("MediaBrowser Client=\"Loomis\", Device=\"PC\", DeviceId=\"{}\", Version=\"{}\", Token=\"{}\"",
                                     "6e7417e2-8d76-4b1f-9c23-018274959a37",
                                     version,
                                     serverConfig.api_key);
      headers_ = {
        {"X-Emby-Authorization", auth},
        {"Accept", "application/json"},
        {"User-Agent", std::format("Loomis/{}", version)}
      };

      // If the service is valid run any needed tasks
      if (GetValid()) BuildPathMap();
   }

   std::optional<std::vector<ApiTask>> EmbyApi::GetTaskList()
   {
      std::vector<ApiTask> tasks;

      auto& quickCheck = tasks.emplace_back();
      quickCheck.name = std::format("EmbyApi({}) - Path Map Quick Check", GetName());
      quickCheck.cronExpression = "30 */5 * * * *";
      quickCheck.func = [this]() {this->RunPathMapQuickCheck(); };

      auto& fullUpdate = tasks.emplace_back();
      fullUpdate.name = std::format("EmbyApi({}) - Path Map Full Update", GetName());
      fullUpdate.cronExpression = "0 45 3 * * *";
      fullUpdate.func = [this]() {this->RunPathMapFullUpdate(); };

      return tasks;
   }

   std::string_view EmbyApi::GetApiBase() const
   {
      return API_BASE;
   }

   std::string_view EmbyApi::GetApiTokenName() const
   {
      return "";
   }

   bool EmbyApi::GetValid()
   {
      auto res = GetClient().Get(BuildApiPath(API_SYSTEM_INFO), headers_);
      return res.error() == httplib::Error::Success && res.value().status < VALID_HTTP_RESPONSE_MAX;
   }

   std::string_view EmbyApi::GetMediaPath() const
   {
      return mediaPath_;
   }

   std::optional<std::string> EmbyApi::GetServerReportedName()
   {
      auto res = GetClient().Get(BuildApiPath(API_SYSTEM_INFO), headers_);

      if (!IsHttpSuccess(__func__, res))
      {
         return std::nullopt;
      }

      JsonServerResponse serverResponse;
      if (auto ec = glz::read < glz::opts{.error_on_unknown_keys = false} > (serverResponse, res.value().body))
      {
         LogWarning("{} - JSON Parse Error: {}",
                    __func__, glz::format_error(ec, res.value().body));
         return std::nullopt;
      }

      if (serverResponse.ServerName.empty())
      {
         return std::nullopt;
      }
      return std::move(serverResponse.ServerName);
   }

   std::optional<std::string> EmbyApi::GetLibraryId(std::string_view libraryName)
   {
      auto res = GetClient().Get(BuildApiPath(API_MEDIA_FOLDERS), headers_);

      if (!IsHttpSuccess(__func__, res)) return std::nullopt;

      std::vector<JsonEmbyLibrary> jsonLibraries;
      if (auto ec = glz::read < glz::opts{.error_on_unknown_keys = false} > (jsonLibraries, res.value().body))
      {
         LogWarning("{} - JSON Parse Error: {}",
                    __func__, glz::format_error(ec, res.value().body));
         return std::nullopt;
      }

      auto it = std::ranges::find_if(jsonLibraries, [&](const auto& lib) {
         return lib.Name == libraryName;
      });

      if (it != jsonLibraries.end())
      {
         // Move the ID out of our temporary vector and return it
         return std::move(it->Id);
      }
      return std::nullopt;
   }

   std::string_view EmbyApi::GetSearchTypeStr(EmbySearchType type)
   {
      switch (type)
      {
         case EmbySearchType::id:
            return IDS;
         case EmbySearchType::path:
            return PATH;
         default:
            return SEARCH_TERM;
      }
   }

   std::optional<EmbyItem> EmbyApi::GetItem(EmbySearchType type, std::string_view name, const ApiParams& extraSearchArgs)
   {
      ApiParams params = {
         {"Recursive", "true"},
         {GetSearchTypeStr(type), name},
         {"Fields", "Path,SeriesName,RunTimeTicks"}
      };
      params.reserve(params.size() + extraSearchArgs.size());
      params.insert(params.end(), extraSearchArgs.begin(), extraSearchArgs.end());

      auto res{GetClient().Get(BuildApiParamsPath(API_ITEMS, params), headers_)};
      if (!IsHttpSuccess(__func__, res)) return std::nullopt;

      JsonEmbyItemsResponse response;
      if (auto ec = glz::read < glz::opts{.error_on_unknown_keys = false} > (response, res.value().body))
      {
         LogWarning("{} - JSON Parse Error: {}", __func__, glz::format_error(ec, res.value().body));
         return std::nullopt;
      }

      // Use a lambda to find the specific item based on the search type
      auto it = std::ranges::find_if(response.Items, [&](const JsonEmbyItem& item) {
         switch (type)
         {
            case EmbySearchType::id:   return item.Id == name;
            case EmbySearchType::path: return item.Path == name;
            case EmbySearchType::name: return item.Name == name;
            default: return false;
         }
      });

      if (it != response.Items.end())
      {
         auto& match = *it;
         EmbyItem returnItem;

         // Move flat data
         returnItem.id = std::move(match.Id);
         returnItem.type = std::move(match.Type);
         returnItem.name = std::move(match.Name);
         returnItem.path = std::move(match.Path);
         returnItem.runTimeTicks = match.RunTimeTicks;

         // Populate nested series data
         returnItem.series.name = std::move(match.SeriesName);
         returnItem.series.seasonNum = match.ParentIndexNumber;
         returnItem.series.episodeNum = match.IndexNumber;

         return returnItem;
      }

      LogWarning("{} returned no valid results {}", __func__, warp::GetTag("search", name));
      return std::nullopt;
   }

   std::optional<EmbyUserData> EmbyApi::GetUser(std::string_view name)
   {
      auto res = GetClient().Get(BuildApiPath(API_USERS), headers_);

      if (!IsHttpSuccess(__func__, res)) return std::nullopt;

      // Parse into a vector of our minimal user structs
      std::vector<JsonEmbyUser> users;
      if (auto ec = glz::read < glz::opts{.error_on_unknown_keys = false} > (users, res.value().body))
      {
         LogWarning("{} - JSON Parse Error: {}",
                    __func__, glz::format_error(ec, res.value().body));
         return std::nullopt;
      }

      // Use any_of to find a match - stops immediately when found
      auto iter = std::ranges::find_if(users, [&](const auto& user) {
         return user.Name == name;
      });

      if (iter == users.end()) return std::nullopt;

      return EmbyUserData{std::move(iter->Name), std::move(iter->Id)};
   }

   bool EmbyApi::GetWatchedStatus(std::string_view userId, std::string_view itemId)
   {
      const auto apiUrl = BuildApiParamsPath(std::format("{}/{}/Items", API_USERS, userId), {
         {IDS, itemId},
         {"IsPlayed", "true"}
      });
      auto res = GetClient().Get(apiUrl, headers_);
      if (!IsHttpSuccess(__func__, res)) return false;

      JsonTotalRecordCount response;
      if (auto ec = glz::read < glz::opts{.error_on_unknown_keys = false} > (response, res.value().body))
      {
         LogWarning("{} - JSON Parse Error: {}",
                    __func__, glz::format_error(ec, res.value().body));
         return false;
      }

      return response.TotalRecordCount > 0;
   }

   bool EmbyApi::SetWatchedStatus(std::string_view userId, std::string_view itemId)
   {
      const auto apiUrl = BuildApiPath(std::format("{}/{}/PlayedItems/{}", API_USERS, userId, itemId));
      auto res{GetClient().Post(apiUrl, headers_)};
      return IsHttpSuccess(__func__, res);
   }

   std::optional<EmbyPlayState> EmbyApi::GetPlayState(std::string_view userId, std::string_view itemId)
   {
      const auto apiUrl = BuildApiParamsPath(std::format("{}/{}/Items", API_USERS, userId), {
         {IDS, itemId},
         {"Fields", "Path,UserDataLastPlayedDate,UserDataPlayCount"}
      });

      auto res = GetClient().Get(apiUrl, headers_);
      if (!IsHttpSuccess(__func__, res)) return std::nullopt;

      JsonEmbyPlayStates response;
      if (auto ec = glz::read < glz::opts{.error_on_unknown_keys = false} > (response, res.value().body))
      {
         LogWarning("{} - JSON Parse Error: {}",
                    __func__, glz::format_error(ec, res.value().body));
         return std::nullopt;
      }

      if (response.Items.empty()) return std::nullopt;

      auto& item = response.Items[0];
      if (item.Type != "Movie" && item.Type != "Episode") return std::nullopt;

      return EmbyPlayState{.path = std::move(item.Path),
                           .percentage = item.UserData.PlayedPercentage,
                           .runTimeTicks = item.RunTimeTicks,
                           .playbackPositionTicks = item.UserData.PlaybackPositionTicks,
                           .play_count = item.UserData.PlayCount,
                           .played = item.UserData.Played};
   }

   bool EmbyApi::SetPlayState(std::string_view userId, std::string_view itemId, int64_t positionTicks, std::string_view dateTimeStr)
   {
      const auto apiUrl = BuildApiParamsPath(std::format("{}/{}/Items/{}/UserData", API_USERS, userId, itemId), {
         {"PlaybackPositionTicks", std::to_string(positionTicks)},
         {"LastPlayedDate", dateTimeStr}
      });
      auto res{GetClient().Post(apiUrl, headers_)};
      return IsHttpSuccess(__func__, res);
   }

   bool EmbyApi::GetPlaylistExists(std::string_view name)
   {
      return GetItem(EmbySearchType::name, name, {{"IncludeItemTypes", "Playlist"}}).has_value();
   }

   std::optional<EmbyPlaylist> EmbyApi::GetPlaylist(std::string_view name)
   {
      auto item = GetItem(EmbySearchType::name, name, {{"IncludeItemTypes", "Playlist"}});
      if (!item.has_value()) return std::nullopt;

      auto res = GetClient().Get(BuildApiPath(std::format("{}/{}/Items", API_PLAYLISTS, item->id)), headers_);
      if (!IsHttpSuccess(__func__, res)) return std::nullopt;

      // Parse the entire "Items" array directly into our struct
      JsonEmbyPlaylistItemsResponse response;
      if (auto ec = glz::read < glz::opts{.error_on_unknown_keys = false} > (response, res.value().body))
      {
         LogWarning("{} - JSON Parse Error: {}",
                    __func__, glz::format_error(ec, res.value().body));
         return std::nullopt;
      }

      // Initialize our return object
      EmbyPlaylist returnPlaylist;
      returnPlaylist.name = std::move(item->name);
      returnPlaylist.id = std::move(item->id);

      returnPlaylist.items.reserve(response.Items.size());
      std::ranges::transform(response.Items, std::back_inserter(returnPlaylist.items), [](auto& item) {
         return EmbyPlaylistItem{
             std::move(item.Name),
             std::move(item.Id),
             std::move(item.PlaylistItemId)
         };
      });

      return returnPlaylist;
   }

   void EmbyApi::CreatePlaylist(std::string_view name, const std::vector<std::string>& itemIds)
   {
      const auto apiUrl = BuildApiParamsPath(API_PLAYLISTS, {
         {NAME, name},
         {IDS, BuildCommaSeparatedList(itemIds)},
         {MEDIA_TYPE, MOVIES}
      });
      auto res{GetClient().Post(apiUrl, headers_)};
      IsHttpSuccess(__func__, res);
   }

   bool EmbyApi::AddPlaylistItems(std::string_view playlistId, const std::vector<std::string>& addIds)
   {
      auto apiUrl = BuildApiParamsPath(std::format("{}/{}/Items", API_PLAYLISTS, playlistId), {
         {IDS, BuildCommaSeparatedList(addIds)}
      });
      auto res{GetClient().Post(apiUrl, headers_)};
      return IsHttpSuccess(__func__, res);
   }

   bool EmbyApi::RemovePlaylistItems(std::string_view playlistId, const std::vector<std::string>& removeIds)
   {
      const auto apiUrl = BuildApiParamsPath(std::format("{}/{}/Items/Delete", API_PLAYLISTS, playlistId), {
         {ENTRY_IDS, BuildCommaSeparatedList(removeIds)}
      });
      auto res{GetClient().Post(apiUrl, headers_)};
      return IsHttpSuccess(__func__, res);
   }

   bool EmbyApi::MovePlaylistItem(std::string_view playlistId, std::string_view itemId, uint32_t index)
   {
      auto apiUrl{BuildApiPath(std::format("{}/{}/Items/{}/Move/{}", API_PLAYLISTS, playlistId, itemId, index))};
      auto res{GetClient().Post(apiUrl, headers_)};
      return IsHttpSuccess(__func__, res);
   }

   void EmbyApi::SetLibraryScan(std::string_view libraryId)
   {
      const auto apiUrl = BuildApiParamsPath(std::format("/Items/{}/Refresh", libraryId), {
         {"Recursive", "true"},
         {"ImageRefreshMode", "Default"},
         {"ReplaceAllImages", "false"},
         {"ReplaceAllMetadata", "false"}
      });

      auto res = GetClient().Post(apiUrl, headers_);
      IsHttpSuccess(__func__, res);
   }

   void EmbyApi::BuildPathMap()
   {
      const auto apiUrl = BuildApiParamsPath(API_ITEMS, {
          {"Recursive", "true"},
          {"IncludeItemTypes", "Movie,Episode"},
          {"Fields", "Path,DateModified"},
          {"IsMissing", "false"}
      });

      auto res = GetClient().Get(apiUrl, headers_);
      if (!IsHttpSuccess(__func__, res)) return;

      PathRebuildItems response;
      if (auto ec = glz::read < glz::opts{.error_on_unknown_keys = false} > (response, res.value().body))
      {
         LogWarning("{} - JSON Parse Error: {}",
             __func__, glz::format_error(ec, res.value().body)); // Log the start of the string for context
         return;
      }

      workingPathMap_.reserve(response.Items.size());

      std::string localMaxTimestamp;
      for (auto& item : response.Items)
      {
         // Check for empty because a missing field in JSON results in an empty string in the struct
         if (!item.Path.empty() && !item.Id.empty())
         {
            // Move strings to avoid allocations
            workingPathMap_.emplace(std::move(item.Path), std::move(item.Id));

            // Track the newest timestamp
            if (item.DateModified > localMaxTimestamp)
            {
               localMaxTimestamp = std::move(item.DateModified);
            }
         }
      }

      if (!workingPathMap_.empty())
      {
         std::lock_guard lock(taskLock_);
         std::swap(workingPathMap_, pathMap_);
         lastSyncTimestamp_ = std::move(localMaxTimestamp);
      }

      workingPathMap_.clear();
   }

   bool EmbyApi::HasLibraryChanged()
   {
      const auto apiUrl = BuildApiParamsPath(API_ITEMS, {
          {"Recursive", "true"},
          {"IncludeItemTypes", "Movie,Episode"},
          {"SortBy", "DateModified"},
          {"SortOrder", "Descending"},
          {"Limit", "1"},
          {"Fields", "DateModified"}
      });

      auto res = GetClient().Get(apiUrl, headers_);
      if (!IsHttpSuccess(__func__, res)) return false;

      PathRebuildItems response;
      if (auto ec = glz::read < glz::opts{.error_on_unknown_keys = false} > (response, res.value().body))
      {
         LogWarning("{} - JSON Parse Error: {}",
             __func__, glz::format_error(ec, res.value().body)); // Log the start of the string for context
         return false;
      }

      return !response.Items.empty()
         && !response.Items[0].DateModified.empty()
         && response.Items[0].DateModified > lastSyncTimestamp_;
   }

   void EmbyApi::RunPathMapQuickCheck()
   {
      if (GetPathMapEmpty() || HasLibraryChanged())
      {
         BuildPathMap();
      }
   }

   void EmbyApi::RunPathMapFullUpdate()
   {
      BuildPathMap();
   }

   bool EmbyApi::GetPathMapEmpty() const
   {
      std::lock_guard lock(taskLock_);
      return pathMap_.empty();
   }

   std::optional<std::string> EmbyApi::GetIdFromPathMap(const std::string& path)
   {
      std::lock_guard lock(taskLock_);

      // Look up the item once
      if (auto it = pathMap_.find(path); it != pathMap_.end())
      {
         return it->second;
      }
      return std::nullopt;
   }
}