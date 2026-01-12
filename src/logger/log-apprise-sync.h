#pragma once

#include "warp/utils.h"

#include <httplib.h>
#include <spdlog/sinks/base_sink.h>

#include <mutex>
#include <regex>
#include <string>

namespace warp
{
   template<typename Mutex>
   class apprise_sink : public spdlog::sinks::base_sink<Mutex>
   {
   public:
      explicit apprise_sink(const AppriseLoggingConfig& config)
         : client_(config.url)
         , key_(config.key)
         , title_(config.message_title)
      {
      }

   protected:
      void sink_it_(const spdlog::details::log_msg& msg) override
      {
         spdlog::memory_buf_t formatted;
         spdlog::sinks::base_sink<Mutex>::formatter_->format(msg, formatted);

         auto message = StripAnsiiCharacters(fmt::to_string(formatted));

         size_t pos = 0;
         while ((pos = message.find('"', pos)) != std::string::npos)
         {
            message.replace(pos, 1, "\\\"");
            pos += 2;
         }

         httplib::Params params{
            {"title", title_},
            {"body", message}
         };
         auto res = client_.Post(std::format("/notify/{}", key_), params);
      }

      void flush_() override
      {
      }

   private:
      httplib::Client client_;
      std::string key_;
      std::string title_;
   };

   using apprise_sink_mt = apprise_sink<std::mutex>;
}