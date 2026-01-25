#include "warp/api/api-plex.h"

#include "api/api-plex-json-types.h"
#include "api/api-utils.h"
#include "warp/log/log.h"
#include "warp/log/log-utils.h"
#include "warp/types.h"
#include "warp/utils.h"

#include <glaze/glaze.hpp>

#include <charconv>
#include <cmath>
#include <format>
#include <ranges>

namespace warp
{
   namespace
   {
      constexpr std::string_view API_BASE{""};
      constexpr std::string_view API_TOKEN_NAME("X-Plex-Token");

      constexpr std::string_view API_SERVERS{"/servers"};
      constexpr std::string_view API_LIBRARIES{"/library/sections/"};
      constexpr std::string_view API_LIBRARY_DATA{"/library/metadata/"};
      constexpr std::string_view API_SEARCH{"/hubs/search"};

      constexpr std::string_view ELEM_MEDIA_CONTAINER{"MediaContainer"};
      constexpr std::string_view ELEM_MEDIA{"Media"};
      constexpr std::string_view ELEM_VIDEO{"Video"};

      constexpr std::string_view ATTR_NAME{"name"};
      constexpr std::string_view ATTR_KEY{"key"};
      constexpr std::string_view ATTR_TITLE{"title"};
      constexpr std::string_view ATTR_FILE{"file"};
   }

   PlexApi::PlexApi(std::string_view appName, std::string_view version, const ServerConfig& serverConfig)
      : ApiBase(ApiBaseData{.name = serverConfig.server_name,
                .url = serverConfig.url,
                .apiKey = serverConfig.api_key,
                .className = "PlexApi",
                .ansiiCode = warp::ANSI_CODE_PLEX,
                .prettyName = warp::GetServerName(warp::GetFormattedPlex(), serverConfig.server_name)})
      , mediaPath_(serverConfig.media_path)
   {
      headers_ = {
        {"X-Plex-Token", serverConfig.api_key},
        {"X-Plex-Client-Identifier", "6e7417e2-8d76-4b1f-9c23-018274959a37"},
        {"Accept", "application/json"},
        {"User-Agent", std::format("{}/{}", appName, version)}
      };

      headers_ = {
        {"X-Plex-Token", serverConfig.api_key},
        {"X-Plex-Client-Identifier", "6e7417e2-8d76-4b1f-9c23-018274959a37"},
        {"Accept", "application/json"},
        {"User-Agent", std::format("{}/{}", appName, version)}
      };

      UpdateRequiredCache(true);
   }

   void PlexApi::EnableExtraCaching()
   {
      enableExtraCache_ = true;
      UpdateExtraCache(true);
   }

   std::optional<std::vector<Task>> PlexApi::GetTaskList()
   {
      std::vector<Task> tasks;

      auto& quickCheck = tasks.emplace_back();
      quickCheck.name = std::format("PlexApi({}) - Data Quick Check", GetName());
      quickCheck.cronExpression = "45 */5 * * * *";
      quickCheck.func = [this]() {this->RefreshCache(false); };

      auto& fullUpdate = tasks.emplace_back();
      fullUpdate.name = std::format("PlexApi({}) - Data Full Update", GetName());
      fullUpdate.cronExpression = "0 48 3 * * *";
      fullUpdate.func = [this]() {this->RefreshCache(true); };

      return tasks;
   }

   std::string_view PlexApi::GetApiBase() const
   {
      return API_BASE;
   }

   std::string_view PlexApi::GetApiTokenName() const
   {
      return "";
   }

   bool PlexApi::GetValid()
   {
      auto res = Get(BuildApiPath(API_SERVERS), headers_);
      return res.error == Error::Success && res.status < VALID_HTTP_RESPONSE_MAX;
   }

   std::string_view PlexApi::GetMediaPath() const
   {
      return mediaPath_;
   }

   std::optional<PlexSearchResults> PlexApi::SearchItem(std::string_view name)
   {
      const auto apiPath = BuildApiParamsPath(API_SEARCH, {
         {"query", name}
      });

      auto res = Get(apiPath, headers_);
      if (!IsHttpSuccess(__func__, res)) return std::nullopt;

      JsonPlexResponse<JsonPlexSearchResult> serverResponse;
      if (auto ec = glz::read < glz::opts{.error_on_unknown_keys = false} > (serverResponse, res.body))
      {
         LogWarning("{} - JSON Parse Error: {}",
                    __func__, glz::format_error(ec, res.body));
         return std::nullopt;
      }

      PlexSearchResults returnResults;

      for (auto& hub : serverResponse.response.hub)
      {
         if (hub.data.size() == 0 || (hub.type != "movie" && hub.type != "episode")) continue;

         auto& hubData = hub.data[0];

         if (hubData.title != name) continue;

         auto& item = returnResults.items.emplace_back();

         item.libraryName = std::move(hubData.library);

         if (hubData.showTitle)
         {
            item.title = std::move(*hubData.showTitle);
            item.title += " - ";
            item.title += hubData.title;
         }
         else
         {
            item.title = std::move(hubData.title);
         }

         item.ratingKey = hubData.ratingKey;
         item.durationMs = hubData.duration;
         item.watched = hubData.viewCount && !hubData.viewOffset;

         if (item.watched)
         {
            item.playbackPercentage = 100;
         }
         else if (item.durationMs > 0 && hubData.viewOffset)
         {
            // std::lround handles the floating point conversion safely
            item.playbackPercentage = std::lround((*hubData.viewOffset * 100.0) / item.durationMs);
         }
         else
         {
            item.playbackPercentage = 0;
         }

         for (auto& media : hubData.media)
         {
            for (auto& part : media.part)
            {
               item.paths.emplace_back(std::move(part.file));
            }
         }
      }

      return returnResults;
   }

   std::optional<std::string> PlexApi::GetServerReportedName()
   {
      auto res = Get(BuildApiPath(API_SERVERS), headers_);
      if (!IsHttpSuccess(__func__, res)) return std::nullopt;

      JsonPlexResponse<JsonPlexServerData> serverResponse;
      if (auto ec = glz::read < glz::opts{.error_on_unknown_keys = false} > (serverResponse, res.body))
      {
         LogWarning("{} - JSON Parse Error: {}",
                    __func__, glz::format_error(ec, res.body));
         return std::nullopt;
      }

      if (serverResponse.response.data.size() == 0)
      {
         LogWarning("{} - No server names reported", __func__);
         return std::nullopt;
      }

      // Return the first name
      return serverResponse.response.data[0].name;
   }

   std::optional<std::string> PlexApi::GetLibraryId(std::string_view libraryName) const
   {
      std::shared_lock lock(dataLock_);
      auto iter = libraries_.find(libraryName);
      return iter == libraries_.end() ? std::nullopt : std::make_optional(iter->second);
   }

   std::optional<PlexSearchResults> PlexApi::GetItemInfo(std::string_view name)
   {
      return SearchItem(name);
   }

   std::unordered_map<std::string, std::string> PlexApi::GetItemsPaths(const std::vector<std::string>& ids)
   {
      std::string path{API_LIBRARY_DATA};
      path += BuildCommaSeparatedList(ids);

      auto res = Get(BuildApiPath(path), headers_);
      if (!IsHttpSuccess(__func__, res)) return {};

      JsonPlexResponse<JsonPlexMetadataContainer> serverResponse;
      if (auto ec = glz::read < glz::opts{.error_on_unknown_keys = false} > (serverResponse, res.body))
      {
         LogWarning("{} - JSON Parse Error: {}",
                    __func__, glz::format_error(ec, res.body));
         return {};
      }

      if (serverResponse.response.data.size() == 0) return {};

      std::unordered_map<std::string, std::string> results;
      results.reserve(ids.size());

      for (auto& data : serverResponse.response.data)
      {
         if (!data.ratingKey.empty()
             && data.media.size() > 0
             && data.media[0].part.size() > 0
             && !data.media[0].part[0].file.empty())
         {
            results.emplace(data.ratingKey, std::move(data.media[0].part[0].file));
         }
      }

      return results;
   }

   void PlexApi::SetLibraryScan(std::string_view libraryId)
   {
      auto apiPath = BuildApiPath(std::format("{}{}/refresh", API_LIBRARIES, libraryId));
      auto res = Get(apiPath, headers_);
      IsHttpSuccess(__func__, res);
   }

   std::string PlexApi::GetCollectionKey(std::string_view library, std::string_view collection)
   {
      if (!enableExtraCache_)
      {
         LogWarning("{} called but extra cache not enabled", __func__);
         return {};
      }

      std::shared_lock lock(dataLock_);

      auto libraryId = GetLibraryId(library);
      if (!libraryId) return {}; // Returns a "null" node

      auto iter = collections_.find(*libraryId);
      if (iter == collections_.end()) return {};

      auto subIter = iter->second.find(collection);
      if (subIter == iter->second.end()) return {};

      return subIter->second;
   }

   bool PlexApi::GetCollectionValid(std::string_view library, std::string_view collection)
   {
      return !GetCollectionKey(library, collection).empty();
   }

   std::optional<PlexCollection> PlexApi::GetCollection(std::string_view library, std::string_view collectionName)
   {
      auto collectionPath = GetCollectionKey(library, collectionName);
      if (collectionPath.empty()) return std::nullopt;

      auto res = Get(BuildApiPath(collectionPath), headers_);
      if (!IsHttpSuccess(__func__, res)) return std::nullopt;

      JsonPlexResponse<JsonPlexCollectionResult> serverResponse;
      if (auto ec = glz::read < glz::opts{.error_on_unknown_keys = false} > (serverResponse, res.body))
      {
         LogWarning("{} - JSON Parse Error: {}",
                    __func__, glz::format_error(ec, res.body));
         return {};
      }

      if (serverResponse.response.data.size() == 0) return std::nullopt;

      PlexCollection collection;
      collection.name = collectionName;

      for (auto& data : serverResponse.response.data)
      {
         if (data.media.size() == 0) continue;

         auto& item = collection.items.emplace_back();
         item.title = data.title;

         for (auto& media : data.media)
         {
            for (auto& part : media.part)
            {
               item.paths.emplace_back(std::move(part.file));
            }
         }
      }

      return collection;
   }

   bool PlexApi::SetPlayed(std::string_view ratingKey, int64_t locationMs)
   {
      const auto apiPath = BuildApiParamsPath("/:/progress", {
         {"identifier", "com.plexapp.plugins.library"},
         {"key", ratingKey},
         {"time", std::format("{}", locationMs)},
         {"state", "stopped"} // 'stopped' commits the time to the database
      });

      auto res = Get(apiPath, headers_);
      if (!IsHttpSuccess(__func__, res))
      {
         auto d = std::chrono::milliseconds(locationMs);
         std::chrono::hh_mm_ss timeSplit{std::chrono::duration_cast<std::chrono::seconds>(d)};
         LogError("{} - Failed to mark {} to play location {}:{}:{}",
                  __func__,
                  warp::GetTag("ratingKey", ratingKey),
                  timeSplit.hours().count(),
                  timeSplit.minutes().count(),
                  timeSplit.seconds().count());
         return false;
      }

      return true;
   }

   bool PlexApi::SetWatched(std::string_view ratingKey)
   {
      const auto apiPath = BuildApiParamsPath("/:/scrobble", {
         {"identifier", "com.plexapp.plugins.library"},
         {"key", ratingKey}
      });

      auto res = Get(apiPath, headers_);
      if (!IsHttpSuccess(__func__, res))
      {
         LogError("{} - Failed to mark {} as watched", __func__, warp::GetTag("ratingKey", ratingKey));
         return false;
      }

      return true;
   }

   void PlexApi::RebuildLibraryMap()
   {
      auto res = Get(BuildApiPath(API_LIBRARIES), headers_);
      if (!IsHttpSuccess(__func__, res)) return;

      JsonPlexResponse<JsonPlexLibraryResult> serverResponse;
      if (auto ec = glz::read < glz::opts{.error_on_unknown_keys = false} > (serverResponse, res.body))
      {
         LogWarning("{} - JSON Parse Error: {}",
                    __func__, glz::format_error(ec, res.body));
         return;
      }

      PlexNameToIdMap newMap;
      newMap.reserve(serverResponse.response.libraries.size());
      for (auto& library : serverResponse.response.libraries)
      {
         newMap.emplace(std::move(library.title), std::move(library.id));
      }

      std::unique_lock lock(dataLock_);
      libraries_ = std::move(newMap);
   }

   void PlexApi::RebuildCollectionMap()
   {
      PlexIdToIdMap newMap;

      std::vector<std::string> libraryIds;
      {
         std::shared_lock sharedLock(dataLock_);
         libraryIds.reserve(libraries_.size());
         for (const auto& [name, id] : libraries_)
         {
            libraryIds.emplace_back(id);
         }
      }

      bool sucessfulCollectionGet = false;
      for (const auto& id : libraryIds)
      {
         std::string apiPath = BuildApiParamsPath(std::format("{}{}/all", API_LIBRARIES, id), {
            {"type", std::format("{}", static_cast<int>(plex_search_collection))}
         });

         auto res = Get(apiPath, headers_);
         if (!IsHttpSuccess(__func__, res)) continue;

         JsonPlexResponse<JsonPlexCollectionResult> serverResponse;
         if (auto ec = glz::read < glz::opts{.error_on_unknown_keys = false} > (serverResponse, res.body))
         {
            LogWarning("{} - JSON Parse Error: {}",
                       __func__, glz::format_error(ec, res.body));
            continue;
         }

         // Received valid collections
         sucessfulCollectionGet = true;

         PlexNameToIdMap nameToIdMap;
         for (auto& item : serverResponse.response.data)
         {
            nameToIdMap.emplace(std::move(item.title), std::move(item.key));
         }

         newMap.emplace(id, std::move(nameToIdMap));
      }

      if (sucessfulCollectionGet)
      {
         std::unique_lock lock(dataLock_);
         collections_ = std::move(newMap);
      }
      else
      {
         LogWarning("{} - Keeping stale collection data due to fetch failures", __func__);
      }
   }

   void PlexApi::UpdateRequiredCache(bool forceRefresh)
   {
      bool refreshLibraries = false;
      {
         std::shared_lock lock(dataLock_);
         if (forceRefresh || libraries_.empty()) refreshLibraries = true;
      }
      if (refreshLibraries) RebuildLibraryMap();
   }

   void PlexApi::UpdateExtraCache(bool forceRefresh)
   {
      bool refreshCollections = false;

      // Scope around the lock
      {
         std::shared_lock lock(dataLock_);
         if (forceRefresh || collections_.empty()) refreshCollections = true;
      }
      if (refreshCollections) RebuildCollectionMap();
   }

   void PlexApi::RefreshCache(bool forceRefresh)
   {
      UpdateRequiredCache(forceRefresh);
      if (enableExtraCache_) UpdateExtraCache(forceRefresh);
   }
}