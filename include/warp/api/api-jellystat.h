#pragma once

#include "warp/api/api-base.h"
#include "warp/api/api-jellystat-types.h"

#include <optional>
#include <string>
#include <string_view>

namespace warp
{
   struct JellystatApiImpl;

   class JellystatApi : public ApiBase
   {
   public:
      JellystatApi(std::string_view appName, std::string_view version, const ServerConfig& serverConfig);
      virtual ~JellystatApi();

      // Returns true if the server is reachable and the API key is valid
      [[nodiscard]] bool GetValid() override;
      [[nodiscard]] std::optional<std::string> GetServerReportedName() override;

      [[nodiscard]] std::optional<JellystatHistoryItems> GetWatchHistoryForUser(std::string_view userId);

   protected:
      std::string_view GetApiBase() const override;
      std::string_view GetApiTokenName() const override;

   private:
      friend struct JellystatApiImpl;
      std::unique_ptr<JellystatApiImpl> pimpl_;
   };
}