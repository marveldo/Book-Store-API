#include "JobService.hpp"
#include <trantor/utils/Logger.h>
#include <exception>
#include <sstream>

using drogon::Task;
using drogon::nosql::RedisClient;
using drogon::nosql::RedisResult;

namespace {
    // Reads the whole job hash and PUBLISHes it as JSON, so listeners always get the
    // full job, not just the fields that changed. A script runs inside Redis, so this is
    // one round trip instead of HGETALL here + PUBLISH back.
    //   KEYS[1] = job:<id>   ARGV[1] = job-updates:<id>
    constexpr const char* PUBLISH_SCRIPT = R"lua(
        local flat = redis.call('HGETALL', KEYS[1])
        if #flat == 0 then return 0 end
        local job = {}
        for i = 1, #flat, 2 do job[flat[i]] = flat[i + 1] end
        return redis.call('PUBLISH', ARGV[1], cjson.encode(job))
    )lua";

    // Delete the lock only if this job still holds it. As two separate commands (GET, then DEL)
    // another job could take the lock in between; a script runs as one step.
    constexpr const char* UNLOCK_SCRIPT =
        "if redis.call('GET', KEYS[1]) == ARGV[1] then return redis.call('DEL', KEYS[1]) else return 0 end";

    std::string jobKey(const std::string& jobId)   { return "job:" + jobId; }
    std::string channel(const std::string& jobId)  { return "job-updates:" + jobId; }
    std::string lockKey(const std::string& bookId) { return "lock:book:" + bookId; }

    int toInt(const Json::Value& value) {
        if (value.isInt()) return value.asInt();
        try { return std::stoi(value.asString()); } catch (...) { return 0; }
    }
}

Json::Value JobStatus::toJson() const {
    Json::Value json;
    json["job_id"]      = jobId;
    json["book_id"]     = bookId;
    json["status"]      = status;
    json["pages_done"]  = pagesDone;
    json["pages_total"] = pagesTotal;
    json["error"]       = error.empty() ? Json::Value(Json::nullValue) : Json::Value(error);
    json["warnings"]    = Json::Value(Json::arrayValue);
    for (const auto& warning : warnings) json["warnings"].append(warning);
    return json;
}

std::string JobStatus::toMessage() const {
    Json::StreamWriterBuilder writer;
    writer["indentation"] = "";
    return Json::writeString(writer, toJson());
}

JobStatus JobStatus::fromHash(const Json::Value& hash) {
    JobStatus job;
    job.jobId      = hash.get("job_id", "").asString();
    job.bookId     = hash.get("book_id", "").asString();
    job.userId     = hash.get("user_id", "").asString();
    job.status     = hash.get("status", "").asString();
    job.error      = hash.get("error", "").asString();
    job.pagesDone  = toInt(hash.get("pages_done", 0));
    job.pagesTotal = toInt(hash.get("pages_total", 0));

    // Stored in the hash as a JSON list in a string: '["120 pictures were left out ..."]'
    const std::string warnings = hash.get("warnings", "").asString();
    if (!warnings.empty()) {
        Json::Value list;
        Json::CharReaderBuilder reader;
        std::string errors;
        std::istringstream in(warnings);
        if (Json::parseFromStream(reader, in, &list, &errors) && list.isArray()) {
            for (const auto& warning : list) job.warnings.push_back(warning.asString());
        }
    }
    return job;
}

JobService::JobService(const std::string& host, int port, const std::string& password, const std::string& username, int db) {
    // One connection, so commands reach Redis in the order they were sent
    // (a progress update can never land after "completed").
    redis_ = RedisClient::newRedisClient(trantor::InetAddress(host, static_cast<uint16_t>(port)), 1, password,
                                         static_cast<unsigned int>(db), username);
    subscriber_ = redis_->newSubscriber();
}

// ---- book lock --------------------------------------------------------------

Task<bool> JobService::lockBook(const std::string& bookId, const std::string& jobId) {
    auto result = co_await redis_->execCommandCoro("SET %s %s NX EX %d", lockKey(bookId).c_str(), jobId.c_str(), LOCK_TTL_SECONDS);
    co_return !result.isNil();   // nil = the key already existed
}

Task<> JobService::unlockBook(const std::string& bookId, const std::string& jobId) {
    co_await redis_->execCommandCoro("EVAL %s 1 %s %s", UNLOCK_SCRIPT, lockKey(bookId).c_str(), jobId.c_str());
}

// ---- storing (HSET into the job:<id> hash) ------------------------------------

Task<> JobService::create(const std::string& jobId, const std::string& bookId, const std::string& userId) {
    const std::string key = jobKey(jobId);
    co_await redis_->execCommandCoro(
        "HSET %s job_id %s book_id %s user_id %s status queued error %s pages_done 0 pages_total 0",
        key.c_str(), jobId.c_str(), bookId.c_str(), userId.c_str(), "");
    co_await redis_->execCommandCoro("EXPIRE %s %d", key.c_str(), JOB_TTL_SECONDS);
}

Task<> JobService::setStatus(const std::string& jobId, const std::string& status, const std::string& error) {
    const std::string key = jobKey(jobId);
    co_await redis_->execCommandCoro("HSET %s status %s error %s", key.c_str(), status.c_str(), error.c_str());
    co_await redis_->execCommandCoro("EXPIRE %s %d", key.c_str(), JOB_TTL_SECONDS);
}

Task<> JobService::complete(const std::string& jobId, int pages, const std::vector<std::string>& warnings) {
    const std::string key = jobKey(jobId);
    Json::Value list(Json::arrayValue);
    for (const auto& warning : warnings) list.append(warning);
    Json::StreamWriterBuilder writer;
    writer["indentation"] = "";
    const std::string warningsJson = Json::writeString(writer, list);

    co_await redis_->execCommandCoro("HSET %s status completed error %s pages_done %d pages_total %d warnings %s",
                                     key.c_str(), "", pages, pages, warningsJson.c_str());
    co_await redis_->execCommandCoro("EXPIRE %s %d", key.c_str(), JOB_TTL_SECONDS);
}

void JobService::setProgress(const std::string& jobId, int done, int total) {
    redis_->execCommandAsync(
        [](const RedisResult&) {},
        [jobId](const std::exception& e) { LOG_WARN << "JobService: progress update for " << jobId << " failed: " << e.what(); },
        "HSET %s pages_done %d pages_total %d", jobKey(jobId).c_str(), done, total);
}

Task<std::optional<JobStatus>> JobService::get(const std::string& jobId) {
    auto result = co_await redis_->execCommandCoro("HGETALL %s", jobKey(jobId).c_str());
    const auto flat = result.asArray();   // [field, value, field, value, ...]
    if (flat.empty()) co_return std::nullopt;

    Json::Value hash;
    for (size_t i = 0; i + 1 < flat.size(); i += 2) hash[flat[i].asString()] = flat[i + 1].asString();
    co_return JobStatus::fromHash(hash);
}

// ---- publishing (PUBLISH on job-updates:<id>) ---------------------------------

Task<> JobService::publish(const std::string& jobId) {
    co_await redis_->execCommandCoro("EVAL %s 1 %s %s", PUBLISH_SCRIPT, jobKey(jobId).c_str(), channel(jobId).c_str());
}

void JobService::publishAsync(const std::string& jobId) {
    redis_->execCommandAsync(
        [](const RedisResult&) {},
        [jobId](const std::exception& e) { LOG_WARN << "JobService: publish for " << jobId << " failed: " << e.what(); },
        "EVAL %s 1 %s %s", PUBLISH_SCRIPT, jobKey(jobId).c_str(), channel(jobId).c_str());
}

// ---- listening (SUBSCRIBE to job-updates:<id>) ---------------------------------

void JobService::watch(const std::string& jobId, UpdateCallback onUpdate) {
    subscriber_->subscribe(channel(jobId), [onUpdate = std::move(onUpdate)](const std::string&, const std::string& message) {
        Json::Value hash;
        Json::CharReaderBuilder reader;
        std::string errors;
        std::istringstream in(message);
        if (!Json::parseFromStream(reader, in, &hash, &errors)) {
            LOG_WARN << "JobService: bad job update: " << errors;
            return;
        }
        onUpdate(JobStatus::fromHash(hash));
    });
}

void JobService::unwatch(const std::string& jobId) {
    subscriber_->unsubscribe(channel(jobId));
}
