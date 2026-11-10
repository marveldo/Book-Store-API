#include "Services.hpp"
#include <drogon/orm/DbClient.h>

namespace {
    Services* g_services = nullptr;  
}

Services* getServices(drogon::orm::DbClientPtr dbClient, Settings& settings, EventBus* events){
    if (g_services) return g_services;
    g_services = new Services(dbClient, settings, events);
    return g_services;
}
