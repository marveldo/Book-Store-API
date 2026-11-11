#pragma once

#include <drogon/WebSocketController.h>

#include "services/Services.hpp"

#include "Api.hpp"

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

using namespace drogon;

// Live progress for a background job.
//
//   ws://<host>/api/v1/jobs/ws?job_id=<id>&token=<access token>
//
// Browsers can't set an Authorization header on a WebSocket, so the access token
// goes in the query string. The server sends the job's status as JSON right away
// and on every change, then closes the socket when the job completes or fails.
//
// The job may be running on a different server instance. Updates arrive through
// Redis (JobService::watch); this controller only keeps track of which sockets on
// *this* instance are watching which job.
class JobsController : public drogon::WebSocketCoroController<JobsController, false>
{
  public:
    explicit JobsController(Services* srv) : services_(srv) {}

    WS_PATH_LIST_BEGIN
    WS_PATH_ADD(API_V1 + "/jobs/ws");
    WS_PATH_LIST_END

    Task<> handleNewConnectionCoro(const HttpRequestPtr& req, const WebSocketConnectionPtr& conn) override;
    Task<> handleNewMessageCoro(const WebSocketConnectionPtr& conn, std::string&& message, const WebSocketMessageType& type) override;
    Task<> handleConnectionClosedCoro(const WebSocketConnectionPtr& conn) override;

  private:
    void addWatcher(const std::string& jobId, const WebSocketConnectionPtr& conn);
    void removeWatcher(const std::string& jobId, const WebSocketConnectionPtr& conn);
    void broadcast(const JobStatus& job);   // called by Redis when the job changes

    Services* services_;

    // job id -> sockets on this instance watching it. Touched by Drogon's IO threads
    // and the Redis thread, hence the mutex.
    std::mutex mutex_;
    std::unordered_map<std::string, std::vector<WebSocketConnectionPtr>> watchers_;
};
