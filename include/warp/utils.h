#pragma once

#include <algorithm>
#include <cctype>
#include <cwctype>
#include <filesystem>
#include <format>
#include <regex>
#include <string>
#include <string_view>

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
         return newServerInstance.empty()
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

   template <typename CharT>
   inline std::basic_string<CharT> ToLower(std::basic_string<CharT> data)
   {
      std::transform(data.begin(), data.end(), data.begin(), [](auto c) {
         if constexpr (std::is_same_v<CharT, wchar_t>)
         {
            return static_cast<wchar_t>(std::towlower(static_cast<std::wint_t>(c)));
         }
         else
         {
            return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
         }
      });
      return data;
   }

   template <typename StringT>
   inline bool ContainsSeason(const StringT& str)
   {
      if (str.size() < 6) return false;

      auto isSeasonMatch = [](auto ch, char target) {
         return (ch == target) || (ch == (target - 32));
      };
      constexpr std::string_view target = "season";
      auto it = std::search(str.begin(), str.end(), target.begin(), target.end(),
          [&isSeasonMatch](auto a, char b) {
         return isSeasonMatch(a, b);
      });

      return it != str.end();
   }

   inline std::string GetDisplayFolder(std::string_view path)
   {
      namespace fs = std::filesystem;
      if (path.empty()) return "";

      fs::path p(path);

      if (p.has_relative_path() && p.filename().empty())
      {
         p = p.parent_path();
      }

      const bool hasFile = p.has_extension();
      const fs::path folderPath = hasFile ? p.parent_path() : p;

      fs::path result;
      if (ContainsSeason(folderPath.filename().native()) && folderPath.has_parent_path())
      {
         result = folderPath.parent_path().filename() / folderPath.filename();
         if (hasFile) result /= p.filename();
      }
      else if (hasFile)
      {
         result = folderPath.filename() / p.filename();
      }
      else
      {
         result = folderPath.filename();
      }

      std::string out = result.generic_string();
      return out.empty() ? std::string(path) : out;
   }
}