#pragma once

#include "loomlog/log-types.h"

#include <format>
#include <memory>
#include <string_view>

namespace loomlog
{
   enum class Level
   {
      TRACE, INFO, WARN, ERR, CRITICAL
   };

   class Logger
   {
   public:
      // Returns a static instance of the Logger class
      static Logger& Instance()
      {
         static Logger instance;
         return instance;
      }

      void InitApprise(const AppriseLoggingConfig& config);

      template<typename... Args>
      void Trace(std::format_string<Args...> fmt, Args &&...args);

      template<typename... Args>
      void TraceWithHeader(std::string_view header, std::format_string<Args...> fmt, Args &&...args);

      template<typename... Args>
      void Info(std::format_string<Args...> fmt, Args &&...args);

      template<typename... Args>
      void InfoWithHeader(std::string_view header, std::format_string<Args...> fmt, Args &&...args);

      template<typename... Args>
      void Warning(std::format_string<Args...> fmt, Args &&...args);

      template<typename... Args>
      void WarningWithHeader(std::string_view header, std::format_string<Args...> fmt, Args &&...args);

      template<typename... Args>
      void Error(std::format_string<Args...> fmt, Args &&...args);

      template<typename... Args>
      void ErrorWithHeader(std::string_view header, std::format_string<Args...> fmt, Args &&...args);

      template<typename... Args>
      void Critical(std::format_string<Args...> fmt, Args &&...args);

      template<typename... Args>
      void CriticalWithHeader(std::string_view header, std::format_string<Args...> fmt, Args &&...args);

   private:
      Logger();
      virtual ~Logger();

      void LogInternal(Level level, std::string_view msg);
      bool ShouldLog(Level level);

      struct Impl;
      std::unique_ptr<Impl> pimpl_;
   };

   template<typename... Args>
   inline void Logger::Trace(std::format_string<Args...> fmt, Args &&...args)
   {
      if (ShouldLog(Level::TRACE))
      {
         std::string msg = std::format(fmt, std::forward<Args>(args)...);
         LogInternal(Level::TRACE, msg);
      }
   }

   template<typename... Args>
   inline void Logger::TraceWithHeader(std::string_view header, std::format_string<Args...> fmt, Args &&...args)
   {
      if (ShouldLog(Level::TRACE))
      {
         std::string msg = std::format("{}: {}", header, std::format(fmt, std::forward<Args>(args)...));
         LogInternal(Level::TRACE, msg);
      }
   }

   template<typename... Args>
   inline void Logger::Info(std::format_string<Args...> fmt, Args &&...args)
   {
      if (ShouldLog(Level::INFO))
      {
         std::string msg = std::format(fmt, std::forward<Args>(args)...);
         LogInternal(Level::INFO, msg);
      }
   }

   template<typename... Args>
   inline void Logger::InfoWithHeader(std::string_view header, std::format_string<Args...> fmt, Args &&...args)
   {
      if (ShouldLog(Level::INFO))
      {
         std::string msg = std::format("{}: {}", header, std::format(fmt, std::forward<Args>(args)...));
         LogInternal(Level::INFO, msg);
      }
   }

   template<typename... Args>
   inline void Logger::Warning(std::format_string<Args...> fmt, Args &&...args)
   {
      if (ShouldLog(Level::WARN))
      {
         std::string msg = std::format(fmt, std::forward<Args>(args)...);
         LogInternal(Level::WARN, msg);
      }
   }

   template<typename... Args>
   inline void Logger::WarningWithHeader(std::string_view header, std::format_string<Args...> fmt, Args &&...args)
   {
      if (ShouldLog(Level::WARN))
      {
         std::string msg = std::format("{}: {}", header, std::format(fmt, std::forward<Args>(args)...));
         LogInternal(Level::WARN, msg);
      }
   }

   template<typename... Args>
   inline void Logger::Error(std::format_string<Args...> fmt, Args &&...args)
   {
      if (ShouldLog(Level::ERR))
      {
         std::string msg = std::format(fmt, std::forward<Args>(args)...);
         LogInternal(Level::ERR, msg);
      }
   }

   template<typename... Args>
   inline void Logger::ErrorWithHeader(std::string_view header, std::format_string<Args...> fmt, Args &&...args)
   {
      if (ShouldLog(Level::ERR))
      {
         std::string msg = std::format("{}: {}", header, std::format(fmt, std::forward<Args>(args)...));
         LogInternal(Level::ERR, msg);
      }
   }

   template<typename... Args>
   inline void Logger::Critical(std::format_string<Args...> fmt, Args &&...args)
   {
      std::string msg = std::format(fmt, std::forward<Args>(args)...);
      LogInternal(Level::CRITICAL, msg);
   }

   template<typename... Args>
   inline void Logger::CriticalWithHeader(std::string_view header, std::format_string<Args...> fmt, Args &&...args)
   {
      std::string msg = std::format("{}: {}", header, std::format(fmt, std::forward<Args>(args)...));
      LogInternal(Level::CRITICAL, msg);
   }
}