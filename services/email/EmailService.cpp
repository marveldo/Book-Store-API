#include "EmailService.hpp"
#include <stdexcept>

EmailService::EmailService(std::string apiKey, std::string from, const std::string& templatesDir, std::string appUrl, std::string assetsUrl, trantor::EventLoop* loop, int connections)
    : apiKey_(std::move(apiKey)),
      from_(std::move(from)),
      appUrl_(std::move(appUrl)),
      assetsUrl_(std::move(assetsUrl)),
      loop_(loop),
      env_(templatesDir.ends_with('/') ? templatesDir : templatesDir + "/") {
    env_.set_html_autoescape(true);
    for (int i = 0; i < connections; i++) {
        clients_.push_back(drogon::HttpClient::newHttpClient("https://api.resend.com", loop));
    }
}

drogon::Task<> EmailService::send(std::string to, std::string subject, std::string templateName, nlohmann::json data) {
    co_await drogon::switchThreadCoro(loop_);

    data["app_url"]    = appUrl_;
    data["assets_url"] = assetsUrl_;
    const std::string html = env_.render_file(templateName, data);

    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(drogon::Post);
    req->setPath("/emails");
    req->addHeader("Authorization", "Bearer " + apiKey_);
    req->setContentTypeCode(drogon::CT_APPLICATION_JSON);
    req->setBody(nlohmann::json{{"from", from_}, {"to", {to}}, {"subject", subject}, {"html", html}}.dump());

    auto client = clients_[next_++ % clients_.size()];
    auto resp = co_await client->sendRequestCoro(req, 10);
    if (resp->statusCode() >= 300) {
        throw std::runtime_error("resend returned " + std::to_string(static_cast<int>(resp->statusCode())) + ": " + std::string(resp->body()));
    }
}
