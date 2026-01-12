#pragma once

#include "loomlog/log-types.h"

#include <format>
#include <regex>
#include <string>

namespace loomlog
{
   template <typename T>
   inline std::string GetTag(std::string_view tag, const T& value)
   {
      // std::format will handle converting the value to a string 
      // regardless of whether it is a string, int, or bool.
      return std::format("{}{}{}={}", ANSI_CODE_TAG, tag, ANSI_CODE_LOG, value);
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

   inline std::string GetServerName(std::string_view server, std::string_view serverInstance)
   {
      return serverInstance.empty()
         ? std::string(server)
         : std::format("{}({})", server, serverInstance);
   }

   inline std::string BuildSyncServerString(std::string_view currentServerList, std::string_view newServer, std::string_view newServerInstance)
   {
      if (currentServerList.empty())
      {
         return GetServerName(newServer, newServerInstance);
      }
      else
      {
         return newServer.empty()
            ? std::format("{},{}", currentServerList, newServer)
            : std::format("{},{}({})", currentServerList, newServer, newServerInstance);
      }
   }

   inline std::string StripAsciiCharacters(const std::string& data)
   {
      // Strip ansii codes from the log msg
      const std::regex ansii(R"(\x1B(?:[@-Z\\-_]|\[[0-?]*[ -/]*[@-~]))");
      return std::regex_replace(data, ansii, "");
   }

   inline std::string ToLower(std::string data)
   {
      std::transform(data.begin(), data.end(), data.begin(),
         [](unsigned char c) { return std::tolower(c); });
      return data;
   }
}