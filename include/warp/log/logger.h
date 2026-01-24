#pragma once

#include "warp/log/log-types.h"

#include <format>
#include <memory>
#include <string_view>

namespace warp
{
   class Logger
   {
   public:
      // Returns a static instance of the Logger class
      static Logger& Instance()
      {
         static Logger instance;
         return instance;
      }

      void InitFileLogging(std::string_view path, std::string_view filename);
      void InitApprise(const AppriseLoggingConfig& config);
      void InitGotify(const GotifyLoggingConfig& config);

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

      void LogInternal(LogType level, std::string_view msg);
      bool ShouldLog(LogType level);

      struct Impl;
      std::unique_ptr<Impl> pimpl_;
   };

   template<typename... Args>
   inline void Logger::Trace(std::format_string<Args...> fmt, Args &&...args)
   {
      if (ShouldLog(LogType::TRACE))
      {
         std::string msg = std::format(fmt, std::forward<Args>(args)...);
         LogInternal(LogType::TRACE, msg);
      }
   }

   template<typename... Args>
   inline void Logger::TraceWithHeader(std::string_view header, std::format_string<Args...> fmt, Args &&...args)
   {
      if (ShouldLog(LogType::TRACE))
      {
         std::string msg = std::format("{}: {}", header, std::format(fmt, std::forward<Args>(args)...));
         LogInternal(LogType::TRACE, msg);
      }
   }

   template<typename... Args>
   inline void Logger::Info(std::format_string<Args...> fmt, Args &&...args)
   {
      if (ShouldLog(LogType::INFO))
      {
         std::string msg = std::format(fmt, std::forward<Args>(args)...);
         LogInternal(LogType::INFO, msg);
      }
   }

   template<typename... Args>
   inline void Logger::InfoWithHeader(std::string_view header, std::format_string<Args...> fmt, Args &&...args)
   {
      if (ShouldLog(LogType::INFO))
      {
         std::string msg = std::format("{}: {}", header, std::format(fmt, std::forward<Args>(args)...));
         LogInternal(LogType::INFO, msg);
      }
   }

   template<typename... Args>
   inline void Logger::Warning(std::format_string<Args...> fmt, Args &&...args)
   {
      if (ShouldLog(LogType::WARN))
      {
         std::string msg = std::format(fmt, std::forward<Args>(args)...);
         LogInternal(LogType::WARN, msg);
      }
   }

   template<typename... Args>
   inline void Logger::WarningWithHeader(std::string_view header, std::format_string<Args...> fmt, Args &&...args)
   {
      if (ShouldLog(LogType::WARN))
      {
         std::string msg = std::format("{}: {}", header, std::format(fmt, std::forward<Args>(args)...));
         LogInternal(LogType::WARN, msg);
      }
   }

   template<typename... Args>
   inline void Logger::Error(std::format_string<Args...> fmt, Args &&...args)
   {
      if (ShouldLog(LogType::ERR))
      {
         std::string msg = std::format(fmt, std::forward<Args>(args)...);
         LogInternal(LogType::ERR, msg);
      }
   }

   template<typename... Args>
   inline void Logger::ErrorWithHeader(std::string_view header, std::format_string<Args...> fmt, Args &&...args)
   {
      if (ShouldLog(LogType::ERR))
      {
         std::string msg = std::format("{}: {}", header, std::format(fmt, std::forward<Args>(args)...));
         LogInternal(LogType::ERR, msg);
      }
   }

   template<typename... Args>
   inline void Logger::Critical(std::format_string<Args...> fmt, Args &&...args)
   {
      std::string msg = std::format(fmt, std::forward<Args>(args)...);
      LogInternal(LogType::CRITICAL, msg);
   }

   template<typename... Args>
   inline void Logger::CriticalWithHeader(std::string_view header, std::format_string<Args...> fmt, Args &&...args)
   {
      std::string msg = std::format("{}: {}", header, std::format(fmt, std::forward<Args>(args)...));
      LogInternal(LogType::CRITICAL, msg);
   }
}