#include "warp/api/api-plex.h"

#include "api/api-plex-json-types.h"
#include "api/api-utils.h"
#include "warp/log/log-utils.h"
#include "warp/types.h"
#include "warp/utils.h"

#include <glaze/glaze.hpp>
#include <glaze/thread/threadpool.hpp>

#include <charconv>
#include <cmath>
#include <format>
#include <mutex>
#include <ranges>
#include <shared_mutex>

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

      constexpr size_t THREAD_POOL_SIZE{1};
   }

   struct PlexApiImpl
   {
      PlexApi& parent_;
      Headers headers_;

      std::string mediaPath_;
      bool enableExtraCache_{false};

      mutable std::shared_mutex dataLock;

      using PlexNameToIdMap = std::unordered_map<std::string, std::string, StringHash, std::equal_to<>>;
      PlexNameToIdMap libraries_;
      PlexNameToIdMap workingLibraries_;

      using PlexIdToIdMap = std::unordered_map<std::string, PlexNameToIdMap, StringHash, std::equal_to<>>;
      PlexIdToIdMap collections_;
      PlexIdToIdMap workingCollections_;

      std::unique_ptr<glz::pool> threadPool_;

      PlexApiImpl(PlexApi& p, std::string_view appName, std::string_view version, const ServerConfig& serverConfig)
         : parent_(p)
         , mediaPath_(serverConfig.media_path)
         , threadPool_(std::make_unique<glz::pool>(THREAD_POOL_SIZE))
      {
         headers_ = {
            {"X-Plex-Token", serverConfig.api_key},
            {"X-Plex-Client-Identifier", "6e7417e2-8d76-4b1f-9c23-018274959a37"},
            {"Accept", "application/json"},
            {"User-Agent", std::format("{}/{}", appName, version)}
         };

         UpdateRequiredCache(true);
      }

      void EnableExtraCaching()
      {
         enableExtraCache_ = true;
         UpdateExtraCache(true);
      }

      void Shutdown()
      {
         if (threadPool_)
         {
            threadPool_->wait();
            threadPool_.reset();
         }
      }

      void RebuildLibraryMap()
      {
         parent_.LogTrace("Rebuilding Library Map");

         auto res = parent_.Get(parent_.BuildApiPath(API_LIBRARIES), headers_);
         if (!parent_.IsHttpSuccess(__func__, res)) return;

         JsonPlexResponse<JsonPlexLibraryResult> serverResponse;
         if (auto ec = glz::read < glz::opts{.error_on_unknown_keys = false} > (serverResponse, res.body))
         {
            parent_.LogWarning("{} - JSON Parse Error: {}",
                              __func__, glz::format_error(ec, res.body));
            return;
         }

         workingLibraries_.reserve(serverResponse.response.libraries.size());
         for (auto& library : serverResponse.response.libraries)
         {
            workingLibraries_.emplace(std::move(library.title), std::move(library.id));
         }

         if (!workingLibraries_.empty())
         {
            std::unique_lock lock(dataLock);
            std::swap(workingLibraries_, libraries_);
            workingLibraries_.clear();
         }
         else
         {
            parent_.LogWarning("{} - Keeping stale library data due to fetch failures", __func__);
         }
      }

      void RebuildCollectionMap()
      {
         parent_.LogTrace("Rebuilding Collection Map");

         std::vector<std::string> libraryIds;
         {
            std::shared_lock sharedLock(dataLock);
            libraryIds.reserve(libraries_.size());
            for (const auto& [name, id] : libraries_)
            {
               libraryIds.emplace_back(id);
            }
         }

         workingCollections_.reserve(libraryIds.size());

         bool sucessfulCollectionGet = false;
         for (const auto& id : libraryIds)
         {
            std::string apiPath = parent_.BuildApiParamsPath(std::format("{}{}/all", API_LIBRARIES, id), {
               {"type", std::format("{}", static_cast<int>(plex_search_collection))}
            });

            auto res = parent_.Get(apiPath, headers_);
            if (!parent_.IsHttpSuccess(__func__, res)) continue;

            JsonPlexResponse<JsonPlexCollectionResult> serverResponse;
            if (auto ec = glz::read < glz::opts{.error_on_unknown_keys = false} > (serverResponse, res.body))
            {
               parent_.LogWarning("{} - JSON Parse Error: {}",
                                 __func__, glz::format_error(ec, res.body));
               continue;
            }

            // Received valid collections_
            sucessfulCollectionGet = true;

            PlexNameToIdMap nameToIdMap;
            nameToIdMap.reserve(serverResponse.response.data.size());

            for (auto& item : serverResponse.response.data)
            {
               nameToIdMap.emplace(std::move(item.title), std::move(item.key));
            }

            workingCollections_.emplace(id, std::move(nameToIdMap));
         }

         if (sucessfulCollectionGet)
         {
            std::unique_lock lock(dataLock);
            std::swap(workingCollections_, collections_);
            workingCollections_.clear();
         }
         else
         {
            parent_.LogWarning("{} - Keeping stale collection data due to fetch failures", __func__);
         }
      }

      void UpdateRequiredCache(bool forceRefresh)
      {
         bool refreshLibraries = false;
         {
            std::unique_lock lock(dataLock);
            if (forceRefresh || libraries_.empty()) refreshLibraries = true;
         }
         if (refreshLibraries) threadPool_->emplace_back([this]() { RebuildLibraryMap(); });
      }

      void UpdateExtraCache(bool forceRefresh)
      {
         bool refreshCollections = false;

         // Scope around the lock
         {
            std::unique_lock lock(dataLock);
            if (forceRefresh || collections_.empty()) refreshCollections = true;
         }
         if (refreshCollections) threadPool_->emplace_back([this]() { RebuildCollectionMap(); });
      }

      void RefreshCache(bool forceRefresh)
      {
         UpdateRequiredCache(forceRefresh);
         if (enableExtraCache_) UpdateExtraCache(forceRefresh);
      }

      std::optional<std::string> GetLibraryId(std::string_view libraryName) const
      {
         std::shared_lock lock(dataLock);
         auto iter = libraries_.find(libraryName);
         return iter == libraries_.end() ? std::nullopt : std::make_optional(iter->second);
      }

      // Returns the collection api path
      std::string GetCollectionKey(std::string_view library, std::string_view collection)
      {
         if (!enableExtraCache_)
         {
            parent_.LogWarning("{} called but extra cache not enabled", __func__);
            return {};
         }

         std::shared_lock lock(dataLock);

         auto libraryId = GetLibraryId(library);
         if (!libraryId) return {}; // Returns a "null" node

         auto iter = collections_.find(*libraryId);
         if (iter == collections_.end()) return {};

         auto subIter = iter->second.find(collection);
         if (subIter == iter->second.end()) return {};

         return subIter->second;
      }

      std::optional<PlexSearchResults> SearchItem(std::string_view name)
      {
         const auto apiPath = parent_.BuildApiParamsPath(API_SEARCH, {
            {"query", name}
         });

         auto res = parent_.Get(apiPath, headers_);
         if (!parent_.IsHttpSuccess(__func__, res)) return std::nullopt;

         JsonPlexResponse<JsonPlexSearchResult> serverResponse;
         if (auto ec = glz::read < glz::opts{.error_on_unknown_keys = false} > (serverResponse, res.body))
         {
            parent_.LogWarning("{} - JSON Parse Error: {}",
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
   };

   PlexApi::PlexApi(std::string_view appName, std::string_view version, const ServerConfig& serverConfig)
      : ApiBase(ApiBaseData{.name = serverConfig.server_name,
                .url = serverConfig.url,
                .apiKey = serverConfig.api_key,
                .className = "PlexApi",
                .ansiiCode = ANSI_CODE_PLEX,
                .prettyName = GetServerName(GetFormattedPlex(), serverConfig.server_name)})
      , pimpl_(std::make_unique<PlexApiImpl>(*this, appName, version, serverConfig))
   {
   }

   PlexApi::~PlexApi() = default;

   void PlexApi::EnableExtraCaching()
   {
      pimpl_->EnableExtraCaching();
   }

   void PlexApi::Shutdown()
   {
      pimpl_->Shutdown();
   }

   std::optional<std::vector<Task>> PlexApi::GetTaskList()
   {
      std::vector<Task> tasks;

      auto& quickCheck = tasks.emplace_back();
      quickCheck.name = std::format("PlexApi({}) - Refresh Cache Quick", GetName());
      quickCheck.cronExpression = GetNextCronQuickTime();
      quickCheck.func = [this]() {pimpl_->RefreshCache(false); };

      auto& fullUpdate = tasks.emplace_back();
      fullUpdate.name = std::format("PlexApi({}) - Refresh Cache Full", GetName());
      fullUpdate.cronExpression = GetNextCronFullTime();
      fullUpdate.func = [this]() {pimpl_->RefreshCache(true); };

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
      auto res = Get(BuildApiPath(API_SERVERS), pimpl_->headers_);
      return res.error == Error::Success && res.status < VALID_HTTP_RESPONSE_MAX;
   }

   std::string_view PlexApi::GetMediaPath() const
   {
      return pimpl_->mediaPath_;
   }

   std::optional<std::string> PlexApi::GetServerReportedName()
   {
      auto res = Get(BuildApiPath(API_SERVERS), pimpl_->headers_);
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
      return pimpl_->GetLibraryId(libraryName);
   }

   std::optional<PlexSearchResults> PlexApi::GetItemInfo(std::string_view name)
   {
      return pimpl_->SearchItem(name);
   }

   std::unordered_map<std::string, std::string> PlexApi::GetItemsPaths(const std::vector<std::string>& ids)
   {
      std::string path{API_LIBRARY_DATA};
      path += BuildCommaSeparatedList(ids);

      auto res = Get(BuildApiPath(path), pimpl_->headers_);
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
      auto res = Get(apiPath, pimpl_->headers_);
      IsHttpSuccess(__func__, res);
   }

   bool PlexApi::GetCollectionValid(std::string_view library, std::string_view collection)
   {
      return !pimpl_->GetCollectionKey(library, collection).empty();
   }

   std::optional<PlexCollection> PlexApi::GetCollection(std::string_view library, std::string_view collectionName)
   {
      auto collectionPath = pimpl_->GetCollectionKey(library, collectionName);
      if (collectionPath.empty()) return std::nullopt;

      auto res = Get(BuildApiPath(collectionPath), pimpl_->headers_);
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

      auto res = Get(apiPath, pimpl_->headers_);
      if (!IsHttpSuccess(__func__, res))
      {
         auto d = std::chrono::milliseconds(locationMs);
         std::chrono::hh_mm_ss timeSplit{std::chrono::duration_cast<std::chrono::seconds>(d)};
         LogError("{} - Failed to mark {} to play location {}:{}:{}",
                  __func__,
                  GetTag("ratingKey", ratingKey),
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

      auto res = Get(apiPath, pimpl_->headers_);
      if (!IsHttpSuccess(__func__, res))
      {
         LogError("{} - Failed to mark {} as watched", __func__, GetTag("ratingKey", ratingKey));
         return false;
      }

      return true;
   }
}