#pragma once

#include "warp/api/api-base.h"
#include "warp/api/api-tracearr-types.h"
#include "warp/types.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace warp
{
   struct TracearrServerInfo
   {
      std::string serverName;
      ApiType apiType;
   };

   class TracearrApi : public ApiBase
   {
   public:
      TracearrApi(std::string_view appName,
                  std::string_view version,
                  const TracearrConfig& serverConfig);
      virtual ~TracearrApi();

      // Returns true if the server is reachable and the API key is valid
      [[nodiscard]] bool GetValid() override;
      [[nodiscard]] std::optional<std::string> GetServerReportedName() override;

      [[nodiscard]] std::optional<std::vector<Task>> GetTaskList() override;

      // Returns the server name for the given tracearr server name, or nullopt if not found
      [[nodiscard]] std::optional<TracearrServerInfo> GetServerData(std::string_view tracearrServerName) const;

      // Returns the watch history for all servers
      [[nodiscard]] std::optional<TracearrHistoryItems> GetWatchHistory(std::string_view dateForHistory);

   protected:
      std::string_view GetApiBase(std::optional<int32_t> version) const override;
      std::string_view GetApiTokenName() const override;

   private:
      struct TracearrApiImpl;
      friend struct TracearrApiImpl;
      std::unique_ptr<TracearrApiImpl> pimpl_;
   };
}