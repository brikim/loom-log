#pragma once

#include <spdlog/formatter.h>
#include <spdlog/pattern_formatter.h>

#include <string>

namespace loomlog
{
   // Class used to strip the ansii character strings from a log message
   class AnsiiRemoveFormatter : public spdlog::formatter
   {
   public:
      AnsiiRemoveFormatter();
      virtual ~AnsiiRemoveFormatter() = default;

      void format(const spdlog::details::log_msg& msg, spdlog::memory_buf_t& dest) override;
      std::unique_ptr<spdlog::formatter> clone() const override;

   private:
      spdlog::pattern_formatter patternFormatter_;
   };
}