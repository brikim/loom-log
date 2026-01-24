#include "warp/scheduler/cron-scheduler.h"

#include "warp/log/log.h"
#include "warp/log/log-utils.h"

#include <algorithm>
#include <ranges>

namespace warp
{
   CronScheduler::~CronScheduler()
   {
      Shutdown();
   }

   void CronScheduler::Add(const Task& task)
   {
      if (runThread_)
      {
         warp::log::Error("Cron Scheduler: Attempted to add task {} after start", task.name);
         return;
      }

      try
      {
         auto& cronTask = cronTasks_.emplace_back();
         cronTask.task = task;
         cronTask.cron = cron::make_cron(task.cronExpression);
         cronTask.nextRun = cron::cron_next(cronTask.cron, std::chrono::system_clock::now());

         warp::log::Trace("Cron Scheduler: Added task {} with {}",
                                  warp::GetTag("name", cronTask.task.name),
                                  warp::GetTag("cron", cronTask.task.cronExpression));
      }
      catch (const cron::bad_cronexpr& ex)
      {
         warp::log::Error("Cron Scheduler: {} bad CRON {} - {}",
                                  task.name, task.cronExpression, ex.what());
      }
   }

   std::chrono::system_clock::time_point CronScheduler::GetNextRunTime() const
   {
      if (cronTasks_.empty()) return std::chrono::system_clock::time_point::max();

      // Use std::min_element to find the earliest task in one line
      auto it = std::ranges::min_element(cronTasks_,
          [](const auto& a, const auto& b) { return a.nextRun < b.nextRun; });

      return it->nextRun;
   }

   void CronScheduler::RunTasks()
   {
      auto currentTime = std::chrono::system_clock::now();
      for (auto& cronTask : cronTasks_)
      {
         // Should this task be run
         if (cronTask.nextRun <= currentTime)
         {
            warp::log::Trace("Cron Scheduler: Running task {} with {}",
                                     warp::GetTag("name", cronTask.task.name),
                                     warp::GetTag("cron", cronTask.task.cronExpression));
            try
            {
               cronTask.task.func();
            }
            catch (const std::exception& e)
            {
               warp::log::Error("Task {} failed: {}", cronTask.task.name, e.what());
            }

            // Update nextRun for next time
            cronTask.nextRun = cron::cron_next(cronTask.cron, currentTime);
         }
      }
   }

   void CronScheduler::Work(std::stop_token stopToken)
   {
      while (!stopToken.stop_requested())
      {
         // Get the next time we need to wake up
         auto nextWaitPoint = GetNextRunTime();

         // Wait until the next scheduled task or a stop is requested
         std::unique_lock<std::mutex> lock(cvLock_);
         cv_.wait_until(lock, stopToken, nextWaitPoint, [&] { return false; });

         // If a stop is requested break out of the loop
         if (stopToken.stop_requested()) break;

         RunTasks();
      }

      warp::log::Info("Cron Scheduler: Work thread shutting down");
   }

   bool CronScheduler::Start()
   {
      if (cronTasks_.empty()) return false;

      // jthread starts immediately and manages its own lifetime
      runThread_ = std::make_unique<std::jthread>([this](std::stop_token st) { Work(st); });

      for (const auto& cronTask : cronTasks_)
      {
         if (cronTask.task.service)
         {
            warp::log::Info("{}: Enabled - Schedule: {}",
                                    cronTask.task.name, cronTask.task.cronExpression);
         }
      }
      return true;
   }

   void CronScheduler::Shutdown()
   {
      if (!runThread_) return;

      runThread_->request_stop();
      cv_.notify_all();

      if (runThread_->joinable()) runThread_->join();
      runThread_.reset();
   }
}