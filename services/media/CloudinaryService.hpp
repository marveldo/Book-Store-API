#pragma once
#include <drogon/HttpClient.h>
#include <drogon/utils/coroutine.h>
#include <string>
#include <utility>
#include <vector>

using Params = std::vector<std::pair<std::string, std::string>>;

struct UploadSignature {
    std::string uploadUrl;
    Params fields;
};

class CloudinaryService {
public:
    CloudinaryService(std::string cloudName, std::string apiKey, std::string apiSecret);

    UploadSignature signUpload(const std::string& publicId) const;
    drogon::Task<bool> exists(std::string publicId);

    static std::string sign(Params params, const std::string& secret);

private:
    std::string cloudName_;
    std::string apiKey_;
    std::string apiSecret_;
    drogon::HttpClientPtr delivery_;

    static constexpr const char* ALLOWED_FORMATS = "jpg,jpeg,png,webp";
    static constexpr const char* INCOMING = "c_limit,w_1200,h_1800";
};
