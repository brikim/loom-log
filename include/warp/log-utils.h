#pragma once

#include "warp/log-types.h"
#include "warp/types.h"

#include <concepts>
#include <format>
#include <string>
#include <type_traits>

template <typename T>
concept arithmetic = std::is_arithmetic_v<T>;

namespace warp
{
   template <typename T>
   inline std::string GetTag(std::string_view tag, const T& value)
   {
      // std::format will handle converting the value to a string 
      // regardless of whether it is a string, int, or bool.
      return std::format("{}{}{}[{}]", ANSI_CODE_TAG, tag, ANSI_CODE_LOG, value);
   }

   template <arithmetic T>
   inline std::string GetTag(std::string_view tag, const T& value, std::string_view fmt)
   {
      // Construct the dynamic format string: e.g., "{}{}{}[{:.2f}]"
      std::string dynamic_fmt = std::format("{}{}{}[{{:{}}}]",
                                            ANSI_CODE_TAG, tag, ANSI_CODE_LOG, fmt);

      return std::vformat(dynamic_fmt, std::make_format_args(value));
   }

   inline std::string GetAnsiText(std::string_view text, std::string_view ansiCode)
   {
      return std::format("{}{}{}", ansiCode, text, ANSI_CODE_LOG);
   }

   inline std::string GetStandoutText(std::string_view text)
   {
      return std::format("{}{}{}", ANSI_CODE_STANDOUT, text, ANSI_CODE_LOG);
   }

   inline std::string_view GetFormattedPlex()
   {
      return ANSI_FORMATTED_PLEX;
   }

   inline std::string_view GetFormattedEmby()
   {
      return ANSI_FORMATTED_EMBY;
   }

   inline std::string_view GetFormattedTautulli()
   {
      return ANSI_FORMATTED_TAUTULLI;
   }

   inline std::string_view GetFormattedJellystat()
   {
      return ANSI_FORMATTED_JELLYSTAT;
   }

   inline std::string GetServiceHeader(std::string_view ansiiCode, std::string_view name)
   {
      return std::format("{}{}{}", ansiiCode, name, ANSI_CODE_LOG);
   }

   inline std::string_view GetFormattedApiName(ApiType type)
   {
      switch (type)
      {
         case ApiType::PLEX:
            return GetFormattedPlex();
         case ApiType::EMBY:
            return GetFormattedEmby();
         case ApiType::TAUTULLI:
            return GetFormattedTautulli();
         case ApiType::JELLYSTAT:
            return GetFormattedJellystat();
         default:
            return ANSI_FORMATTED_UNKNOWN;
      }
   }
}