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

      // Returns a list of tasks that the API can perform. If no tasks are available, returns nullopt
      [[nodiscard]] std::optional<std::vector<Task>> GetTaskList() override;

      // Returns the watch history for all servers
      [[nodiscard]] std::optional<TracearrHistoryItems> GetWatchHistory(std::string_view dateForHistory);

      // Returns the server information for the given server id. If the server is not found, returns nullopt
      [[nodiscard]] std::optional<std::string_view> GetServerNameFromId(std::string_view id);

      // Returns the user information for the given user id. If the user is not found, returns nullopt
      [[nodiscard]] std::optional<TracearrUser> GetUser(std::string_view id);

   protected:
      std::string_view GetApiBase(std::optional<int32_t> version) const override;
      std::string_view GetApiTokenName() const override;

   private:
      struct TracearrApiImpl;
      friend struct TracearrApiImpl;
      std::unique_ptr<TracearrApiImpl> pimpl_;
   };
}