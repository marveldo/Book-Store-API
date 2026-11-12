#include "TaskQueue.hpp"
#include "TaskErrors.hpp"
#include <trantor/utils/Logger.h>
#include <algorithm>
#include <optional>
#include <stdexcept>

using drogon::nosql::RedisClient;
using drogon::nosql::RedisResult;

TaskQueue::TaskQueue(const std::string& host, int port, const std::string& password, int maxAttempts, int maxInFlight,
                     const std::string& username, int db)
    : loopThread_("TaskQueue"),
      address_(host, static_cast<uint16_t>(port)),
      password_(password),
      username_(username),
      db_(db),
      maxAttempts_(maxAttempts) {
    producer_ = RedisClient::newRedisClient(address_, 2, password_, static_cast<unsigned int>(db_), username_);
    addLane("", maxInFlight);
    loopThread_.run();
}

TaskQueue::~TaskQueue() {
    stop();
}

trantor::EventLoop* TaskQueue::loop() {
    return loopThread_.getLoop();
}

void TaskQueue::addLane(const std::string& name, int maxInFlight) {
    if (started_) throw std::logic_error("TaskQueue: add lanes before start()");
    Lane lane;
    lane.list        = name.empty() ? QUEUE : std::string(QUEUE) + ":" + name;
    lane.maxInFlight = std::max(1, maxInFlight);
    lane.consumer    = RedisClient::newRedisClient(address_, 1, password_, static_cast<unsigned int>(db_), username_);
    lanes_[name] = std::move(lane);
}

void TaskQueue::registerHandler(const std::string& type, Handler handler, std::vector<Middleware> middlewares,
                                OnDead onDead, const std::string& lane) {
    if (!lanes_.count(lane)) throw std::logic_error("TaskQueue: no lane called '" + lane + "' (call addLane first)");
    handlers_[type] = Entry{std::move(handler), std::move(middlewares), std::move(onDead), lane};
}

TaskQueue::Lane& TaskQueue::laneFor(const std::string& type) {
    auto it = handlers_.find(type);
    return lanes_.at(it == handlers_.end() ? "" : it->second.lane);   // unknown types go to the default lane, then to dead
}

void TaskQueue::start() {
    started_ = true;
    pollers_ = static_cast<int>(lanes_.size());
    loop()->queueInLoop([this]() {
        for (auto& [name, lane] : lanes_) poll(&lane);
    });
}

void TaskQueue::stop() {
    if (!started_ || stopping_.exchange(true)) return;
    stopped_.get_future().wait();
}

void TaskQueue::enqueue(const std::string& type, const nlohmann::json& payload) {
    const std::string task = nlohmann::json{{"type", type}, {"payload", payload}, {"attempts", 0}}.dump();
    const std::string list = laneFor(type).list;
    producer_->execCommandAsync(
        [](const RedisResult&) {},
        [type](const std::exception& e) { LOG_ERROR << "TaskQueue: failed to enqueue " << type << ": " << e.what(); },
        "LPUSH %s %s", list.c_str(), task.c_str());
}

// One of these per lane. It only takes a task from Redis when the lane has room, so tasks
// that can't run yet wait in Redis rather than in this server's memory.
drogon::AsyncTask TaskQueue::poll(Lane* lane) {
    while (!stopping_) {
        if (lane->inFlight >= lane->maxInFlight) {
            co_await drogon::sleepCoro(loop(), 0.05);
            continue;
        }

        std::optional<std::string> raw;
        bool failed = false;
        try {
            auto result = co_await lane->consumer->execCommandCoro("BRPOP %s %d", lane->list.c_str(), 2);
            if (!result.isNil()) raw = result.asArray()[1].asString();
        } catch (const std::exception& e) {
            LOG_ERROR << "TaskQueue (" << lane->list << "): " << e.what();
            failed = true;
        }

        co_await drogon::switchThreadCoro(loop());
        if (failed) co_await drogon::sleepCoro(loop(), 1.0);
        if (raw) {
            ++lane->inFlight;
            execute(lane, std::move(*raw));
        }
    }

    while (lane->inFlight > 0) co_await drogon::sleepCoro(loop(), 0.05);
    if (--pollers_ == 0) stopped_.set_value();   // the last lane to finish lets stop() return
}

drogon::AsyncTask TaskQueue::execute(Lane* lane, std::string raw) {
    try {
        std::string list;
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
                bool failed    = false;
                bool permanent = false;
                try {
                    co_await run(it->second, task["payload"], 0);
                } catch (const PermanentTaskError& e) {
                    error = e.what();
                    failed = permanent = true;
                } catch (const std::exception& e) {
                    error = e.what();
                    failed = true;
                } catch (...) {
                    error = "unknown error";
                    failed = true;
                }

                if (failed) {
                    const int attempts = task.value("attempts", 0) + 1;
                    task["attempts"]   = attempts;
                    task["last_error"] = error;
                    // A retry goes to the back of its own lane.
                    list  = (!permanent && attempts < maxAttempts_) ? lane->list : DEAD;
                    value = task.dump();

                    if (list == DEAD) {
                        LOG_WARN << "TaskQueue: " << type << (permanent ? " failed permanently" : " failed " + std::to_string(attempts) + " times")
                                 << ", moved to " << DEAD << ": " << error;
                        if (it->second.onDead) {
                            try {
                                co_await it->second.onDead(task["payload"], error);
                            } catch (const std::exception& e) {
                                LOG_ERROR << "TaskQueue: cleanup for " << type << " failed: " << e.what();
                            }
                        }
                    }
                }
            }
        }

        if (!list.empty()) co_await push(std::move(list), std::move(value));
    } catch (const std::exception& e) {
        LOG_ERROR << "TaskQueue: " << e.what();
    }

    co_await drogon::switchThreadCoro(loop());
    --lane->inFlight;
}

drogon::Task<> TaskQueue::run(const Entry& entry, const nlohmann::json& payload, size_t index) {
    if (index == entry.middlewares.size()) {
        co_await entry.handler(payload);
        co_return;
    }
    co_await entry.middlewares[index](payload, [&]() { return run(entry, payload, index + 1); });
}

drogon::Task<> TaskQueue::push(std::string list, std::string value) {
    co_await producer_->execCommandCoro("LPUSH %s %s", list.c_str(), value.c_str());
}
