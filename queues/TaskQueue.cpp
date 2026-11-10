#include "TaskQueue.hpp"
#include <trantor/utils/Logger.h>
#include <optional>

using drogon::nosql::RedisClient;
using drogon::nosql::RedisResult;

TaskQueue::TaskQueue(const std::string& host, int port, const std::string& password, int maxAttempts, int maxInFlight)
    : loopThread_("TaskQueue"), maxAttempts_(maxAttempts), maxInFlight_(maxInFlight) {
    const trantor::InetAddress address(host, static_cast<uint16_t>(port));
    producer_ = RedisClient::newRedisClient(address, 2, password);
    consumer_ = RedisClient::newRedisClient(address, 1, password);
    loopThread_.run();
}

TaskQueue::~TaskQueue() {
    stop();
}

trantor::EventLoop* TaskQueue::loop() {
    return loopThread_.getLoop();
}

void TaskQueue::registerHandler(const std::string& type, Handler handler, std::vector<Middleware> middlewares) {
    handlers_[type] = Entry{std::move(handler), std::move(middlewares)};
}

void TaskQueue::start() {
    started_ = true;
    loop()->queueInLoop([this]() { poll(); });
}

void TaskQueue::stop() {
    if (!started_ || stopping_.exchange(true)) return;
    stopped_.get_future().wait();
}

void TaskQueue::enqueue(const std::string& type, const nlohmann::json& payload) {
    const std::string task = nlohmann::json{{"type", type}, {"payload", payload}, {"attempts", 0}}.dump();
    producer_->execCommandAsync(
        [](const RedisResult&) {},
        [type](const std::exception& e) { LOG_ERROR << "TaskQueue: failed to enqueue " << type << ": " << e.what(); },
        "LPUSH %s %s", QUEUE, task.c_str());
}

drogon::AsyncTask TaskQueue::poll() {
    while (!stopping_) {
        if (inFlight_ >= maxInFlight_) {
            co_await drogon::sleepCoro(loop(), 0.05);
            continue;
        }

        std::optional<std::string> raw;
        bool failed = false;
        try {
            auto result = co_await consumer_->execCommandCoro("BRPOP %s %d", QUEUE, 2);
            if (!result.isNil()) raw = result.asArray()[1].asString();
        } catch (const std::exception& e) {
            LOG_ERROR << "TaskQueue: " << e.what();
            failed = true;
        }

        co_await drogon::switchThreadCoro(loop());
        if (failed) co_await drogon::sleepCoro(loop(), 1.0);
        if (raw) {
            ++inFlight_;
            execute(std::move(*raw));
        }
    }

    while (inFlight_ > 0) co_await drogon::sleepCoro(loop(), 0.05);
    stopped_.set_value();
}

drogon::AsyncTask TaskQueue::execute(std::string raw) {
    try {
        const char* list = nullptr;
        std::string value;

        auto task = nlohmann::json::parse(raw, nullptr, false);
        if (task.is_discarded() || !task.is_object()) {
            list  = DEAD;
            value = nlohmann::json{{"raw", raw}, {"last_error", "invalid task JSON"}}.dump();
        } else {
            const std::string type = task.value("type", "");
            auto it = handlers_.find(type);
            if (it == handlers_.end()) {
                task["last_error"] = "no handler for type: " + type;
                list  = DEAD;
                value = task.dump();
            } else {
                std::string error;
                try {
                    co_await run(it->second, task["payload"], 0);
                } catch (const std::exception& e) {
                    error = e.what();
                } catch (...) {
                    error = "unknown error";
                }

                if (!error.empty()) {
                    const int attempts = task.value("attempts", 0) + 1;
                    task["attempts"]   = attempts;
                    task["last_error"] = error;
                    list  = attempts < maxAttempts_ ? QUEUE : DEAD;
                    value = task.dump();
                    if (list == DEAD) LOG_WARN << "TaskQueue: " << type << " failed " << attempts << " times, moved to " << DEAD;
                }
            }
        }

        if (list) co_await push(list, std::move(value));
    } catch (const std::exception& e) {
        LOG_ERROR << "TaskQueue: " << e.what();
    }

    co_await drogon::switchThreadCoro(loop());
    --inFlight_;
}

drogon::Task<> TaskQueue::run(const Entry& entry, const nlohmann::json& payload, size_t index) {
    if (index == entry.middlewares.size()) {
        co_await entry.handler(payload);
        co_return;
    }
    co_await entry.middlewares[index](payload, [&]() { return run(entry, payload, index + 1); });
}

drogon::Task<> TaskQueue::push(const char* list, std::string value) {
    co_await producer_->execCommandCoro("LPUSH %s %s", list, value.c_str());
}
