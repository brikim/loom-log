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

   protected:
      std::string_view GetApiBase() const override;
      std::string_view GetApiTokenName() const override;

   private:
      struct TracearrApiImpl;
      friend struct TracearrApiImpl;
      std::unique_ptr<TracearrApiImpl> pimpl_;
   };
}