#pragma once
#include <drogon/orm/DbClient.h>
#include "settings/Settings.h"

struct AppContext {
    Settings*  settings = nullptr;
    drogon::orm::DbClientPtr db;

    AppContext() = default;
    ~AppContext() { delete settings; }

    AppContext(const AppContext&)            = delete;
    AppContext& operator=(const AppContext&) = delete;
};

AppContext* bootstrap();
