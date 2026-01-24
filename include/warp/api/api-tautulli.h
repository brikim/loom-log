#pragma once

#include "warp/api/api-base.h"
#include "warp/api/api-tautulli-types.h"

#include <httplib.h>

#include <cstdint>
#include <list>
#include <optional>
#include <string>

namespace warp
{
   class TautulliApi : public ApiBase
   {
   public:
      TautulliApi(std::string_view version, const ServerConfig& serverConfig);
      virtual ~TautulliApi() = default;

      [[nodiscard]] std::optional<std::vector<ApiTask>> GetTaskList() override;

      // Returns true if the server is reachable and the API key is valid
      [[nodiscard]] bool GetValid() override;
      [[nodiscard]] std::optional<std::string> GetServerReportedName() override;

      [[nodiscard]] std::optional<TautulliUserInfo> GetUserInfo(std::string_view name);

      [[nodiscard]] std::optional<TautulliHistoryItems> GetWatchHistoryForUser(std::string_view user,
                                                                               std::string_view dateForHistory,
                                                                               int64_t epochHistoryTime);

   private:
      std::string_view GetApiBase() const override;
      std::string_view GetApiTokenName() const override;

      [[nodiscard]] std::pair<std::string_view, std::string_view> GetCmdParam(std::string_view cmd) const;
      [[nodiscard]] std::optional<TautulliHistoryItems> GetWatchHistory(std::string_view user,
                                                                        int64_t epochHistoryTime,
                                                                        const ApiParams& extraParams);

      int32_t GetWatchedPercent();

      bool ReadMonitoringData();
      void RunSettingsUpdate();

      httplib::Headers headers_;

      std::optional<int32_t> watchedPercent_;
   };
}