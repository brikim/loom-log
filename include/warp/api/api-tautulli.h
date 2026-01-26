#pragma once

#include "warp/api/api-base.h"
#include "warp/api/api-tautulli-types.h"
#include "warp/api/api-types.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace warp
{
   struct TautulliApiImpl;

   class TautulliApi : public ApiBase
   {
   public:
      TautulliApi(std::string_view appName, std::string_view version, const ServerConfig& serverConfig);
      virtual ~TautulliApi();

      [[nodiscard]] std::optional<std::vector<Task>> GetTaskList() override;

      // Returns true if the server is reachable and the API key is valid
      [[nodiscard]] bool GetValid() override;
      [[nodiscard]] std::optional<std::string> GetServerReportedName() override;

      [[nodiscard]] std::optional<TautulliUserInfo> GetUserInfo(std::string_view name);

      [[nodiscard]] std::optional<TautulliHistoryItems> GetWatchHistoryForUser(std::string_view user,
                                                                               std::string_view dateForHistory,
                                                                               int64_t epochHistoryTime);

   protected:
      std::string_view GetApiBase() const override;
      std::string_view GetApiTokenName() const override;

   private:
      friend struct TautulliApiImpl;
      std::unique_ptr<TautulliApiImpl> pimpl_;
   };
}