#include "CloudinaryService.hpp"
#include <openssl/evp.h>
#include <algorithm>
#include <chrono>
#include <cstdio>

CloudinaryService::CloudinaryService(std::string cloudName, std::string apiKey, std::string apiSecret)
    : cloudName_(std::move(cloudName)),
      apiKey_(std::move(apiKey)),
      apiSecret_(std::move(apiSecret)),
      delivery_(drogon::HttpClient::newHttpClient("https://res.cloudinary.com")) {}

std::string CloudinaryService::sign(Params params, const std::string& secret) {
    std::sort(params.begin(), params.end());

    std::string toSign;
    for (size_t i = 0; i < params.size(); i++) {
        if (i > 0) toSign += '&';
        toSign += params[i].first + "=" + params[i].second;
    }
    toSign += secret;

    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int length = 0;
    EVP_Digest(toSign.data(), toSign.size(), digest, &length, EVP_sha1(), nullptr);

    std::string hex;
    char byte[3];
    for (unsigned int i = 0; i < length; i++) {
        std::snprintf(byte, sizeof(byte), "%02x", digest[i]);
        hex += byte;
    }
    return hex;
}

UploadSignature CloudinaryService::signUpload(const std::string& publicId) const {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    const std::string timestamp = std::to_string(std::chrono::duration_cast<std::chrono::seconds>(now).count());

    Params params = {
        {"allowed_formats", ALLOWED_FORMATS},
        {"public_id", publicId},
        {"timestamp", timestamp},
        {"transformation", INCOMING},
    };

    UploadSignature result;
    result.uploadUrl = "https://api.cloudinary.com/v1_1/" + cloudName_ + "/image/upload";
    result.fields = params;
    result.fields.push_back({"api_key", apiKey_});
    result.fields.push_back({"signature", sign(params, apiSecret_)});
    return result;
}

drogon::Task<bool> CloudinaryService::exists(std::string publicId) {
    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(drogon::Head);
    req->setPath("/" + cloudName_ + "/image/upload/" + publicId);
    auto resp = co_await delivery_->sendRequestCoro(req, 10);
    co_return resp->statusCode() == drogon::k200OK;
}
