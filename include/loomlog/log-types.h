#pragma once

#include <string>

namespace loomlog
{
   inline constexpr const std::string_view ANSI_CODE_START{"\33[38;5;"};
   inline constexpr const std::string_view ANSI_CODE_END{"m"};
   inline constexpr const std::string_view ANSI_CODE_RESET{"\033[49m"};

   inline const std::string ANSI_CODE_LOG_HEADER{std::format("{}249{}", ANSI_CODE_START, ANSI_CODE_END)};
   inline const std::string ANSI_CODE_LOG_INFO{std::format("{}75{}", ANSI_CODE_START, ANSI_CODE_END)};
   inline const std::string ANSI_CODE_LOG_WARNING{std::format("{}214{}", ANSI_CODE_START, ANSI_CODE_END)};
   inline const std::string ANSI_CODE_LOG_ERROR{std::format("{}196{}", ANSI_CODE_START, ANSI_CODE_END)};
   inline const std::string ANSI_CODE_LOG_CRITICAL{std::format("{}255;48;5;1{}", ANSI_CODE_START, ANSI_CODE_END)};
   inline const std::string ANSI_CODE_LOG_DEFAULT{std::format("{}8{}", ANSI_CODE_START, ANSI_CODE_END)};
   inline const std::string ANSI_CODE_LOG{std::format("{}15{}", ANSI_CODE_START, ANSI_CODE_END)};
   inline const std::string ANSI_CODE_TAG{std::format("{}37{}", ANSI_CODE_START, ANSI_CODE_END)};
   inline const std::string ANSI_CODE_STANDOUT{std::format("{}158{}", ANSI_CODE_START, ANSI_CODE_END)};

   inline const std::string ANSI_CODE_PLEX{std::format("{}220{}", ANSI_CODE_START, ANSI_CODE_END)};
   inline const std::string ANSI_CODE_EMBY{std::format("{}77{}", ANSI_CODE_START, ANSI_CODE_END)};
   inline const std::string ANSI_CODE_TAUTULLI{std::format("{}136{}", ANSI_CODE_START, ANSI_CODE_END)};
   inline const std::string ANSI_CODE_JELLYSTAT{std::format("{}63{}", ANSI_CODE_START, ANSI_CODE_END)};

   inline const std::string ANSI_FORMATTED_UNKNOWN("Unknown Server");
   inline const std::string ANSI_FORMATTED_PLEX(std::format("{}Plex{}", ANSI_CODE_PLEX, ANSI_CODE_LOG));
   inline const std::string ANSI_FORMATTED_EMBY(std::format("{}Emby{}", ANSI_CODE_EMBY, ANSI_CODE_LOG));
   inline const std::string ANSI_FORMATTED_TAUTULLI(std::format("{}Tautulli{}", ANSI_CODE_TAUTULLI, ANSI_CODE_LOG));
   inline const std::string ANSI_FORMATTED_JELLYSTAT(std::format("{}Jellystat{}", ANSI_CODE_JELLYSTAT, ANSI_CODE_LOG));

   enum class ApiType
   {
      PLEX,
      EMBY,
      TAUTULLI,
      JELLYSTAT
   };

   struct AppriseLoggingConfig
   {
      bool enabled{false};
      std::string url;
      std::string key;
      std::string message_title;
   };
}