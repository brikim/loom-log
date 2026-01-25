#pragma once

#include "warp/api/api-base.h"
#include "warp/api/api-emby-types.h"
#include "warp/api/api-types.h"

#include <chrono>
#include <cstdint>
#include <list>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace warp
{
   class EmbyApi : public ApiBase
   {
   public:
      EmbyApi(std::string_view appName, std::string_view version, const ServerConfig& serverConfig);
      virtual ~EmbyApi() = default;

      void EnableExtraCaching();

      [[nodiscard]] std::optional<std::vector<Task>> GetTaskList() override;

      // Returns true if the server is reachable and the API key is valid
      [[nodiscard]] bool GetValid() override;
      [[nodiscard]] std::string_view GetMediaPath() const;
      [[nodiscard]] std::optional<std::string> GetServerReportedName() override;
      [[nodiscard]] std::optional<std::string> GetLibraryId(std::string_view libraryName);

      std::optional<EmbyItem> GetItem(EmbySearchType type, std::string_view name, const ApiParams& extraSearchArgs = {});

      [[nodiscard]] std::optional<EmbyUserData> GetUser(std::string_view name);

      [[nodiscard]] bool GetWatchedStatus(std::string_view userId, std::string_view itemId);
      bool SetWatchedStatus(std::string_view userId, std::string_view itemId);

      [[nodiscard]] std::optional<EmbyPlayState> GetPlayState(std::string_view userId, std::string_view itemId);
      bool SetPlayState(std::string_view userId, std::string_view itemId, int64_t positionTicks, std::string_view dateTimeStr);

      [[nodiscard]] bool GetPlaylistExists(std::string_view name);
      [[nodiscard]] std::optional<EmbyPlaylist> GetPlaylist(std::string_view name);
      void CreatePlaylist(std::string_view name, const std::vector<std::string>& itemIds);
      bool AddPlaylistItems(std::string_view playlistId, const std::vector<std::string>& addIds);
      bool RemovePlaylistItems(std::string_view playlistId, const std::vector<std::string>& removeIds);
      bool MovePlaylistItem(std::string_view playlistId, std::string_view itemId, uint32_t index);

      // Tell Emby to scan the passed in library
      void SetLibraryScan(std::string_view libraryId);

      [[nodiscard]] bool GetPathMapEmpty() const;
      [[nodiscard]] std::optional<std::string> GetIdFromPathMap(const std::string& path);

   private:
      std::string_view GetApiBase() const override;
      std::string_view GetApiTokenName() const override;

      void BuildPathMap();
      void RunPathMapQuickCheck();
      void RunPathMapFullUpdate();

      bool HasLibraryChanged();

      std::string_view GetSearchTypeStr(EmbySearchType type);

      Headers headers_;
      std::string mediaPath_;

      std::string lastSyncTimestamp_;
      bool enableExtraCache_{false};
      EmbyPathMap pathMap_;
      EmbyPathMap workingPathMap_;

      mutable std::mutex taskLock_;
   };
}