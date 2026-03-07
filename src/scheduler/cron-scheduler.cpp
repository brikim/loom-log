#include "warp/scheduler/cron-scheduler.h"

#include "warp/log/log.h"
#include "warp/log/log-utils.h"

#include <libcron/Cron.h>

#include <algorithm>
#include <ranges>

namespace warp
{
   class CronScheduler::CronSchedulerImpl
   {
   public:
      libcron::Cron<libcron::LocalClock, libcron::Locker> cronTasks_;
      std::mutex mtx_;
      std::condition_variable_any cv_;
      std::unique_ptr<std::jthread> runThread_;

      void Add(const Task& task);

      void Work(std::stop_token stopToken);
      bool Start();
      void Shutdown();
   };

   CronScheduler::CronScheduler()
      : pimpl_(std::make_unique<CronSchedulerImpl>())
   {
   }

   CronScheduler::~CronScheduler()
   {
      Shutdown();
   }

   void CronScheduler::CronSchedulerImpl::Add(const Task& task)
   {
      if (runThread_)
      {
         log::Error("Cron Scheduler: Attempted to add task {} after start", task.name);
         return;
      }

      log::Info("{}: Enabled - Schedule: {}", task.name, task.cronExpression);

      bool success = cronTasks_.add_schedule(task.name, task.cronExpression, [task](auto& info) {
         try
         {
            log::Trace("Cron Scheduler: Running task {} with {}",
                       GetTag("name", task.name),
                       GetTag("cron", task.cronExpression));
            task.func();
         }
         catch (const std::exception& e)
         {
            // Log using your warp logger
         }
      });
   }

   void CronScheduler::Add(const Task& task)
   {
      pimpl_->Add(task);
   }

   void CronScheduler::CronSchedulerImpl::Work(std::stop_token stopToken)
   {
      while (!stopToken.stop_requested())
      {
         cronTasks_.tick();

         std::unique_lock lock(mtx_);
         cv_.wait_for(lock, stopToken, std::chrono::milliseconds(1000), [&] {
            return stopToken.stop_requested();
         });

         // If a stop is requested break out of the loop
         if (stopToken.stop_requested()) break;
      }

      log::Info("Cron Scheduler: Work thread shutting down");
   }

   bool CronScheduler::CronSchedulerImpl::Start()
   {
      if (cronTasks_.count() == 0) return false;

      // jthread starts immediately and manages its own lifetime
      runThread_ = std::make_unique<std::jthread>([this](std::stop_token st) { Work(st); });

      return true;
   }

   bool CronScheduler::Start()
   {
      return pimpl_->Start();
   }

   void CronScheduler::CronSchedulerImpl::Shutdown()
   {
      if (!runThread_) return;

      runThread_->request_stop();
      cv_.notify_all();

      if (runThread_->joinable()) runThread_->join();
      runThread_.reset();
   }

   void CronScheduler::Shutdown()
   {
      pimpl_->Shutdown();
   }
}