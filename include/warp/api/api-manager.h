#pragma once

#include "warp/api/api-base.h"
#include "warp/api/api-emby.h"
#include "warp/api/api-jellystat.h"
#include "warp/api/api-plex.h"
#include "warp/api/api-tautulli.h"
#include "warp/api/api-types.h"
#include "warp/types.h"

#include <memory>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

namespace warp
{
   class ApiManager
   {
   public:
      ApiManager(std::string_view version,
                 const std::vector<ServerConfig>& plexConfigs,
                 const std::vector<ServerConfig>& embyConfigs);
      virtual ~ApiManager() = default;

      void GetTasks(std::vector<ApiTask>& tasks);

      [[nodiscard]] PlexApi* GetPlexApi(std::string_view name) const;
      [[nodiscard]] EmbyApi* GetEmbyApi(std::string_view name) const;
      [[nodiscard]] TautulliApi* GetTautulliApi(std::string_view name) const;
      [[nodiscard]] JellystatApi* GetJellystatApi(std::string_view name) const;
      [[nodiscard]] ApiBase* GetApi(warp::ApiType type, std::string_view name) const;

   private:
      void SetupPlexApis(std::string_view version, const std::vector<ServerConfig>& serverConfigs);
      void SetupEmbyApis(std::string_view version, const std::vector<ServerConfig>& serverConfigs);

      void LogServerConnectionSuccess(std::string_view serverName, ApiBase* api);
      void LogServerConnectionError(std::string_view serverName, ApiBase* api);

      template <typename ApiT, typename ContainerT>
      ApiT* InitializeApi(std::string_view version, ContainerT& container, const ServerConfig& config, std::string_view logName)
      {
         auto& api = container.emplace_back(std::make_unique<ApiT>(version, config));
         api->GetValid() ? LogServerConnectionSuccess(logName, api.get()) : LogServerConnectionError(logName, api.get());
         return api.get();
      }

      template <typename ContainerT>
      void InitializeTasks(ContainerT& container, std::vector<ApiTask>& tasks)
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

      std::vector<std::unique_ptr<PlexApi>> plexApis_;
      std::vector<std::unique_ptr<EmbyApi>> embyApis_;
      std::vector<std::unique_ptr<TautulliApi>> tautulliApis_;
      std::vector<std::unique_ptr<JellystatApi>> jellystatApis_;

   };
}