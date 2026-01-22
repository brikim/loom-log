#pragma once

#include <filesystem>
#include <format>
#include <regex>
#include <string>

namespace warp
{
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

   inline std::string StripAnsiiCharacters(const std::string& data)
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

   inline std::string GetDisplayFolder(std::string_view path)
   {
      namespace fs = std::filesystem;
      fs::path p(path);

      // If path ends in a slash, some implementations return an empty filename.
      // We want the actual last directory component.
      if (p.has_relative_path() && p.filename().empty())
      {
         p = p.parent_path();
      }

      auto lastElement = p.filename();
      auto lastStr = lastElement.string();

      // Check for "Season" in the last folder name
      // (Matches "Season 01", "Specials", etc. if they contain "Season")
      if (lastStr.find("Season") != std::string::npos)
      {
         if (p.has_parent_path())
         {
            // Returns "ShowName/Season 01"
            return (p.parent_path().filename() / lastElement).generic_string();
         }
      }

      return lastStr.empty() ? std::string(path) : lastStr;
   }
}