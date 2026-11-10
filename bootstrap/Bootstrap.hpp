#pragma once
#include <drogon/orm/DbClient.h>
#include "settings/Settings.h"
#include "events/EventBus.hpp"
#include "queues/TaskQueue.hpp"
#include "services/email/EmailService.hpp"

struct AppContext {
    Settings*  settings = nullptr;
    drogon::orm::DbClientPtr db;
    EventBus*  events = nullptr;
    TaskQueue* queue  = nullptr;
    EmailService* email = nullptr;

    AppContext() = default;
    ~AppContext() {
        if (queue) queue->stop();
        delete email;
        delete queue;
        delete events;
        delete settings;
    }

    AppContext(const AppContext&)            = delete;
    AppContext& operator=(const AppContext&) = delete;
};

AppContext* bootstrap();
