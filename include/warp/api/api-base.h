#pragma once

#include "warp/api/api-response.h"
#include "warp/api/api-types.h"
#include "warp/base.h"
#include "warp/types.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace warp
{
   using ApiParams = std::vector<std::pair<std::string_view, std::string_view>>;

   class ApiBase : public Base
   {
   public:
      ApiBase(const ApiBaseData& data);
      virtual ~ApiBase();

      // Api tasks are optional. Api's can override to perform a task
      [[nodiscard]] virtual std::optional<std::vector<Task>> GetTaskList();

      virtual void Shutdown();

      [[nodiscard]] const std::string& GetName() const;
      [[nodiscard]] const std::string& GetPrettyName() const;
      [[nodiscard]] const std::string& GetUrl() const;
      [[nodiscard]] const std::string& GetApiKey() const;

      [[nodiscard]] virtual bool GetValid() = 0;
      [[nodiscard]] virtual std::optional<std::string> GetServerReportedName() = 0;

   protected:
      [[nodiscard]] virtual std::string_view GetApiBase() const = 0;
      [[nodiscard]] virtual std::string_view GetApiTokenName() const = 0;

      [[nodiscard]] std::string GetNextCronQuickTime() const;
      [[nodiscard]] std::string GetNextCronFullTime() const;

      [[nodiscard]] Response Get(const std::string& path, const Headers& headers);
      [[nodiscard]] Response Post(const std::string& path, const Headers& headers);
      [[nodiscard]] Response Post(const std::string& path, const Headers& headers, const std::string& body, const std::string& contentType);
      [[nodiscard]] Response Delete(const std::string& path, const Headers& headers);

      void AddApiParam(std::string& url, const ApiParams& params) const;
      [[nodiscard]] std::string BuildApiPath(std::string_view path) const;
      [[nodiscard]] std::string BuildApiParamsPath(std::string_view path, const ApiParams& params) const;

      // Returns if the http request was successful and outputs to the log if not successful
      bool IsHttpSuccess(std::string_view name, const Response& response, bool log = true);

   private:
      struct ApiBaseImpl;
      friend struct ApiBaseImpl;
      std::unique_ptr<ApiBaseImpl> pimpl_;
   };
}