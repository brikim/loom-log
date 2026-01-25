#pragma once

#include "warp/types.h"

#include <memory>

namespace warp
{
   class CronSchedulerImpl;

   class CronScheduler
   {
   public:
      CronScheduler();
      ~CronScheduler();

      // Standardize: Add tasks before starting
      void Add(const Task& task);

      bool Start();
      void Shutdown();

   private:
      std::unique_ptr<CronSchedulerImpl> impl_;
   };
}