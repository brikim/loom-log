#include "warp/logger.h"

#include "ansii-formatter.h"
#include "ansii-remove-formatter.h"
#include "log-apprise-sync.h"

#include <spdlog/async.h>
#include <spdlog/async_logger.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <filesystem>
#include <string>

namespace warp
{
   namespace
   {
      spdlog::level::level_enum ToSpdLogLevel(Level level)
      {
         switch (level)
         {
            case Level::TRACE:
               return spdlog::level::trace;
            case Level::WARN:
               return spdlog::level::warn;
            case Level::ERR:
               return spdlog::level::err;
            case Level::CRITICAL:
               return spdlog::level::critical;
            default:
               return spdlog::level::info;
         }
      }
   }

   struct Logger::Impl
   {
      std::shared_ptr<spdlog::logger> logger;
   };

   Logger::Logger()
      : pimpl_(std::make_unique<Impl>())
   {
      // 8192 is the queue size (must be power of 2), 1 is the number of worker threads
      spdlog::init_thread_pool(128, 1);

      std::vector<spdlog::sink_ptr> sinks;
      auto& consoleSink{sinks.emplace_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>())};
      consoleSink->set_formatter(std::make_unique<AnsiiFormatter>());

      pimpl_->logger = std::make_shared<spdlog::async_logger>("warp-logger",
                                                              sinks.begin(),
                                                              sinks.end(),
                                                              spdlog::thread_pool(),
                                                              spdlog::async_overflow_policy::block);
      pimpl_->logger->flush_on(spdlog::level::info);

#if defined(_DEBUG) || !defined(NDEBUG)
      pimpl_->logger->set_level(spdlog::level::trace);
#endif
   }

   Logger::~Logger() = default;

   void Logger::InitFileLogging(std::string_view path, std::string_view filename)
   {
      std::filesystem::path p(path);
      p /= filename;

      std::error_code ec;
      std::filesystem::create_directories(p.parent_path(), ec);

      constexpr size_t max_size{1048576 * 5};
      constexpr size_t max_files{5};
      auto fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(p.string(), max_size, max_files);
      fileSink->set_formatter(std::make_unique<AnsiiRemoveFormatter>());

      pimpl_->logger->sinks().push_back(fileSink);
   }

   void Logger::InitApprise(const AppriseLoggingConfig& config)
   {
      if (config.enabled)
      {
         auto app_sink = std::make_shared<apprise_sink_mt>(config);

         // Only notify on Warnings and Errors
         app_sink->set_level(spdlog::level::warn);

         // Clean pattern for mobile/email notifications (No colors)
         app_sink->set_pattern("[%l] %v");

         pimpl_->logger->sinks().push_back(app_sink);
      }
   }

   void Logger::LogInternal(Level level, std::string_view msg)
   {
      pimpl_->logger->log(ToSpdLogLevel(level), msg);
   }

   bool Logger::ShouldLog(Level level)
   {
      return pimpl_->logger->should_log(ToSpdLogLevel(level));

   }
}