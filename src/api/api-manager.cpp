#include "warp/api/api-manager.h"

#include "warp/log/log.h"
#include "warp/log/log-utils.h"

#include <format>

namespace warp
{
   ApiManager::ApiManager(std::string_view appName,
                          std::string_view version,
                          const std::vector<ServerConfig>& plexConfigs,
                          const std::vector<ServerConfig>& embyConfigs)
   {
      SetupPlexApis(appName, version, plexConfigs);
      SetupEmbyApis(appName, version, embyConfigs);
   }

   void ApiManager::SetupPlexApis(std::string_view appName, std::string_view version, const std::vector<ServerConfig>& configs)
   {
      for (const auto& server : configs)
      {
         InitializeApi<PlexApi>(appName, version, plexApis_, server, warp::GetFormattedPlex());

         if (!server.tracker_url.empty())
         {
            InitializeApi<TautulliApi>(appName, version, tautulliApis_, server, warp::GetFormattedTautulli());
         }
      }
   }

   void ApiManager::SetupEmbyApis(std::string_view appName, std::string_view version, const std::vector<ServerConfig>& configs)
   {
      for (const auto& server : configs)
      {
         InitializeApi<EmbyApi>(appName, version, embyApis_, server, warp::GetFormattedEmby());

         if (!server.tracker_url.empty())
         {
            InitializeApi<JellystatApi>(appName, version, jellystatApis_, server, warp::GetFormattedJellystat());
         }
      }
   }

   void ApiManager::EnableExtraCaching()
   {
      for (auto& plexApi : plexApis_)
      {
         plexApi->EnableExtraCaching();
      }

      for (auto& embyApi : embyApis_)
      {
         embyApi->EnableExtraCaching();
      }
   }

   void ApiManager::GetTasks(std::vector<Task>& tasks)
   {
      InitializeTasks(plexApis_, tasks);
      InitializeTasks(tautulliApis_, tasks);
      InitializeTasks(embyApis_, tasks);
      InitializeTasks(jellystatApis_, tasks);
   }

   void ApiManager::LogServerConnectionSuccess(std::string_view serverName, ApiBase* api)
   {
      auto reported = api->GetServerReportedName();
      warp::log::Info("Connected to {}({}) successfully.{}",
                              serverName, api->GetName(),
                              reported ? std::format(" Server reported {}", warp::GetTag("name", *reported)) : "");
   }

   void ApiManager::LogServerConnectionError(std::string_view serverName, ApiBase* api)
   {
      warp::log::Warning("{}({}) server not available. Is this correct? {} {}",
                                 serverName, api->GetName(),
                                 warp::GetTag("url", api->GetUrl()),
                                 warp::GetTag("api_key", api->GetApiKey()));
   }

   PlexApi* ApiManager::GetPlexApi(std::string_view name) const
   {
      return FindApi(plexApis_, name);
   }

   EmbyApi* ApiManager::GetEmbyApi(std::string_view name) const
   {
      return FindApi(embyApis_, name);
   }

   TautulliApi* ApiManager::GetTautulliApi(std::string_view name) const
   {
      return FindApi(tautulliApis_, name);
   }

   JellystatApi* ApiManager::GetJellystatApi(std::string_view name) const
   {
      return FindApi(jellystatApis_, name);
   }

   ApiBase* ApiManager::GetApi(warp::ApiType type, std::string_view name) const
   {
      switch (type)
      {
         case warp::ApiType::PLEX:      return FindApi(plexApis_, name);
         case warp::ApiType::EMBY:      return FindApi(embyApis_, name);
         case warp::ApiType::TAUTULLI:  return FindApi(tautulliApis_, name);
         case warp::ApiType::JELLYSTAT: return FindApi(jellystatApis_, name);
         default:                 return nullptr;
      }
   }
}