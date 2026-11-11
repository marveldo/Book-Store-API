#pragma once
#include <drogon/drogon.h>

inline void registerApiDocs() {
    using namespace drogon;

    app().registerHandler("/openapi.json", [](HttpRequestPtr) -> Task<HttpResponsePtr> {
        co_return HttpResponse::newFileResponse("openapi.json", "", CT_APPLICATION_JSON);
    }, {Get});

    app().registerHandler("/docs", [](HttpRequestPtr) -> Task<HttpResponsePtr> {
        co_return HttpResponse::newFileResponse("templates/docs.html", "", CT_TEXT_HTML);
    }, {Get});
}
