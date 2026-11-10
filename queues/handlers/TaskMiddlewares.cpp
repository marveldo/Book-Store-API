#include "TaskMiddlewares.hpp"
#include <trantor/utils/Logger.h>
#include <chrono>

TaskQueue::Middleware timed(const std::string& name) {
    return [name](const nlohmann::json&, const TaskQueue::Next& next) -> drogon::Task<> {
        const auto start = std::chrono::steady_clock::now();
        co_await next();
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
        LOG_INFO << name << " took " << ms << "ms";
    };
}
