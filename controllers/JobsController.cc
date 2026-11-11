#include "JobsController.h"

namespace {
    void reject(const WebSocketConnectionPtr& conn, const std::string& message) {
        Json::Value body;
        body["status"] = "error";
        body["error"]  = message;
        Json::StreamWriterBuilder writer;
        writer["indentation"] = "";
        conn->send(Json::writeString(writer, body));
        conn->shutdown(CloseCode::kViolation, message);
    }
}

Task<> JobsController::handleNewConnectionCoro(const HttpRequestPtr& req, const WebSocketConnectionPtr& conn) {
    const std::string jobId = req->getParameter("job_id");
    const std::string token = req->getParameter("token");

    std::string userId;
    try {
        auto claims = services_->jwtService->verifyAccessToken(token);
        if (claims) userId = claims->userId;
    } catch (const std::exception&) {
        // handled below: userId stays empty
    }
    if (userId.empty()) {
        reject(conn, "Invalid or missing token");
        co_return;
    }

    auto job = co_await services_->jobService->get(jobId);
    // Same answer for "doesn't exist" and "not yours", so job ids can't be probed.
    if (!job) {
        reject(conn, "Job not found");
        co_return;
    }
    if (job->userId != userId) {
        const bool admin = co_await services_->userService->isAdmin(userId);
        if (!admin) {
            reject(conn, "Job not found");
            co_return;
        }
    }

    // Start listening first, then read the current state. The other way round, an update
    // that lands between the read and the subscribe would be missed.
    conn->setContext(std::make_shared<std::string>(jobId));   // so the close handler knows the job
    addWatcher(jobId, conn);

    auto current = co_await services_->jobService->get(jobId);
    if (!current) {
        removeWatcher(jobId, conn);
        reject(conn, "Job not found");
        co_return;
    }
    conn->send(current->toMessage());
    if (current->finished()) {
        removeWatcher(jobId, conn);
        conn->shutdown(CloseCode::kNormalClosure, "Job finished");
    }
}

Task<> JobsController::handleNewMessageCoro(const WebSocketConnectionPtr&, std::string&&, const WebSocketMessageType&) {
    // The client only listens. Pings are answered by Drogon itself.
    co_return;
}

Task<> JobsController::handleConnectionClosedCoro(const WebSocketConnectionPtr& conn) {
    if (auto jobId = conn->getContext<std::string>()) removeWatcher(*jobId, conn);
    co_return;
}

void JobsController::addWatcher(const std::string& jobId, const WebSocketConnectionPtr& conn) {
    bool first = false;
    {
        std::lock_guard lock(mutex_);
        auto& sockets = watchers_[jobId];
        first = sockets.empty();
        sockets.push_back(conn);
    }
    // One Redis subscription per job on this instance, however many sockets watch it.
    if (first) services_->jobService->watch(jobId, [this](const JobStatus& job) { broadcast(job); });
}

void JobsController::removeWatcher(const std::string& jobId, const WebSocketConnectionPtr& conn) {
    bool last = false;
    {
        std::lock_guard lock(mutex_);
        auto it = watchers_.find(jobId);
        if (it == watchers_.end()) return;
        std::erase(it->second, conn);
        if (it->second.empty()) {
            watchers_.erase(it);
            last = true;
        }
    }
    if (last) services_->jobService->unwatch(jobId);
}

void JobsController::broadcast(const JobStatus& job) {
    std::vector<WebSocketConnectionPtr> sockets;
    {
        std::lock_guard lock(mutex_);
        auto it = watchers_.find(job.jobId);
        if (it == watchers_.end()) return;
        sockets = it->second;
    }

    // conn->send only queues the message on the socket's own IO thread.
    const std::string message = job.toMessage();
    for (const auto& conn : sockets) {
        if (conn->connected()) conn->send(message);
    }

    // Closing a socket runs handleConnectionClosedCoro, which removes it from watchers_
    // and drops the Redis subscription once nobody is left.
    if (job.finished()) {
        for (const auto& conn : sockets) conn->shutdown(CloseCode::kNormalClosure, "Job finished");
    }
}
