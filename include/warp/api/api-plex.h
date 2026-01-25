#pragma once

#include "warp/api/api-base.h"
#include "warp/api/api-plex-types.h"
#include "warp/api/api-types.h"
#include "warp/types.h"

#include <cstdint>
#include <list>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>

namespace warp
{
   class PlexApi : public ApiBase
   {
   public:
      PlexApi(std::string_view appName, std::string_view version, const ServerConfig& serverConfig);
      virtual ~PlexApi() = default;

      void EnableExtraCaching();

      [[nodiscard]] std::optional<std::vector<Task>> GetTaskList() override;

      // Returns true if the server is reachable and the API key is valid
      [[nodiscard]] bool GetValid() override;
      [[nodiscard]] std::string_view GetMediaPath() const;
      [[nodiscard]] std::optional<std::string> GetServerReportedName() override;
      [[nodiscard]] std::optional<std::string> GetLibraryId(std::string_view libraryName) const;
      [[nodiscard]] std::optional<PlexSearchResults> GetItemInfo(std::string_view name);
      [[nodiscard]] std::unordered_map<std::string, std::string> GetItemsPaths(const std::vector<std::string>& ids);

      // Returns if the collection in the library is valid on this server
      [[nodiscard]] bool GetCollectionValid(std::string_view library, std::string_view collection);

      // Returns the collection information for the collection in the library
      [[nodiscard]] std::optional<PlexCollection> GetCollection(std::string_view library, std::string_view collectionName);

      // Tell Plex to scan the passed in library
      void SetLibraryScan(std::string_view libraryId);

      bool SetPlayed(std::string_view ratingKey, int64_t locationMs);
      bool SetWatched(std::string_view ratingKey);

   private:
      std::string_view GetApiBase() const override;
      std::string_view GetApiTokenName() const override;

      void RebuildLibraryMap();
      void RebuildCollectionMap();
      void UpdateRequiredCache(bool forceRefresh);
      void UpdateExtraCache(bool forceRefresh);
      void RefreshCache(bool forceRefresh);

      // Returns the collection api path
      std::string GetCollectionKey(std::string_view library, std::string_view collection);

      std::optional<PlexSearchResults> SearchItem(std::string_view name);

      Headers headers_;

      std::string mediaPath_;
      bool enableExtraCache_{false};

      mutable std::shared_mutex dataLock_;

      using PlexNameToIdMap = std::unordered_map<std::string, std::string, StringHash, std::equal_to<>>;
      PlexNameToIdMap libraries_;

      using PlexIdToIdMap = std::unordered_map<std::string, PlexNameToIdMap, StringHash, std::equal_to<>>;
      PlexIdToIdMap collections_;
   };
}