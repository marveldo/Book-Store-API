#include "AccountOwnerMiddleware.hpp"
#include "controllers/Api.hpp"

using namespace drogon;

void AccountOwnerMiddleware::invoke(const HttpRequestPtr& req,
                                    MiddlewareNextCallback&& nextCb,
                                    MiddlewareCallback&& mcb) {
    const auto& params = req->getRoutingParameters();
    const std::string tokenId = req->attributes()->get<std::string>("id");

    if (tokenId.empty() || params.empty() || params[0] != tokenId) {
        mcb(res::error(k403Forbidden, "You can only access your own account"));
        return;
    }

    nextCb([mcb = std::move(mcb)](const HttpResponsePtr& resp) { mcb(resp); });
}
