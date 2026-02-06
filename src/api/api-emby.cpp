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
#include <shared_mutex>

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
      constexpr std::string_view RECURSIVE{"Recursive"};
      constexpr std::string_view MOVIES{"Movies"};
      constexpr std::string_view SEARCH_TERM{"SearchTerm"};
      constexpr std::string_view FIELDS("Fields");
      constexpr std::string_view IS_MISSING{"IsMissing"};
      constexpr std::string_view ENTRY_IDS{"EntryIds"};
      constexpr std::string_view BACKDROP("Backdrop");
      constexpr std::string_view PARENT_ID("ParentId");
      constexpr std::string_view INCLUDE_ITEM_TYPES("IncludeItemTypes");
   }

   struct EmbyApi::EmbyApiImpl
   {
      EmbyApi& parent_;
      Headers headers_;
      std::filesystem::path mediaPath_;

      std::string lastSyncTimestamp_;

      using EmbyNameToIdMap = std::unordered_map<std::string, std::string, StringHash, std::equal_to<>>;
      EmbyNameToIdMap libraries_;
      EmbyNameToIdMap workingLibraries_;

      EmbyNameToIdMap users_;
      EmbyNameToIdMap workingUsers_;

      using EmbyPathMap = std::unordered_map<std::filesystem::path, std::string, PathHash>;
      bool enableExtraCache_{false};
      EmbyPathMap pathMap_;
      EmbyPathMap workingPathMap_;

      mutable std::shared_mutex dataLock_;

      EmbyApiImpl(EmbyApi& p, std::string_view appName, std::string_view version, const ServerConfig& serverConfig);

      [[nodiscard]] bool GetLibraryMapEmpty() const;
      [[nodiscard]] bool GetUsersMapEmpty() const;

      std::optional<std::string> GetLibraryId(std::string_view libraryName);
      std::optional<EmbyUserData> GetUser(std::string_view name);

      bool GetPathCacheEmpty() const;
      std::optional<std::string> GetIdFromPath(const std::filesystem::path& path);

      void RebuildPathMap();
      void RebuildLibraryMap();
      void RebuildUsersMap();

      bool HasLibraryChanged();

      void UpdateRequiredCache(bool forceRefresh);
      void UpdateExtraCache(bool forceRefresh);
      void RefreshCache(bool forceRefresh);

      std::string_view GetSearchTypeStr(EmbySearchType type);
   };

   EmbyApi::EmbyApiImpl::EmbyApiImpl(EmbyApi& p, std::string_view appName, std::string_view version, const ServerConfig& serverConfig)
      : parent_(p)
      , mediaPath_(serverConfig.mediaPath)
   {
      std::string auth = std::format("MediaBrowser Client=\"{}\", Device=\"PC\", DeviceId=\"{}\", Version=\"{}\", Token=\"{}\"",
                                     appName,
                                     "6e7417e2-8d76-4b1f-9c23-018274959a37",
                                     version,
                                     serverConfig.apiKey);
      headers_ = {
         {"X-Emby-Authorization", auth},
         {"Accept", "application/json"},
         {"User-Agent", std::format("{}/{}", appName, version)}
      };

      UpdateRequiredCache(true);
   }

   EmbyApi::EmbyApi(std::string_view appName, std::string_view version, const ServerConfig& serverConfig)
      : ApiBase(ApiBaseData{.name = serverConfig.serverName,
            .url = serverConfig.url,
            .apiKey = serverConfig.apiKey,
            .className = "EmbyApi",
            .ansiiCode = ANSI_CODE_EMBY,
            .prettyName = GetServerName(GetFormattedEmby(), serverConfig.serverName)})
      , pimpl_(std::make_unique<EmbyApiImpl>(*this, appName, version, serverConfig))
   {
   }

   EmbyApi::~EmbyApi() = default;

   void EmbyApi::EnableExtraCaching()
   {
      pimpl_->enableExtraCache_ = true;
      pimpl_->UpdateExtraCache(true);
   }

   std::optional<std::vector<Task>> EmbyApi::GetTaskList()
   {
      std::vector<Task> tasks;

      auto& quickCheck = tasks.emplace_back();
      quickCheck.name = std::format("EmbyApi({}) - Refresh Cache Quick", GetName());
      quickCheck.cronExpression = GetNextCronQuickTime();
      quickCheck.func = [this]() {pimpl_->RefreshCache(false); };

      auto& fullUpdate = tasks.emplace_back();
      fullUpdate.name = std::format("EmbyApi({}) - Refresh Cache Full", GetName());
      fullUpdate.cronExpression = GetNextCronFullTime();
      fullUpdate.func = [this]() {pimpl_->RefreshCache(true); };

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
      auto res = Get(BuildApiPath(API_SYSTEM_INFO), pimpl_->headers_);
      return res.error == Error::Success && res.status < VALID_HTTP_RESPONSE_MAX;
   }

   const std::filesystem::path& EmbyApi::GetMediaPath() const
   {
      return pimpl_->mediaPath_;
   }

   std::optional<std::string> EmbyApi::GetServerReportedName()
   {
      auto res = Get(BuildApiPath(API_SYSTEM_INFO), pimpl_->headers_);
      if (!IsHttpSuccess(__func__, res))
      {
         return std::nullopt;
      }

      JsonServerResponse serverResponse;
      if (auto ec = glz::read < glz::opts{.error_on_unknown_keys = false} > (serverResponse, res.body))
      {
         LogWarning("{} - JSON Parse Error: {}",
                    __func__, glz::format_error(ec, res.body));
         return std::nullopt;
      }

      if (serverResponse.ServerName.empty())
      {
         return std::nullopt;
      }
      return std::move(serverResponse.ServerName);
   }

   std::optional<std::string> EmbyApi::EmbyApiImpl::GetLibraryId(std::string_view libraryName)
   {
      std::shared_lock lock(dataLock_);
      auto iter = libraries_.find(libraryName);
      return iter == libraries_.end() ? std::nullopt : std::make_optional(iter->second);
   }

   std::optional<std::string> EmbyApi::GetLibraryId(std::string_view libraryName)
   {
      return pimpl_->GetLibraryId(libraryName);
   }

   std::string_view EmbyApi::EmbyApiImpl::GetSearchTypeStr(EmbySearchType type)
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
         {RECURSIVE, "true"},
         {pimpl_->GetSearchTypeStr(type), name},
         {FIELDS, "Path,SeriesName,RunTimeTicks"}
      };
      params.reserve(params.size() + extraSearchArgs.size());
      params.insert(params.end(), extraSearchArgs.begin(), extraSearchArgs.end());

      auto res = Get(BuildApiParamsPath(API_ITEMS, params), pimpl_->headers_);
      if (!IsHttpSuccess(__func__, res)) return std::nullopt;

      JsonEmbyItemsResponse response;
      if (auto ec = glz::read < glz::opts{.error_on_unknown_keys = false} > (response, res.body))
      {
         LogWarning("{} - JSON Parse Error: {}", __func__, glz::format_error(ec, res.body));
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

      LogWarning("{} returned no valid results {}", __func__, GetTag("search", name));
      return std::nullopt;
   }

   std::optional<EmbyUserData> EmbyApi::EmbyApiImpl::GetUser(std::string_view name)
   {
      std::shared_lock lock(dataLock_);
      auto iter = users_.find(name);
      return iter == users_.end() ? std::nullopt : std::make_optional<EmbyUserData>(iter->first, iter->second);
   }

   std::optional<EmbyUserData> EmbyApi::GetUser(std::string_view name)
   {
      return pimpl_->GetUser(name);
   }

   bool EmbyApi::GetWatchedStatus(std::string_view userId, std::string_view itemId)
   {
      const auto apiPath = BuildApiParamsPath(std::format("{}/{}/Items", API_USERS, userId), {
         {IDS, itemId},
         {"IsPlayed", "true"}
      });

      auto res = Get(apiPath, pimpl_->headers_);
      if (!IsHttpSuccess(__func__, res)) return false;

      JsonTotalRecordCount response;
      if (auto ec = glz::read < glz::opts{.error_on_unknown_keys = false} > (response, res.body))
      {
         LogWarning("{} - JSON Parse Error: {}",
                    __func__, glz::format_error(ec, res.body));
         return false;
      }

      return response.TotalRecordCount > 0;
   }

   bool EmbyApi::SetWatchedStatus(std::string_view userId, std::string_view itemId)
   {
      const auto apiPath = BuildApiPath(std::format("{}/{}/PlayedItems/{}", API_USERS, userId, itemId));
      auto res = Post(apiPath, pimpl_->headers_);
      return IsHttpSuccess(__func__, res);
   }

   std::optional<EmbyPlayState> EmbyApi::GetPlayState(std::string_view userId, std::string_view itemId)
   {
      const auto apiPath = BuildApiParamsPath(std::format("{}/{}/Items", API_USERS, userId), {
         {IDS, itemId},
         {FIELDS, "Path,UserDataLastPlayedDate,UserDataPlayCount"}
      });

      auto res = Get(apiPath, pimpl_->headers_);
      if (!IsHttpSuccess(__func__, res)) return std::nullopt;

      JsonEmbyPlayStates response;
      if (auto ec = glz::read < glz::opts{.error_on_unknown_keys = false} > (response, res.body))
      {
         LogWarning("{} - JSON Parse Error: {}",
                    __func__, glz::format_error(ec, res.body));
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
                           .watched = item.UserData.Played};
   }

   bool EmbyApi::SetPlayState(std::string_view userId, std::string_view itemId, int64_t positionTicks, std::string_view dateTimeStr)
   {
      const auto apiPath = BuildApiParamsPath(std::format("{}/{}/Items/{}/UserData", API_USERS, userId, itemId), {
         {"PlaybackPositionTicks", std::to_string(positionTicks)},
         {"LastPlayedDate", dateTimeStr}
      });
      auto res = Post(apiPath, pimpl_->headers_);
      return IsHttpSuccess(__func__, res);
   }

   bool EmbyApi::GetPlaylistExists(std::string_view name)
   {
      return GetItem(EmbySearchType::name, name, {{INCLUDE_ITEM_TYPES, "Playlist"}}).has_value();
   }

   std::optional<EmbyPlaylist> EmbyApi::GetPlaylist(std::string_view name)
   {
      static const ApiParams apiParams = {
         {INCLUDE_ITEM_TYPES, "Playlist"}
      };
      auto item = GetItem(EmbySearchType::name, name, apiParams);
      if (!item.has_value()) return std::nullopt;

      auto res = Get(BuildApiPath(std::format("{}/{}/Items", API_PLAYLISTS, item->id)), pimpl_->headers_);
      if (!IsHttpSuccess(__func__, res)) return std::nullopt;

      // Parse the entire "Items" array directly into our struct
      JsonEmbyPlaylistItemsResponse response;
      if (auto ec = glz::read < glz::opts{.error_on_unknown_keys = false} > (response, res.body))
      {
         LogWarning("{} - JSON Parse Error: {}",
                    __func__, glz::format_error(ec, res.body));
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
      const auto apiPath = BuildApiParamsPath(API_PLAYLISTS, {
         {NAME, name},
         {IDS, BuildCommaSeparatedList(itemIds)},
         {MEDIA_TYPE, MOVIES}
      });

      auto res = Post(apiPath, pimpl_->headers_);
      IsHttpSuccess(__func__, res);
   }

   bool EmbyApi::AddPlaylistItems(std::string_view playlistId, const std::vector<std::string>& addIds)
   {
      auto apiPath = BuildApiParamsPath(std::format("{}/{}/Items", API_PLAYLISTS, playlistId), {
         {IDS, BuildCommaSeparatedList(addIds)}
      });

      auto res = Post(apiPath, pimpl_->headers_);
      return IsHttpSuccess(__func__, res);
   }

   bool EmbyApi::RemovePlaylistItems(std::string_view playlistId, const std::vector<std::string>& removeIds)
   {
      const auto apiPath = BuildApiParamsPath(std::format("{}/{}/Items/Delete", API_PLAYLISTS, playlistId), {
         {ENTRY_IDS, BuildCommaSeparatedList(removeIds)}
      });

      auto res = Post(apiPath, pimpl_->headers_);
      return IsHttpSuccess(__func__, res);
   }

   bool EmbyApi::MovePlaylistItem(std::string_view playlistId, std::string_view itemId, uint32_t index)
   {
      auto apiPath{BuildApiPath(std::format("{}/{}/Items/{}/Move/{}", API_PLAYLISTS, playlistId, itemId, index))};

      auto res = Post(apiPath, pimpl_->headers_);
      return IsHttpSuccess(__func__, res);
   }

   std::vector<EmbyItemBackdropImages> EmbyApi::GetItemsWithMultipleBackdrops(std::string_view libId)
   {
      const ApiParams apiParams = {
         {RECURSIVE, "true"},
         {INCLUDE_ITEM_TYPES, "Movie,Series"},
         {IS_MISSING, "false"},
         {PARENT_ID, libId}
      };
      auto res = Get(BuildApiParamsPath(API_ITEMS, apiParams), pimpl_->headers_);
      if (!IsHttpSuccess(__func__, res)) return {};

      JsonEmbyItemsResponse response;
      if (auto ec = glz::read < glz::opts{.error_on_unknown_keys = false} > (response, res.body))
      {
         LogWarning("{} - JSON Parse Error: {}", __func__, glz::format_error(ec, res.body));
         return {};
      }

      std::vector<EmbyItemBackdropImages> returnVector;
      returnVector.reserve(response.Items.size());
      for (auto& item : response.Items)
      {
         if (item.BackdropImageTags.size() < 2) continue;

         returnVector.emplace_back(EmbyItemBackdropImages{
            .name = std::move(item.Name),
            .id = std::move(item.Id),
            .backdropImageIds = std::move(item.BackdropImageTags)
         });
      }

      return returnVector;
   }

   [[nodiscard]] std::vector<EmbyBackdrop> EmbyApi::GetBackdrops(std::string_view id)
   {
      const auto apiPath = BuildApiPath(std::format("/Items/{}/Images", id));
      auto res = Get(apiPath, pimpl_->headers_);
      if (!IsHttpSuccess(__func__, res)) return {};

      std::vector<JsonEmbyBackdrop> response;
      if (auto ec = glz::read < glz::opts{.error_on_unknown_keys = false} > (response, res.body))
      {
         LogWarning("{} - JSON Parse Error: {}", __func__, glz::format_error(ec, res.body));
         return {};
      }

      std::vector<EmbyBackdrop> returnBackdrops;
      for (auto& image : response)
      {
         if (image.ImageType == BACKDROP && image.ImageIndex)
         {
            returnBackdrops.emplace_back(EmbyBackdrop{
               .index = *image.ImageIndex,
               .path = std::move(image.Path)
            });
         }
      }

      return returnBackdrops;
   }

   bool EmbyApi::RemoveBackdropImage(std::string_view id, int32_t index)
   {
      const auto apiPath = BuildApiPath(std::format("/Items/{}/Images/Backdrop/{}", id, index));

      auto res = Delete(apiPath, pimpl_->headers_);
      return IsHttpSuccess(__func__, res);
   }

   void EmbyApi::SetLibraryScan(std::string_view libraryId)
   {
      static const ApiParams apiParams = {
         {RECURSIVE, "true"},
         {"ImageRefreshMode", "Default"},
         {"ReplaceAllImages", "false"},
         {"ReplaceAllMetadata", "false"}
      };
      const auto apiPath = BuildApiParamsPath(std::format("/Items/{}/Refresh", libraryId), apiParams);

      auto res = Post(apiPath, pimpl_->headers_);
      IsHttpSuccess(__func__, res);
   }

   bool EmbyApi::EmbyApiImpl::GetPathCacheEmpty() const
   {
      std::lock_guard lock(dataLock_);
      return pathMap_.empty();
   }

   bool EmbyApi::GetPathCacheEmpty() const
   {
      return pimpl_->GetPathCacheEmpty();
   }

   std::optional<std::string> EmbyApi::EmbyApiImpl::GetIdFromPath(const std::filesystem::path& path)
   {
      if (!enableExtraCache_)
      {
         parent_.LogWarning("{} called but extra caching not enabled", __func__);
         return std::nullopt;
      }

      std::lock_guard lock(dataLock_);

      // Look up the item once
      if (auto it = pathMap_.find(path);
          it != pathMap_.end())
      {
         return it->second;
      }
      return std::nullopt;
   }

   std::optional<std::string> EmbyApi::GetIdFromPath(const std::filesystem::path& path)
   {
      return pimpl_->GetIdFromPath(path);
   }

   bool EmbyApi::EmbyApiImpl::GetLibraryMapEmpty() const
   {
      std::shared_lock lock(dataLock_);
      return libraries_.empty();
   }

   bool EmbyApi::EmbyApiImpl::GetUsersMapEmpty() const
   {
      std::shared_lock lock(dataLock_);
      return users_.empty();
   }

   void EmbyApi::EmbyApiImpl::RebuildPathMap()
   {
      parent_.LogTrace("Rebuilding Path Map");

      static const ApiParams apiParams = {
         {RECURSIVE, "true"},
         {INCLUDE_ITEM_TYPES, "Movie,Episode"},
         {FIELDS, "Path,DateModified"},
         {IS_MISSING, "false"}
      };
      const auto apiPath = parent_.BuildApiParamsPath(API_ITEMS, apiParams);

      auto res = parent_.Get(apiPath, headers_);
      if (!parent_.IsHttpSuccess(__func__, res)) return;

      PathRebuildItems response;
      if (auto ec = glz::read < glz::opts{.error_on_unknown_keys = false} > (response, res.body))
      {
         parent_.LogWarning("{} - JSON Parse Error: {}",
                           __func__, glz::format_error(ec, res.body)); // Log the start of the string for context
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
         std::lock_guard lock(dataLock_);
         std::swap(workingPathMap_, pathMap_);
         lastSyncTimestamp_ = std::move(localMaxTimestamp);

         workingPathMap_.clear();
      }
      else
      {
         parent_.LogWarning("{} - Keeping stale path data due to fetch failures", __func__);
      }
   }

   void EmbyApi::EmbyApiImpl::RebuildLibraryMap()
   {
      parent_.LogTrace("Rebuilding Library Map");

      auto res = parent_.Get(parent_.BuildApiPath(API_MEDIA_FOLDERS), headers_);
      if (!parent_.IsHttpSuccess(__func__, res)) return;

      std::vector<JsonEmbyLibrary> jsonLibraries;
      if (auto ec = glz::read < glz::opts{.error_on_unknown_keys = false} > (jsonLibraries, res.body))
      {
         parent_.LogWarning("{} - JSON Parse Error: {}",
                           __func__, glz::format_error(ec, res.body));
         return;
      }

      workingLibraries_.reserve(jsonLibraries.size());
      for (auto& library : jsonLibraries)
      {
         workingLibraries_.emplace(std::move(library.Name), std::move(library.Id));
      }

      if (!workingLibraries_.empty())
      {
         std::unique_lock lock(dataLock_);
         std::swap(workingLibraries_, libraries_);
         workingLibraries_.clear();
      }
      else
      {
         parent_.LogWarning("{} - Keeping stale library data due to fetch failures", __func__);
      }
   }

   void EmbyApi::EmbyApiImpl::RebuildUsersMap()
   {
      parent_.LogTrace("Rebuilding User Map");

      auto res = parent_.Get(parent_.BuildApiPath(API_USERS), headers_);
      if (!parent_.IsHttpSuccess(__func__, res)) return;

      // Parse into a vector of our minimal user structs
      std::vector<JsonEmbyUser> users;
      if (auto ec = glz::read < glz::opts{.error_on_unknown_keys = false} > (users, res.body))
      {
         parent_.LogWarning("{} - JSON Parse Error: {}",
                            __func__, glz::format_error(ec, res.body));
         return;
      }

      workingUsers_.reserve(users.size());
      std::ranges::for_each(users, [&](auto& user) {
         workingUsers_.emplace(std::move(user.Name), std::move(user.Id));
      });

      if (!workingUsers_.empty())
      {
         std::unique_lock lock(dataLock_);
         std::swap(workingUsers_, users_);
         workingUsers_.clear();
      }
      else
      {
         parent_.LogWarning("{} - Keeping stale user data due to fetch failures", __func__);
      }
   }

   bool EmbyApi::EmbyApiImpl::HasLibraryChanged()
   {
      static const ApiParams apiParams = {
         {RECURSIVE, "true"},
         {INCLUDE_ITEM_TYPES, "Movie,Episode"},
         {"SortBy", "DateModified"},
         {"SortOrder", "Descending"},
         {"Limit", "1"},
         {FIELDS, "DateModified"}
      };
      const auto apiPath = parent_.BuildApiParamsPath(API_ITEMS, apiParams);

      auto res = parent_.Get(apiPath, headers_);
      if (!parent_.IsHttpSuccess(__func__, res)) return false;

      PathRebuildItems response;
      if (auto ec = glz::read < glz::opts{.error_on_unknown_keys = false} > (response, res.body))
      {
         parent_.LogWarning("{} - JSON Parse Error: {}",
                            __func__, glz::format_error(ec, res.body)); // Log the start of the string for context
         return false;
      }

      return !response.Items.empty()
         && !response.Items[0].DateModified.empty()
         && response.Items[0].DateModified > lastSyncTimestamp_;
   }

   void EmbyApi::EmbyApiImpl::UpdateRequiredCache(bool forceRefresh)
   {
      if (forceRefresh || GetLibraryMapEmpty()) RebuildLibraryMap();
      if (forceRefresh || GetUsersMapEmpty()) RebuildUsersMap();
   }

   void EmbyApi::EmbyApiImpl::UpdateExtraCache(bool forceRefresh)
   {
      if (forceRefresh || GetPathCacheEmpty() || HasLibraryChanged())  RebuildPathMap();
   }

   void EmbyApi::EmbyApiImpl::RefreshCache(bool forceRefresh)
   {
      UpdateRequiredCache(forceRefresh);
      if (enableExtraCache_) UpdateExtraCache(forceRefresh);
   }
}