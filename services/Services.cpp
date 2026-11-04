#include "Services.hpp"
#include <drogon/orm/DbClient.h>

namespace {
    Services* g_services = nullptr;  
}

Services* getServices(drogon::orm::DbClientPtr dbClient, Settings& settings){
    if (g_services) return g_services;
    g_services = new Services(dbClient, settings);
    return g_services;
}
