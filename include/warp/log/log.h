#pragma once

#include "warp/log/logger.h"

#include <string_view>

namespace warp::log
{
   // Init file logging
   inline void InitFileLogging(std::string_view path, std::string_view filename)
   {
      Logger::Instance().InitFileLogging(path, filename);
   }

   // Init Apprise logging
   inline void InitApprise(const AppriseLoggingConfig& config)
   {
      Logger::Instance().InitApprise(config);
   }

   // Init Gotify logging
   inline void InitGotify(const GotifyLoggingConfig& config)
   {
      Logger::Instance().InitGotify(config);
   }

   template<typename... Args>
   inline void Trace(std::format_string<Args...> fmt, Args &&...args)
   {
      Logger::Instance().Trace(fmt, std::forward<Args>(args)...);
   }

   template<typename... Args>
   inline void TraceWithHeader(std::string_view header, std::format_string<Args...> fmt, Args &&...args)
   {
      Logger::Instance().TraceWithHeader(header, fmt, std::forward<Args>(args)...);
   }

   template<typename... Args>
   inline void Info(std::format_string<Args...> fmt, Args &&...args)
   {
      Logger::Instance().Info(fmt, std::forward<Args>(args)...);
   }

   template<typename... Args>
   inline void InfoWithHeader(std::string_view header, std::format_string<Args...> fmt, Args &&...args)
   {
      Logger::Instance().InfoWithHeader(header, fmt, std::forward<Args>(args)...);
   }

   template<typename... Args>
   inline void Warning(std::format_string<Args...> fmt, Args &&...args)
   {
      Logger::Instance().Warning(fmt, std::forward<Args>(args)...);
   }

   template<typename... Args>
   inline void WarningWithHeader(std::string_view header, std::format_string<Args...> fmt, Args &&...args)
   {
      Logger::Instance().WarningWithHeader(header, fmt, std::forward<Args>(args)...);
   }

   template<typename... Args>
   inline void Error(std::format_string<Args...> fmt, Args &&...args)
   {
      Logger::Instance().Error(fmt, std::forward<Args>(args)...);
   }

   template<typename... Args>
   inline void ErrorWithHeader(std::string_view header, std::format_string<Args...> fmt, Args &&...args)
   {
      Logger::Instance().ErrorWithHeader(header, fmt, std::forward<Args>(args)...);
   }

   template<typename... Args>
   inline void Critical(std::format_string<Args...> fmt, Args &&...args)
   {
      Logger::Instance().Critical(fmt, std::forward<Args>(args)...);
   }

   template<typename... Args>
   inline void CriticalWithHeader(std::string_view header, std::format_string<Args...> fmt, Args &&...args)
   {
      Logger::Instance().CriticalWithHeader(header, fmt, std::forward<Args>(args)...);
   }
};