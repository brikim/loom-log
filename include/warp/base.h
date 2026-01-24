#pragma once

#include "warp/log/log.h"
#include "warp/log/logger.h"
#include "warp/log/log-types.h"

#include <format>
#include <optional>
#include <string_view>

namespace warp
{
   class Base
   {
   public:
      Base(std::string_view className,
           std::string_view ansiiCode,
           std::optional<std::string_view> classExtra);
      virtual ~Base() = default;

      template<typename... Args>
      void LogTrace(std::format_string<Args...> fmt, Args &&...args)
      {
         log::TraceWithHeader(header_, fmt, std::forward<Args>(args)...);
      }

      template<typename... Args>
      void LogInfo(std::format_string<Args...> fmt, Args &&...args)
      {
         log::InfoWithHeader(header_, fmt, std::forward<Args>(args)...);
      }

      template<typename... Args>
      void LogWarning(std::format_string<Args...> fmt, Args&&... args)
      {
         log::WarningWithHeader(header_, fmt, std::forward<Args>(args)...);
      }

      template<typename... Args>
      void LogError(std::format_string<Args...> fmt, Args &&...args)
      {
         log::ErrorWithHeader(header_, fmt, std::forward<Args>(args)...);
      }

      template<typename... Args>
      void Log(LogType type, std::string_view fmt, Args &&...args)
      {
         switch (type)
         {
            case LogType::TRACE:
               LogTrace(fmt, std::forward<Args>(args)...);
               break;
            case LogType::INFO:
               LogInfo(fmt, std::forward<Args>(args)...);
               break;
            case LogType::WARN:
               LogWarning(fmt, std::forward<Args>(args)...);
               break;
            case LogType::ERR:
               LogError(fmt, std::forward<Args>(args)...);
               break;
            default:
               break;
         }
      }

   private:
      std::string header_;
   };
}