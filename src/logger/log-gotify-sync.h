#pragma once

#include "warp/utils.h"

#include <httplib.h>
#include <spdlog/sinks/base_sink.h>

#include <mutex>
#include <string>

namespace warp
{
   template<typename Mutex>
   class LogGotifySink : public spdlog::sinks::base_sink<Mutex>
   {
   public:
      explicit LogGotifySink(const GotifyLoggingConfig& config)
         : client_(config.url)
         , title_(config.message_title)
         , priority_{config.priority}
         , headers_{{"X-Gotify-Key", config.key}}
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
            {"message", message},
            {"priority", std::to_string(priority_)}
         };
         auto res = client_.Post("/message", headers_, params);
      }

      void flush_() override
      {
      }

   private:
      httplib::Client client_;
      std::string title_;
      int32_t priority_{0};
      httplib::Headers headers_;
   };

   using gotify_sink_mt = LogGotifySink<std::mutex>;
}