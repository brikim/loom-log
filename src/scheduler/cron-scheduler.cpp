#include "warp/scheduler/cron-scheduler.h"

#include "warp/log/log.h"
#include "warp/log/log-utils.h"

#include <libcron/Cron.h>

#include <algorithm>
#include <atomic>
#include <ranges>

namespace warp
{
   class CronScheduler::CronSchedulerImpl
   {
   public:
      libcron::Cron<libcron::LocalClock, libcron::Locker> cronTasks_;
      std::mutex mtx_;
      std::condition_variable_any cv_;
      std::atomic_bool started_{false};
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
      if (started_)
      {
         log::Error("Cron Scheduler: Attempted to add task {} after start", task.name);
         return;
      }

      std::string expr = task.cronExpression;
      if (expr.ends_with('*') && std::ranges::count(expr, ' ') == 5)
      {
         expr.back() = '?';
      }

      auto tagTaskName = GetTag("task", task.name);
      auto tagCronName = GetTag("cron", task.cronExpression);
      bool success = cronTasks_.add_schedule(task.name, expr, [task, tagTaskName, tagCronName]([[maybe_unused]] auto& info) {
         try
         {
            log::Trace("Cron Scheduler: Running {} with {}", tagTaskName, tagCronName);
            task.func();
         }
         catch (const std::exception& e)
         {
            log::Error("Cron Scheduler: {} caught {}",
                       tagTaskName,
                       GetTag("exception", e.what()));
         }
      });

      if (success)
         log::Info("{}: Enabled - Schedule: {}", task.name, task.cronExpression);
      else
         log::Error("Cron Scheduler: Failed to schedule {} {}",
                    tagTaskName,
                    tagCronName);

   }

   void CronScheduler::Add(const Task& task)
   {
      pimpl_->Add(task);
   }

   void CronScheduler::CronSchedulerImpl::Work(std::stop_token stopToken)
   {
      // Set the scheduler to started
      started_ = true;

      while (!stopToken.stop_requested())
      {
         // Cron tasks tick will monitor the current time and run needed tasks
         cronTasks_.tick();

         std::unique_lock lock(mtx_);
         cv_.wait_for(lock, stopToken, std::chrono::milliseconds(1000), [&] {
            return stopToken.stop_requested();
         });
      }

      log::Info("Cron Scheduler: Work thread shutting down");
   }

   bool CronScheduler::CronSchedulerImpl::Start()
   {
      if (cronTasks_.count() == 0)
         return false;

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
      if (!runThread_)
         return;

      runThread_->request_stop();
      cv_.notify_all();

      if (runThread_->joinable())
         runThread_->join();

      runThread_.reset();
   }

   void CronScheduler::Shutdown()
   {
      pimpl_->Shutdown();
   }
}