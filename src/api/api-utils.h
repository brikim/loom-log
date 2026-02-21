#pragma once

#include <glaze/json/write.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <list>
#include <numeric>
#include <string>
#include <string_view>
#include <vector>

namespace warp
{
   const std::string APPLICATION_JSON{"application/json"};

   inline std::string BuildCommaSeparatedList(const std::vector<std::string>& list)
   {
      if (list.empty()) return "";

      return std::accumulate(std::next(list.begin()), list.end(), list[0],
         [](std::string a, const std::string& b) {
         return std::move(a) + "," + b;
      });
   }

   inline std::string GetPercentEncoded(std::string_view src)
   {
      // 1. Lookup table for "unreserved" characters (RFC 3986)
      // 0 = needs encoding, 1 = safe
      static constexpr auto SAFE = []() {
         std::array<std::uint8_t, 256> table{{0}};
         // Fill unreserved characters (RFC 3986)
         for (int i = '0'; i <= '9'; ++i) table[i] = 1;
         for (int i = 'A'; i <= 'Z'; ++i) table[i] = 1;
         for (int i = 'a'; i <= 'z'; ++i) table[i] = 1;
         table['-'] = 1;
         table['.'] = 1;
         table['_'] = 1;
         table['~'] = 1;
         return table;
      }();

      static constexpr char hex_chars[] = "0123456789ABCDEF";

      // 2. Pre-calculate exact size to avoid reallocations
      size_t new_size{0};
      for (unsigned char c : src)
      {
         new_size += SAFE[c] ? 1 : 3;
      }

      // 3. One single allocation
      std::string result;
      result.reserve(new_size);

      // 4. Direct pointer-style insertion
      for (unsigned char c : src)
      {
         if (SAFE[c])
         {
            result.push_back(c);
         }
         else
         {
            result.push_back('%');
            result.push_back(hex_chars[c >> 4]);   // High nibble
            result.push_back(hex_chars[c & 0x0F]); // Low nibble
         }
      }

      return result;
   }

   inline std::string ParamsToJson(const std::list<std::pair<std::string_view, std::string_view>> params)
   {
      // Copy into a standard map so Glaze recognizes the structure
      std::map<std::string_view, std::string_view> m;
      for (const auto& [key, value] : params)
      {
         m[key] = value;
      }

      return glz::write_json(m).value_or("{}");
   }
}