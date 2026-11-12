#pragma once
#include <drogon/nosql/RedisClient.h>
#include <drogon/nosql/RedisSubscriber.h>
#include <drogon/utils/coroutine.h>
#include <json/json.h>
#include <functional>
#include <optional>
#include <string>
#include <vector>

struct JobStatus {
    std::string jobId;
    std::string bookId;
    std::string userId;
    std::string status;      // queued | processing | completed | failed
    std::string error;
    int pagesDone  = 0;
    int pagesTotal = 0;
    std::vector<std::string> warnings;   // set when completed: what was left out

    bool finished() const { return status == "completed" || status == "failed"; }

    Json::Value toJson() const;      // what clients see (no user_id)
    std::string toMessage() const;   // toJson() as one line, for the WebSocket

    static JobStatus fromHash(const Json::Value& hash);   // the JSON Redis publishes
};

// Background jobs (PDF extraction for now), stored in Redis so every server
// instance sees the same state:
//
//   job:<id>          hash with the job's status, expires a day after its last update
//   job-updates:<id>  channel: every change is PUBLISHed here as JSON
//   lock:book:<id>    one PDF per book at a time
//
// Whichever instance runs the task writes; whichever instance holds the client's
// WebSocket listens on the channel (see JobsController). They don't have to be the same.
class JobService {
public:
    using UpdateCallback = std::function<void(const JobStatus&)>;

    JobService(const std::string& host, int port, const std::string& password, const std::string& username = "", int db = 0);

    // Book lock
    drogon::Task<bool> lockBook(const std::string& bookId, const std::string& jobId);
    drogon::Task<> unlockBook(const std::string& bookId, const std::string& jobId);

    // Storing: these only write the job:<id> hash. Nobody is told until you call publish.
    drogon::Task<> create(const std::string& jobId, const std::string& bookId, const std::string& userId);
    drogon::Task<> setStatus(const std::string& jobId, const std::string& status, const std::string& error = "");
    drogon::Task<> complete(const std::string& jobId, int pages, const std::vector<std::string>& warnings = {});
    void setProgress(const std::string& jobId, int done, int total);   // fire and forget, any thread

    drogon::Task<std::optional<JobStatus>> get(const std::string& jobId);

    // Publishing: sends the job's current hash to everyone watching it, on any instance.
    drogon::Task<> publish(const std::string& jobId);
    void publishAsync(const std::string& jobId);   // fire and forget, any thread

    // Listening: get every publish for one job on this instance. One callback per job: the
    // controller fans it out to its own sockets.
    void watch(const std::string& jobId, UpdateCallback onUpdate);
    void unwatch(const std::string& jobId);

private:
    drogon::nosql::RedisClientPtr     redis_;
    std::shared_ptr<drogon::nosql::RedisSubscriber> subscriber_;   // one Redis connection for all subscriptions

    static constexpr int JOB_TTL_SECONDS  = 24 * 60 * 60;
    static constexpr int LOCK_TTL_SECONDS = 2 * 60 * 60;   // a lost job frees its book after 2 hours
};
