#include "warp/api/api-plex.h"

#include "api/api-plex-json-types.h"
#include "api/api-utils.h"
#include "warp/log/log-utils.h"
#include "warp/types.h"
#include "warp/utils.h"

#include <glaze/glaze.hpp>
#include <httplib.h>
#include <pugixml.hpp>

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
      const std::string PLEX_RESOURCE_URL{"https://plex.tv"};

      constexpr std::string_view API_BASE{""};
      const std::string API_TOKEN_NAME{"X-Plex-Token"};

      const std::string API_PLEXTV_RESOURCES{"/api/resources"};
      const std::string API_PLEXTV_ADMIN_USER{"/api/v2/user"};
      const std::string API_PLEXTV_SERVERS("/api/servers");

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

   struct PlexApi::PlexApiImpl
   {
      httplib::Client plexTvClient_;
      httplib::Headers plexTvHeaders_;

      PlexApi& parent_;
      Headers headersNoToken_;
      Headers adminHeaders_;

      std::filesystem::path mediaPath_;
      bool enableUserTokens_{false};
      bool enableCacheCollections_{false};

      mutable std::shared_mutex dataLock_;

      using PlexNameToIdMap = std::unordered_map<std::string, std::string, StringHash, std::equal_to<>>;
      PlexNameToIdMap libraries_;
      PlexNameToIdMap workingLibraries_;

      PlexNameToIdMap userTokens_;
      PlexNameToIdMap workingUserTokens_;

      using PlexIdToIdMap = std::unordered_map<std::string, PlexNameToIdMap, StringHash, std::equal_to<>>;
      PlexIdToIdMap collections_;
      PlexIdToIdMap workingCollections_;

      PlexApiImpl(PlexApi& p, std::string_view appName, std::string_view version, const ServerConfig& serverConfig);

      void EnableUserTokens();
      void EnableCacheCollections();

      void RebuildLibraryMap();
      void RebuildUserTokenMap();
      void RebuildCollectionMap();

      void UpdateCacheRequired(bool forceRefresh);
      void UpdateUserTokens(bool forceRefresh);
      void UpdateCacheCollections(bool forceRefresh);
      void RefreshCache(bool forceRefresh);

      std::optional<std::string> GetLibraryId(std::string_view libraryName) const;

      // Returns the collection api path
      std::string GetCollectionKey(std::string_view library, std::string_view collection);

      std::optional<PlexSearchResults> SearchItem(std::string_view userName, std::string_view itemName);
   };

   PlexApi::PlexApiImpl::PlexApiImpl(PlexApi& p, std::string_view appName, std::string_view version, const ServerConfig& serverConfig)
      : plexTvClient_(PLEX_RESOURCE_URL)
      , parent_(p)
      , mediaPath_(serverConfig.mediaPath)
   {
      auto baseHeaders = Headers{
         {"X-Plex-Client-Identifier", "6e7417e2-8d76-4b1f-9c23-018274959a37"},
         {"User-Agent", std::format("{}/{}", appName, version)}
      };

      headersNoToken_ = baseHeaders;
      headersNoToken_.emplace("Accept", APPLICATION_JSON);

      adminHeaders_ = headersNoToken_;
      adminHeaders_.emplace("Accept", APPLICATION_JSON);
      adminHeaders_.emplace(API_TOKEN_NAME, parent_.GetApiKey());

      // Copy the default headers into the httplib version
      std::ranges::for_each(baseHeaders, [this](auto& header) {
         plexTvHeaders_.emplace(header.first, header.second);
      });
      plexTvHeaders_.emplace(API_TOKEN_NAME, parent_.GetApiKey());
      plexTvHeaders_.emplace("Accept", "application/xml");

      UpdateCacheRequired(true);
   }

   PlexApi::PlexApi(std::string_view appName,
                    std::string_view version,
                    const ServerConfig& serverConfig,
                    const ServerPlexOptions& options)
      : ApiBase(ApiBaseData{.name = serverConfig.serverName,
                .url = serverConfig.url,
                .apiKey = serverConfig.apiKey,
                .className = "PlexApi",
                .ansiiCode = ANSI_CODE_PLEX,
                .prettyName = GetServerName(GetFormattedPlex(), serverConfig.serverName)})
      , pimpl_(std::make_unique<PlexApiImpl>(*this, appName, version, serverConfig))
   {
      if (options.enableUserTokens) pimpl_->EnableUserTokens();
      if (options.enableCacheCollection) pimpl_->EnableCacheCollections();
   }

   PlexApi::~PlexApi() = default;

   void PlexApi::PlexApiImpl::EnableUserTokens()
   {
      enableUserTokens_ = true;
      UpdateUserTokens(true);
   }

   void PlexApi::PlexApiImpl::EnableCacheCollections()
   {
      enableCacheCollections_ = true;
      UpdateCacheCollections(true);
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
      auto res = Get(BuildApiPath(API_SERVERS), pimpl_->adminHeaders_);
      return res.error == Error::Success && res.status < VALID_HTTP_RESPONSE_MAX;
   }

   const std::filesystem::path& PlexApi::GetMediaPath() const
   {
      return pimpl_->mediaPath_;
   }

   std::optional<std::string> PlexApi::GetServerReportedName()
   {
      auto res = Get(BuildApiPath(API_SERVERS), pimpl_->adminHeaders_);
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

   std::optional<std::string> PlexApi::PlexApiImpl::GetLibraryId(std::string_view libraryName) const
   {
      std::shared_lock lock(dataLock_);
      auto iter = libraries_.find(libraryName);
      return iter == libraries_.end() ? std::nullopt : std::make_optional(iter->second);
   }

   std::optional<std::string> PlexApi::GetLibraryId(std::string_view libraryName) const
   {
      return pimpl_->GetLibraryId(libraryName);
   }

   std::optional<PlexSearchResults> PlexApi::PlexApiImpl::SearchItem(std::string_view userName, std::string_view itemName)
   {
      std::string tokenToUse;

      // Scope around the lock
      {
         std::shared_lock sharedLock(dataLock_);
         auto iter = userTokens_.find(userName);
         if (iter != userTokens_.end())
            tokenToUse = iter->second;
      }

      if (tokenToUse.empty())
      {
         parent_.LogWarning("{} - No token found for user {}",
                    __func__, GetTag("userName", userName));
         return std::nullopt;
      }

      const auto apiPath = parent_.BuildApiParamsPath(API_SEARCH, {
         {"query", itemName}
      });

      auto headersToUse = headersNoToken_;
      headersToUse.emplace(API_TOKEN_NAME, tokenToUse);

      auto res = parent_.Get(apiPath, headersToUse);
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

         if (hubData.title != itemName) continue;

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

   std::optional<PlexSearchResults> PlexApi::GetItemInfo(std::string_view userName, std::string_view itemName)
   {
      return pimpl_->SearchItem(userName, itemName);
   }

   std::optional<std::filesystem::path> PlexApi::GetItemPath(std::string_view id)
   {
      auto path = std::format("{}{}", API_LIBRARY_DATA, id);
      auto res = Get(BuildApiPath(path), pimpl_->adminHeaders_);
      if (!IsHttpSuccess(__func__, res)) return std::nullopt;

      JsonPlexResponse<JsonPlexMetadataContainer> serverResponse;
      if (auto ec = glz::read < glz::opts{.error_on_unknown_keys = false} > (serverResponse, res.body))
      {
         LogWarning("{} - JSON Parse Error: {}",
                    __func__, glz::format_error(ec, res.body));
         return std::nullopt;
      }

      if (serverResponse.response.data.size() == 0) return std::nullopt;

      for (auto& data : serverResponse.response.data)
      {
         if (data.media.size() > 0
             && data.media[0].part.size() > 0
             && !data.media[0].part[0].file.empty())
         {
            return std::move(data.media[0].part[0].file);
         }
      }

      return std::nullopt;
   }

   std::unordered_map<std::string, std::filesystem::path> PlexApi::GetItemsPaths(const std::vector<std::string>& ids)
   {
      std::string apiPath{API_LIBRARY_DATA};
      apiPath += BuildCommaSeparatedList(ids);

      auto res = Get(BuildApiPath(apiPath), pimpl_->adminHeaders_);
      if (!IsHttpSuccess(__func__, res)) return {};

      JsonPlexResponse<JsonPlexMetadataContainer> serverResponse;
      if (auto ec = glz::read < glz::opts{.error_on_unknown_keys = false} > (serverResponse, res.body))
      {
         LogWarning("{} - JSON Parse Error: {}",
                    __func__, glz::format_error(ec, res.body));
         return {};
      }

      if (serverResponse.response.data.size() == 0) return {};

      std::unordered_map<std::string, std::filesystem::path> results;
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
      auto res = Get(apiPath, pimpl_->adminHeaders_);
      IsHttpSuccess(__func__, res);
   }

   void PlexApi::SetLibraryScanPath(std::string_view libraryId, const std::filesystem::path& path)
   {
      auto apiPath = BuildApiParamsPath(std::format("{}{}/refresh", API_LIBRARIES, libraryId), {
         { "path", path.generic_string() }
      });
      auto res = Get(apiPath, pimpl_->adminHeaders_);
      IsHttpSuccess(__func__, res);
   }

   std::string PlexApi::PlexApiImpl::GetCollectionKey(std::string_view library, std::string_view collection)
   {
      if (!enableCacheCollections_)
      {
         parent_.LogWarning("{} called but extra cache not enabled", __func__);
         return {};
      }

      std::shared_lock lock(dataLock_);

      auto libIter = libraries_.find(library);
      if (libIter == libraries_.end()) return {};

      auto iter = collections_.find(libIter->second);
      if (iter == collections_.end()) return {};

      auto subIter = iter->second.find(collection);
      if (subIter == iter->second.end()) return {};

      return subIter->second;
   }

   bool PlexApi::GetCollectionValid(std::string_view library, std::string_view collection)
   {
      return !pimpl_->GetCollectionKey(library, collection).empty();
   }

   std::optional<PlexCollection> PlexApi::GetCollection(std::string_view library, std::string_view collectionName)
   {
      auto collectionPath = pimpl_->GetCollectionKey(library, collectionName);
      if (collectionPath.empty()) return std::nullopt;

      auto res = Get(BuildApiPath(collectionPath), pimpl_->adminHeaders_);
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

   bool PlexApi::SetPlayed(std::string_view userName, std::string_view ratingKey, int64_t locationMs)
   {
      std::string tokenToUse;

      // Scope around the lock
      {
         std::shared_lock sharedLock(pimpl_->dataLock_);
         auto iter = pimpl_->userTokens_.find(userName);
         if (iter != pimpl_->userTokens_.end())
            tokenToUse = iter->second;
      }

      if (tokenToUse.empty())
         return false;

      const auto apiPath = BuildApiParamsPath("/:/progress", {
         {"identifier", "com.plexapp.plugins.library"},
         {"key", ratingKey},
         {"time", std::format("{}", locationMs)},
         {"state", "stopped"} // 'stopped' commits the time to the database
      });

      auto headersToUse = pimpl_->headersNoToken_;
      headersToUse.emplace(API_TOKEN_NAME, tokenToUse);

      auto res = Get(apiPath, headersToUse);
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

   bool PlexApi::SetWatched(std::string_view userName, std::string_view ratingKey)
   {
      std::string tokenToUse;

      // Scope around the lock
      {
         std::shared_lock sharedLock(pimpl_->dataLock_);
         auto iter = pimpl_->userTokens_.find(userName);
         if (iter != pimpl_->userTokens_.end())
            tokenToUse = iter->second;
      }

      if (tokenToUse.empty())
         return false;

      const auto apiPath = BuildApiParamsPath("/:/scrobble", {
         {"identifier", "com.plexapp.plugins.library"},
         {"key", ratingKey}
      });

      auto headersToUse = pimpl_->headersNoToken_;
      headersToUse.emplace(API_TOKEN_NAME, tokenToUse);

      auto res = Get(apiPath, headersToUse);
      if (!IsHttpSuccess(__func__, res))
      {
         LogError("{} - Failed to mark {} as watched", __func__, GetTag("ratingKey", ratingKey));
         return false;
      }

      return true;
   }

   void PlexApi::PlexApiImpl::RebuildLibraryMap()
   {
      parent_.LogTrace("Rebuilding Library Map");

      auto res = parent_.Get(parent_.BuildApiPath(API_LIBRARIES), adminHeaders_);
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
         std::unique_lock lock(dataLock_);
         std::swap(workingLibraries_, libraries_);
         workingLibraries_.clear();
      }
      else
      {
         parent_.LogWarning("{} - Keeping stale library data due to fetch failures", __func__);
      }
   }

   void PlexApi::PlexApiImpl::RebuildUserTokenMap()
   {
      auto checkHttpSuccess = [this](const httplib::Result& res, std::string_view function) {
         if (res.error() != httplib::Error::Success
          || res->status >= VALID_HTTP_RESPONSE_MAX)
         {
            if (res->status >= VALID_HTTP_RESPONSE_MAX)
            {
               parent_.LogWarning("{} - HTTP Error: {} {}", function, res->status, res->reason);
            }
            else
            {
               parent_.LogWarning("{} - Connection Error: {}", function, httplib::to_string(res.error()));
            }
            return false;
         }
         return true;
      };

      auto res = plexTvClient_.Get(API_PLEXTV_RESOURCES, plexTvHeaders_);
      if (checkHttpSuccess(res, __func__) == false) return;

      pugi::xml_document doc;
      auto result = doc.load_string(res->body.c_str());

      if (!result)
      {
         parent_.LogWarning("{} - Could not parse {} Plex XML return", __func__, API_PLEXTV_RESOURCES);
         return;
      }

      std::string clientId;
      auto container = doc.child("MediaContainer");
      for (auto device : container.children("Device"))
      {
         std::string currentName = device.attribute("name").value();
         if (currentName == parent_.GetName())
         {
            clientId = device.attribute("clientIdentifier").value();
            break;
         }
      }

      // If the client id was not found return we can't continue
      if (clientId.empty()) return;

      // Now using the client id fetch the shared server to retrieve user tokens
      auto serverRes = plexTvClient_.Get(std::format("{}/{}/shared_servers", API_PLEXTV_SERVERS, clientId), plexTvHeaders_);
      if (checkHttpSuccess(serverRes, __func__) == false) return;

      pugi::xml_document serverDoc;
      auto serverResult = serverDoc.load_string(serverRes->body.c_str());
      if (!serverResult)
      {
         parent_.LogWarning("{} - Could not parse {} Plex XML return", __func__, API_PLEXTV_SERVERS);
         return;
      }

      auto serverContainer = serverDoc.child("MediaContainer");
      workingUserTokens_.reserve(serverContainer.attribute("size").as_int());
      for (auto sharedServer : serverContainer.children("SharedServer"))
      {
         std::string username = sharedServer.attribute("username").value();
         std::string userToken = sharedServer.attribute("accessToken").value();
         if (!username.empty() && !userToken.empty())
         {
            workingUserTokens_.emplace(std::move(username), std::move(userToken));
         }
      }

      if (!workingUserTokens_.empty())
      {
         // Now fetch the admin user  to get the toke to add to the working set of tokens.
         auto adminUserRes = plexTvClient_.Get(API_PLEXTV_ADMIN_USER, plexTvHeaders_);
         if (checkHttpSuccess(adminUserRes, __func__) == false) return;

         pugi::xml_document adminUserDoc;
         auto adminUserResult = adminUserDoc.load_string(adminUserRes->body.c_str());
         if (!adminUserResult)
         {
            parent_.LogWarning("{} - Could not parse {} Plex XML return", __func__, API_PLEXTV_ADMIN_USER);
            return;
         }

         // The root node is <user>
         auto userNode = adminUserDoc.child("user");

         // Add the admin user to the working user tokens
         std::string username = userNode.attribute("username").value();
         std::string userToken = userNode.attribute("authToken").value();
         workingUserTokens_.emplace(std::move(username), std::move(userToken));


         std::unique_lock lock(dataLock_);
         std::swap(workingUserTokens_, userTokens_);
         workingUserTokens_.clear();
      }
      else
      {
         parent_.LogWarning("{} - Keeping stale User data due to fetch failures", __func__);
      }
   }

   void PlexApi::PlexApiImpl::RebuildCollectionMap()
   {
      parent_.LogTrace("Rebuilding Collection Map");

      std::vector<std::string> libraryIds;
      {
         std::shared_lock sharedLock(dataLock_);
         libraryIds.reserve(libraries_.size());
         for (const auto& [name, id] : libraries_)
         {
            libraryIds.emplace_back(id);
         }
      }

      workingCollections_.reserve(libraryIds.size());

      bool success = false;
      for (const auto& id : libraryIds)
      {
         std::string apiPath = parent_.BuildApiParamsPath(std::format("{}{}/all", API_LIBRARIES, id), {
            {"type", std::format("{}", static_cast<int>(plex_search_collection))}
         });

         auto res = parent_.Get(apiPath, adminHeaders_);
         if (!parent_.IsHttpSuccess(__func__, res)) continue;

         JsonPlexResponse<JsonPlexCollectionResult> serverResponse;
         if (auto ec = glz::read < glz::opts{.error_on_unknown_keys = false} > (serverResponse, res.body))
         {
            parent_.LogWarning("{} - JSON Parse Error: {}",
                              __func__, glz::format_error(ec, res.body));
            continue;
         }

         // Received valid collections_
         success = true;

         PlexNameToIdMap nameToIdMap;
         nameToIdMap.reserve(serverResponse.response.data.size());

         for (auto& item : serverResponse.response.data)
         {
            nameToIdMap.emplace(std::move(item.title), std::move(item.key));
         }

         workingCollections_.emplace(id, std::move(nameToIdMap));
      }

      if (success)
      {
         std::unique_lock lock(dataLock_);
         std::swap(workingCollections_, collections_);
         workingCollections_.clear();
      }
      else
      {
         parent_.LogWarning("{} - Keeping stale collection data due to fetch failures", __func__);
      }
   }

   void PlexApi::PlexApiImpl::UpdateCacheRequired(bool forceRefresh)
   {
      bool refreshLibraries = false;

      // Scope around the lock
      {
         std::unique_lock lock(dataLock_);
         refreshLibraries = forceRefresh || libraries_.empty();
      }

      if (refreshLibraries)
         RebuildLibraryMap();
   }

   void PlexApi::PlexApiImpl::UpdateUserTokens(bool forceRefresh)
   {
      bool refreshUserTokens = false;

      // Scope around the lock
      {
         std::unique_lock lock(dataLock_);
         refreshUserTokens = forceRefresh || userTokens_.empty();
      }

      if (refreshUserTokens)
         RebuildUserTokenMap();
   }

   void PlexApi::PlexApiImpl::UpdateCacheCollections(bool forceRefresh)
   {
      bool refreshCollections = false;

      // Scope around the lock
      {
         std::unique_lock lock(dataLock_);
         refreshCollections = forceRefresh || collections_.empty();
      }

      if (refreshCollections)
         RebuildCollectionMap();
   }

   void PlexApi::PlexApiImpl::RefreshCache(bool forceRefresh)
   {
      UpdateCacheRequired(forceRefresh);
      if (enableUserTokens_) UpdateUserTokens(forceRefresh);
      if (enableCacheCollections_) UpdateCacheCollections(forceRefresh);
   }
}