#pragma once
#include <drogon/nosql/RedisClient.h>
#include <drogon/utils/coroutine.h>
#include <trantor/net/EventLoopThread.h>
#include <nlohmann/json.hpp>
#include <atomic>
#include <functional>
#include <future>
#include <string>
#include <unordered_map>
#include <vector>

class TaskQueue {
public:
    using Handler    = std::function<drogon::Task<>(const nlohmann::json&)>;
    using Next       = std::function<drogon::Task<>()>;
    using Middleware = std::function<drogon::Task<>(const nlohmann::json& payload, const Next& next)>;
    // Runs once when a task gives up for good (last attempt failed, or PermanentTaskError). For cleanup.
    using OnDead     = std::function<drogon::Task<>(const nlohmann::json& payload, const std::string& error)>;
    TaskQueue(const std::string& host, int port, const std::string& password, int maxAttempts, int maxInFlight,
              const std::string& username = "", int db = 0);
    ~TaskQueue();

    TaskQueue(const TaskQueue&) = delete;
    TaskQueue& operator=(const TaskQueue&) = delete;

    void registerHandler(const std::string& type, Handler handler, std::vector<Middleware> middlewares = {}, OnDead onDead = nullptr);
    void start();
    void stop();
    void enqueue(const std::string& type, const nlohmann::json& payload);
    trantor::EventLoop* loop();

private:
    struct Entry {
        Handler handler;
        std::vector<Middleware> middlewares;
        OnDead onDead;
    };

    drogon::AsyncTask poll();
    drogon::AsyncTask execute(std::string raw);
    drogon::Task<> run(const Entry& entry, const nlohmann::json& payload, size_t index);
    drogon::Task<> push(const char* list, std::string value);

    trantor::EventLoopThread loopThread_;
    drogon::nosql::RedisClientPtr producer_;
    drogon::nosql::RedisClientPtr consumer_;
    std::unordered_map<std::string, Entry> handlers_;
    int maxAttempts_;
    int maxInFlight_;
    int inFlight_ = 0;
    bool started_ = false;
    std::atomic<bool> stopping_{false};
    std::promise<void> stopped_;

    static constexpr const char* QUEUE = "tasks";
    static constexpr const char* DEAD  = "tasks:dead";
};
