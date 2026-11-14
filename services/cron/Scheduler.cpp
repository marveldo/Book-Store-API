#include "Scheduler.hpp"
#include <trantor/utils/Logger.h>
#include <exception>

Scheduler::Scheduler(TaskQueue* queue, const std::string& host, int port, const std::string& password,
                     const std::string& username, int db)
    : queue_(queue),
      redis_(drogon::nosql::RedisClient::newRedisClient(trantor::InetAddress(host, static_cast<uint16_t>(port)), 1,
                                                        password, static_cast<unsigned int>(db), username)) {}

Scheduler::~Scheduler() {
    for (const auto id : timers_) queue_->loop()->invalidateTimer(id);
}

void Scheduler::every(std::string task, std::chrono::seconds period) {
    jobs_.push_back(Job{std::move(task), period});
}

void Scheduler::start() {
    for (const auto& job : jobs_) {
        const std::string task = job.task;
        timers_.push_back(queue_->loop()->runEvery(std::chrono::duration<double>(job.period), [this, task]() { fire(task); }));
        queue_->loop()->queueInLoop([this, task]() { fire(task); });
    }
}

bool Scheduler::knows(const std::string& task) const {
    return find(task) != nullptr;
}

const Scheduler::Job* Scheduler::find(const std::string& task) const {
    for (const auto& job : jobs_) {
        if (job.task == task) return &job;
    }
    return nullptr;
}

void Scheduler::fire(const std::string& task) {
    drogon::async_run([this, task]() -> drogon::Task<> {
        try {
            co_await trigger(task);
        } catch (const std::exception& e) {
            LOG_WARN << "Scheduler: " << task << " was not queued: " << e.what();
        }
    });
}

drogon::Task<bool> Scheduler::trigger(std::string task) {
    const Job* job = find(task);
    if (!job) co_return false;

    const auto now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch());
    const long long period = job->period.count();
    const std::string key = "cron:" + task + ":" + std::to_string(now.count() / period);

    const auto claimed = co_await redis_->execCommandCoro("SET %s 1 NX EX %lld", key.c_str(), period);
    if (claimed.isNil()) co_return false;

    queue_->enqueue(task, nlohmann::json::object());
    LOG_INFO << "Scheduler: queued " << task;
    co_return true;
}
