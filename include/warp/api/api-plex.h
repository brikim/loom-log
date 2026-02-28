#pragma once

#include "warp/api/api-base.h"
#include "warp/api/api-plex-types.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace warp
{
   class PlexApi : public ApiBase
   {
   public:
      PlexApi(std::string_view appName,
              std::string_view version,
              const ServerConfig& serverConfig,
              const ServerPlexOptions& options);
      virtual ~PlexApi();

      [[nodiscard]] std::optional<std::vector<Task>> GetTaskList() override;

      // Returns true if the server is reachable and the API key is valid
      [[nodiscard]] bool GetValid() override;
      [[nodiscard]] const std::filesystem::path& GetMediaPath() const;
      [[nodiscard]] std::optional<std::string> GetServerReportedName() override;
      [[nodiscard]] std::optional<std::string> GetLibraryId(std::string_view libraryName) const;

      [[nodiscard]] std::optional<PlexSearchResults> GetItemInfo(std::string_view userName, std::string_view itemName);
      [[nodiscard]] std::optional<std::filesystem::path> GetItemPath(std::string_view id);
      [[nodiscard]] std::unordered_map<std::string, std::filesystem::path> GetItemsPaths(const std::vector<std::string>& ids);

      // Returns if the collection in the library is valid on this server
      [[nodiscard]] bool GetCollectionValid(std::string_view library, std::string_view collection);

      // Returns the collection information for the collection in the library
      [[nodiscard]] std::optional<PlexCollection> GetCollection(std::string_view library, std::string_view collectionName);

      // Tell Plex to scan the passed in library
      void SetLibraryScan(std::string_view libraryId);
      void SetLibraryScanPath(std::string_view libraryId, const std::filesystem::path& path);

      bool SetPlayed(std::string_view userName, std::string_view ratingKey, int64_t locationMs);
      bool SetWatched(std::string_view userName, std::string_view ratingKey);

   protected:
      std::string_view GetApiBase() const override;
      std::string_view GetApiTokenName() const override;

   private:
      struct PlexApiImpl;
      friend struct PlexApiImpl;
      std::unique_ptr<PlexApiImpl> pimpl_;
   };
}