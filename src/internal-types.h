#pragma once

#include "loomlog/log-types.h"

#include <format>
#include <string>

namespace loomlog
{
   // Log pattern to be used by the logger
   inline const std::string DEFAULT_PATTERN{"%m/%d/%Y %T [%l] %v"};
   inline const std::string DEFAULT_PATTERN_ANSII_INFO{std::format("{}%m/%d/%Y %T {}[{}%l{}] %v{}", ANSI_CODE_LOG_HEADER, ANSI_CODE_LOG, ANSI_CODE_LOG_INFO, ANSI_CODE_LOG, ANSI_CODE_RESET)};
   inline const std::string DEFAULT_PATTERN_ANSII_WARNING{std::format("{}%m/%d/%Y %T {}[{}%l{}] %v{}", ANSI_CODE_LOG_HEADER, ANSI_CODE_LOG, ANSI_CODE_LOG_WARNING, ANSI_CODE_LOG, ANSI_CODE_RESET)};
   inline const std::string DEFAULT_PATTERN_ANSII_ERROR{std::format("{}%m/%d/%Y %T {}[{}%l{}] %v{}", ANSI_CODE_LOG_HEADER, ANSI_CODE_LOG, ANSI_CODE_LOG_ERROR, ANSI_CODE_LOG, ANSI_CODE_RESET)};
   inline const std::string DEFAULT_PATTERN_ANSII_CRITICAL{std::format("{}%m/%d/%Y %T {}[{}%l{}{}] %v{}", ANSI_CODE_LOG_HEADER, ANSI_CODE_LOG, ANSI_CODE_LOG_CRITICAL, ANSI_CODE_RESET, ANSI_CODE_LOG, ANSI_CODE_RESET)};
   inline const std::string DEFAULT_PATTERN_ANSII_DEFAULT{std::format("{}%m/%d/%Y %T {}[{}%l{}] %v{}", ANSI_CODE_LOG_HEADER, ANSI_CODE_LOG, ANSI_CODE_LOG_DEFAULT, ANSI_CODE_LOG, ANSI_CODE_RESET)};
}