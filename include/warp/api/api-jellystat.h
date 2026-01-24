#pragma once

#include "warp/api/api-base.h"
#include "warp/api/api-jellystat-types.h"

#include <httplib.h>

#include <list>
#include <optional>
#include <string>
#include <string_view>

namespace warp
{
   class JellystatApi : public ApiBase
   {
   public:
      JellystatApi(std::string_view version, const ServerConfig& serverConfig);
      virtual ~JellystatApi() = default;

      // Returns true if the server is reachable and the API key is valid
      [[nodiscard]] bool GetValid() override;
      [[nodiscard]] std::optional<std::string> GetServerReportedName() override;

      [[nodiscard]] std::optional<JellystatHistoryItems> GetWatchHistoryForUser(std::string_view userId);

   private:
      std::string_view GetApiBase() const override;
      std::string_view GetApiTokenName() const override;

      std::string ParamsToJson(const std::list<std::pair<std::string_view, std::string_view>> params);

      httplib::Headers headers_;
   };
}