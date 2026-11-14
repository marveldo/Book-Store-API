#pragma once
#include <drogon/nosql/RedisClient.h>
#include <drogon/utils/coroutine.h>
#include <trantor/net/EventLoop.h>
#include "queues/TaskQueue.hpp"
#include <chrono>
#include <string>
#include <vector>

class Scheduler {
public:
    Scheduler(TaskQueue* queue, const std::string& host, int port, const std::string& password,
              const std::string& username = "", int db = 0);
    ~Scheduler();

    Scheduler(const Scheduler&) = delete;
    Scheduler& operator=(const Scheduler&) = delete;

    void every(std::string task, std::chrono::seconds period);
    void start();

    bool knows(const std::string& task) const;
    drogon::Task<bool> trigger(std::string task);

private:
    struct Job {
        std::string task;
        std::chrono::seconds period;
    };

    const Job* find(const std::string& task) const;
    void fire(const std::string& task);

    TaskQueue* queue_;
    drogon::nosql::RedisClientPtr redis_;
    std::vector<Job> jobs_;
    std::vector<trantor::TimerId> timers_;
};
