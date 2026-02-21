#pragma once

#include "warp/api/api-base.h"
#include "warp/api/api-emby.h"
#include "warp/api/api-jellystat.h"
#include "warp/api/api-plex.h"
#include "warp/api/api-tautulli.h"
#include "warp/api/api-types.h"
#include "warp/types.h"

#include <memory>
#include <string_view>
#include <vector>

namespace warp
{
   struct ApiManagerImpl;

   struct ApiManagerPlexConfig
   {
      std::vector<ServerConfig> servers;
      ServerPlexOptions options;
   };

   struct ApiManagerEmbyConfig
   {
      std::vector<ServerConfig> servers;
      ServerEmbyOptions options;
   };

   struct ApiManagerConfig
   {
      ApiManagerPlexConfig plexConfig;
      ApiManagerEmbyConfig embyConfig;
   };

   class ApiManager
   {
   public:
      ApiManager(std::string_view appName,
                 std::string_view version,
                 const ApiManagerConfig& config);
      virtual ~ApiManager();

      void GetTasks(std::vector<Task>& tasks);

      // Shutdown the api manager
      void Shutdown();

      [[nodiscard]] PlexApi* GetPlexApi(std::string_view name) const;
      [[nodiscard]] EmbyApi* GetEmbyApi(std::string_view name) const;
      [[nodiscard]] TautulliApi* GetTautulliApi(std::string_view name) const;
      [[nodiscard]] JellystatApi* GetJellystatApi(std::string_view name) const;
      [[nodiscard]] ApiBase* GetApi(ApiType type, std::string_view name) const;

   private:
      std::unique_ptr<ApiManagerImpl> pimpl_;
   };
}