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

// Background tasks in Redis lists, run as coroutines on one event loop thread.
//
// Tasks are grouped into lanes. Each lane is its own Redis list with its own limit on how
// many of its tasks run at once, so slow, heavy tasks (PDF imports) can't take the places
// of quick ones (emails). The default lane is the "tasks" list; a lane called "pdf" is
// "tasks:pdf". Waiting tasks stay in Redis, where any server instance with room can take them.
class TaskQueue {
public:
    using Handler    = std::function<drogon::Task<>(const nlohmann::json&)>;
    using Next       = std::function<drogon::Task<>()>;
    using Middleware = std::function<drogon::Task<>(const nlohmann::json& payload, const Next& next)>;
    // Runs once when a task gives up for good (last attempt failed, or PermanentTaskError). For cleanup.
    using OnDead     = std::function<drogon::Task<>(const nlohmann::json& payload, const std::string& error)>;

    // maxInFlight is the default lane's limit.
    TaskQueue(const std::string& host, int port, const std::string& password, int maxAttempts, int maxInFlight,
              const std::string& username = "", int db = 0);
    ~TaskQueue();

    TaskQueue(const TaskQueue&) = delete;
    TaskQueue& operator=(const TaskQueue&) = delete;

    // Call before start(). Task types are put in a lane when they're registered.
    void addLane(const std::string& name, int maxInFlight);
    void registerHandler(const std::string& type, Handler handler, std::vector<Middleware> middlewares = {},
                         OnDead onDead = nullptr, const std::string& lane = "");

    void start();
    void stop();
    void enqueue(const std::string& type, const nlohmann::json& payload);
    trantor::EventLoop* loop();

private:
    struct Lane {
        std::string list;                          // Redis list: "tasks" or "tasks:<name>"
        int maxInFlight;
        int inFlight = 0;
        drogon::nosql::RedisClientPtr consumer;    // its own connection: BRPOP blocks the connection it runs on
    };

    struct Entry {
        Handler handler;
        std::vector<Middleware> middlewares;
        OnDead onDead;
        std::string lane;
    };

    drogon::AsyncTask poll(Lane* lane);
    drogon::AsyncTask execute(Lane* lane, std::string raw);
    drogon::Task<> run(const Entry& entry, const nlohmann::json& payload, size_t index);
    drogon::Task<> push(std::string list, std::string value);
    Lane& laneFor(const std::string& type);

    trantor::EventLoopThread loopThread_;
    trantor::InetAddress address_;
    std::string password_;
    std::string username_;
    int db_;

    drogon::nosql::RedisClientPtr producer_;
    std::unordered_map<std::string, Lane> lanes_;     // name -> lane; "" is the default
    std::unordered_map<std::string, Entry> handlers_;
    int maxAttempts_;
    int pollers_ = 0;                                 // poll loops still running
    bool started_ = false;
    std::atomic<bool> stopping_{false};
    std::promise<void> stopped_;

    static constexpr const char* QUEUE = "tasks";
    static constexpr const char* DEAD  = "tasks:dead";
};
