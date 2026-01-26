#include "warp/scheduler/cron-scheduler.h"

#include "warp/log/log.h"
#include "warp/log/log-utils.h"

#include <algorithm>
#include <ranges>

namespace warp
{
   // Internal structure only visible in this .cpp
   struct CronTask
   {
      Task task;
      cron::cronexpr cron;
      std::chrono::system_clock::time_point nextRun;
   };

   class CronSchedulerImpl
   {
   public:
      std::vector<CronTask> tasks;
      std::mutex mtx;
      std::condition_variable_any cv;
      std::unique_ptr<std::jthread> runThread;

      void Add(const Task& task)
      {
         if (runThread)
         {
            warp::log::Error("Cron Scheduler: Attempted to add task {} after start", task.name);
            return;
         }

         try
         {
            // Construct the cron first as this will throw an exception if invalid.
            // This prevents a task being added with invalid cron
            auto cron = cron::make_cron(task.cronExpression);

            auto& cronTask = tasks.emplace_back();
            cronTask.task = task;
            cronTask.cron = cron;
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

      std::chrono::system_clock::time_point GetNextRunTime() const
      {
         if (tasks.empty()) return std::chrono::system_clock::time_point::max();

         // Use std::min_element to find the earliest task in one line
         auto it = std::ranges::min_element(tasks,
             [](const auto& a, const auto& b) { return a.nextRun < b.nextRun; });

         return it->nextRun;
      }

      void RunTasks()
      {
         auto currentTime = std::chrono::system_clock::now();
         for (auto& task : tasks)
         {
            // Should this task be run
            if (task.nextRun <= currentTime)
            {
               warp::log::Trace("Cron Scheduler: Running task {} with {}",
                                        warp::GetTag("name", task.task.name),
                                        warp::GetTag("cron", task.task.cronExpression));
               try
               {
                  task.task.func();
               }
               catch (const std::exception& e)
               {
                  warp::log::Error("Task {} failed: {}", task.task.name, e.what());
               }

               // Update nextRun for next time
               task.nextRun = cron::cron_next(task.cron, currentTime);
            }
         }
      }

      void Work(std::stop_token stopToken)
      {
         while (!stopToken.stop_requested())
         {
            // Get the next time we need to wake up
            auto nextWaitPoint = GetNextRunTime();

            // Wait until the next scheduled task or a stop is requested
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait_until(lock, stopToken, nextWaitPoint, [&] { return false; });

            // If a stop is requested break out of the loop
            if (stopToken.stop_requested()) break;

            RunTasks();
         }

         warp::log::Info("Cron Scheduler: Work thread shutting down");
      }

      bool Start()
      {
         if (tasks.empty()) return false;

         // jthread starts immediately and manages its own lifetime
         runThread = std::make_unique<std::jthread>([this](std::stop_token st) { Work(st); });

         for (const auto& cronTask : tasks)
         {
            if (cronTask.task.service)
            {
               warp::log::Info("{}: Enabled - Schedule: {}",
                                       cronTask.task.name, cronTask.task.cronExpression);
            }
         }
         return true;
      }

      void Shutdown()
      {
         if (!runThread) return;

         runThread->request_stop();
         cv.notify_all();

         if (runThread->joinable()) runThread->join();
         runThread.reset();
      }
   };

   CronScheduler::CronScheduler()
      : pimpl_(std::make_unique<CronSchedulerImpl>())
   {
   }

   CronScheduler::~CronScheduler()
   {
      Shutdown();
   }

   void CronScheduler::Add(const Task& task)
   {
      pimpl_->Add(task);
   }

   bool CronScheduler::Start()
   {
      return pimpl_->Start();
   }

   void CronScheduler::Shutdown()
   {
      pimpl_->Shutdown();
   }
}