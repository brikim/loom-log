#pragma once

#include "warp/api/api-base.h"
#include "warp/api/api-emby-types.h"
#include "warp/api/api-types.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace warp
{
   class EmbyApi : public ApiBase
   {
   public:
      EmbyApi(std::string_view appName, std::string_view version, const ServerConfig& serverConfig);
      virtual ~EmbyApi();

      void EnableExtraCaching();

      [[nodiscard]] std::optional<std::vector<Task>> GetTaskList() override;

      // Returns true if the server is reachable and the API key is valid
      [[nodiscard]] bool GetValid() override;
      [[nodiscard]] const std::filesystem::path& GetMediaPath() const;
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

      [[nodiscard]] std::vector<EmbyItemBackdropImages> GetItemsWithMultipleBackdrops(std::string_view libId);
      [[nodiscard]] std::vector<EmbyBackdrop> GetBackdrops(std::string_view id);
      bool RemoveBackdropImage(std::string_view id, int32_t index);

      // Tell Emby to scan the passed in library
      void SetLibraryScan(std::string_view libraryId);

      [[nodiscard]] bool GetPathCacheEmpty() const;
      [[nodiscard]] std::optional<std::string> GetIdFromPath(const std::filesystem::path& path);

   protected:
      std::string_view GetApiBase() const override;
      std::string_view GetApiTokenName() const override;

   private:
      struct EmbyApiImpl;
      friend struct EmbyApiImpl;
      std::unique_ptr<EmbyApiImpl> pimpl_;
   };
}