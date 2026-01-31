#pragma once

#include "warp/types.h"

#include <memory>

namespace warp
{
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
      class CronSchedulerImpl;
      std::unique_ptr<CronSchedulerImpl> pimpl_;
   };
}