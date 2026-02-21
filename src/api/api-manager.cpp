#include "warp/api/api-manager.h"

#include "warp/log/log.h"
#include "warp/log/log-utils.h"

#include <format>
#include <ranges>
#include <string>

namespace warp
{
   struct ApiManagerImpl
   {
      std::vector<std::unique_ptr<PlexApi>> plexApis_;
      std::vector<std::unique_ptr<EmbyApi>> embyApis_;
      std::vector<std::unique_ptr<TautulliApi>> tautulliApis_;
      std::vector<std::unique_ptr<JellystatApi>> jellystatApis_;

      void SetupPlexApis(std::string_view appName, std::string_view version, const ApiManagerPlexConfig& config)
      {
         for (const auto& server : config.servers)
         {
            InitializeApiWithOptions<PlexApi>(appName, version, plexApis_, server, config.options, GetFormattedPlex());
            if (!server.trackerUrl.empty())
            {
               InitializeApi<TautulliApi>(appName, version, tautulliApis_, server, GetFormattedTautulli());
            }
         }
      }

      void SetupEmbyApis(std::string_view appName, std::string_view version, const ApiManagerEmbyConfig& config)
      {
         for (const auto& server : config.servers)
         {
            InitializeApiWithOptions<EmbyApi>(appName, version, embyApis_, server, config.options, GetFormattedEmby());
            if (!server.trackerUrl.empty())
            {
               InitializeApi<JellystatApi>(appName, version, jellystatApis_, server, GetFormattedJellystat());
            }
         }
      }

      void GetTasks(std::vector<Task>& tasks)
      {
         InitializeTasks(plexApis_, tasks);
         InitializeTasks(tautulliApis_, tasks);
         InitializeTasks(embyApis_, tasks);
         InitializeTasks(jellystatApis_, tasks);
      }

      void Shutdown()
      {
         std::ranges::for_each(plexApis_, [](auto& api) {
            api->Shutdown();
         });
         std::ranges::for_each(embyApis_, [](auto& api) {
            api->Shutdown();
         });
         std::ranges::for_each(tautulliApis_, [](auto& api) {
            api->Shutdown();
         });
         std::ranges::for_each(jellystatApis_, [](auto& api) {
            api->Shutdown();
         });
      }

      void LogServerConnectionSuccess(std::string_view serverName, ApiBase* api)
      {
         auto reported = api->GetServerReportedName();
         log::Info("Connected to {}({}) successfully.{}",
                   serverName, api->GetName(),
                   reported ? std::format(" Server reported {}", GetTag("name", *reported)) : "");
      }

      void LogServerConnectionError(std::string_view serverName, ApiBase* api)
      {
         log::Warning("{}({}) server not available. Is this correct? {} {}",
                      serverName, api->GetName(),
                      GetTag("url", api->GetUrl()),
                      GetTag("api_key", api->GetApiKey()));
      }

      template <typename ApiT, typename ContainerT, typename OptionsT>
      ApiT* InitializeApiWithOptions(std::string_view appName,
                                     std::string_view version,
                                     ContainerT& container,
                                     const ServerConfig& config,
                                     const OptionsT& options,
                                     std::string_view logName)
      {
         auto& api = container.emplace_back(std::make_unique<ApiT>(appName, version, config, options));
         api->GetValid() ? LogServerConnectionSuccess(logName, api.get()) : LogServerConnectionError(logName, api.get());
         return api.get();
      }

      template <typename ApiT, typename ContainerT>
      ApiT* InitializeApi(std::string_view appName, std::string_view version, ContainerT& container, const ServerConfig& config, std::string_view logName)
      {
         auto& api = container.emplace_back(std::make_unique<ApiT>(appName, version, config));
         api->GetValid() ? LogServerConnectionSuccess(logName, api.get()) : LogServerConnectionError(logName, api.get());
         return api.get();
      }

      template <typename ContainerT>
      void InitializeTasks(ContainerT& container, std::vector<Task>& tasks)
      {
         for (auto& api : container)
         {
            auto taskList = api->GetTaskList();
            if (taskList)
            {
               for (const auto& task : *taskList) tasks.emplace_back(task);
            }
         }
      }

      // In header (private):
      template <typename T>
      [[nodiscard]] T* FindApi(const std::vector<std::unique_ptr<T>>& container, std::string_view name) const
      {
         auto it = std::ranges::find_if(container, [name](const auto& api) {
            return api->GetName() == name;
         });
         return it != container.end() ? it->get() : nullptr;
      }

      PlexApi* GetPlexApi(std::string_view name) const
      {
         return FindApi(plexApis_, name);
      }

      EmbyApi* GetEmbyApi(std::string_view name) const
      {
         return FindApi(embyApis_, name);
      }

      TautulliApi* GetTautulliApi(std::string_view name) const
      {
         return FindApi(tautulliApis_, name);
      }

      JellystatApi* GetJellystatApi(std::string_view name) const
      {
         return FindApi(jellystatApis_, name);
      }

      ApiBase* GetApi(ApiType type, std::string_view name) const
      {
         switch (type)
         {
            case ApiType::PLEX:      return FindApi(plexApis_, name);
            case ApiType::EMBY:      return FindApi(embyApis_, name);
            case ApiType::TAUTULLI:  return FindApi(tautulliApis_, name);
            case ApiType::JELLYSTAT: return FindApi(jellystatApis_, name);
            default:                 return nullptr;
         }
      }
   };

   ApiManager::ApiManager(std::string_view appName,
                          std::string_view version,
                          const ApiManagerConfig& config)
      : pimpl_(std::make_unique<ApiManagerImpl>())
   {
      pimpl_->SetupPlexApis(appName, version, config.plexConfig);
      pimpl_->SetupEmbyApis(appName, version, config.embyConfig);
   }

   ApiManager::~ApiManager() = default;

   void ApiManager::Shutdown()
   {
      pimpl_->Shutdown();
   }

   void ApiManager::GetTasks(std::vector<Task>& tasks)
   {
      pimpl_->GetTasks(tasks);
   }

   PlexApi* ApiManager::GetPlexApi(std::string_view name) const
   {
      return pimpl_->GetPlexApi(name);
   }

   EmbyApi* ApiManager::GetEmbyApi(std::string_view name) const
   {
      return pimpl_->GetEmbyApi(name);
   }

   TautulliApi* ApiManager::GetTautulliApi(std::string_view name) const
   {
      return pimpl_->GetTautulliApi(name);
   }

   JellystatApi* ApiManager::GetJellystatApi(std::string_view name) const
   {
      return pimpl_->GetJellystatApi(name);
   }

   ApiBase* ApiManager::GetApi(ApiType type, std::string_view name) const
   {
      return pimpl_->GetApi(type, name);
   }
}