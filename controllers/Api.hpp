#pragma once
#include <string>

inline const std::string API_V1 = "/api/v1";

#pragma once
#include <drogon/HttpResponse.h>
#include <json/json.h>
#include <string>
#include <vector>
#include "utils/Validation.hpp"

namespace res {

inline drogon::HttpResponsePtr json(drogon::HttpStatusCode status, const Json::Value& body) {
    auto r = drogon::HttpResponse::newHttpJsonResponse(body);
    r->setStatusCode(status);
    return r;
}

inline drogon::HttpResponsePtr success(const Json::Value& data, drogon::HttpStatusCode status = drogon::k200OK) {
    Json::Value body;
    body["success"] = true;
    body["data"]    = data;
    return json(status, body);
}

inline drogon::HttpResponsePtr error(drogon::HttpStatusCode status, const std::string& message,
                                     const Json::Value& details = Json::nullValue) {
    Json::Value body;
    body["success"]          = false;
    body["error"]["message"] = message;
    if (!details.isNull()) body["error"]["details"] = details;
    return json(status, body);
}

inline drogon::HttpResponsePtr badRequest(const std::string& message) {
    return error(drogon::k400BadRequest, message);
}

inline drogon::HttpResponsePtr badRequest(const std::vector<FieldError>& errors) {
    Json::Value details(Json::arrayValue);
    for (const auto& e : errors) {
        Json::Value item;
        item["field"]   = e.field;
        item["message"] = e.message;
        details.append(item);
    }
    return error(drogon::k400BadRequest, "Validation failed", details);
}

inline drogon::HttpResponsePtr notFound(const std::string& message = "Not found") {
    return error(drogon::k404NotFound, message);
}

template <typename Model>
Json::Value toJsonArray(const std::vector<Model>& rows) {
    Json::Value list(Json::arrayValue);
    for (const auto& row : rows) list.append(row.toJson());
    return list;
}

}