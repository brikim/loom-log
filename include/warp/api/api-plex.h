#pragma once

#include "warp/api/api-base.h"
#include "warp/api/api-plex-types.h"

#include <httplib.h>

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
      PlexApi(std::string_view version, const ServerConfig& serverConfig);
      virtual ~PlexApi() = default;

      [[nodiscard]] std::optional<std::vector<ApiTask>> GetTaskList() override;

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
      void BuildData(bool forceRefresh);

      // Returns the collection api path
      std::string GetCollectionKey(std::string_view library, std::string_view collection);

      std::optional<PlexSearchResults> SearchItem(std::string_view name);

      httplib::Headers headers_;

      std::string mediaPath_;

      struct string_hash
      {
         using is_transparent = void; // This enables heterogeneous lookup
         size_t operator()(std::string_view sv) const
         {
            return std::hash<std::string_view>{}(sv);
         }
         size_t operator()(const std::string& s) const
         {
            return std::hash<std::string>{}(s);
         }
      };

      mutable std::shared_mutex dataLock_;

      using PlexNameToIdMap = std::unordered_map<std::string, std::string, string_hash, std::equal_to<>>;
      PlexNameToIdMap libraries_;

      using PlexIdToIdMap = std::unordered_map<std::string, PlexNameToIdMap, string_hash, std::equal_to<>>;
      PlexIdToIdMap collections_;
   };
}