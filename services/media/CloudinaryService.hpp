#pragma once
#include <drogon/HttpClient.h>
#include <drogon/utils/coroutine.h>
#include <atomic>
#include <string>
#include <utility>
#include <vector>

using Params = std::vector<std::pair<std::string, std::string>>;

struct UploadSignature {
    std::string uploadUrl;
    Params fields;
};

// An image the server uploads itself (pictures extracted from PDFs).
struct ImageUpload {
    std::string publicId;
    const std::string* data;   // the encoded file; must stay alive until the upload finishes
    std::string mime;          // e.g. image/png
};

class CloudinaryService {
public:
    CloudinaryService(std::string cloudName, std::string apiKey, std::string apiSecret,
                      std::string apiUrl = "https://api.cloudinary.com");

    // Browser uploads (book covers): the frontend sends the file straight to Cloudinary.
    UploadSignature signUpload(const std::string& publicId) const;
    drogon::Task<bool> exists(std::string publicId);

    // Server uploads, stored as WebP. Runs up to `parallel` uploads at once; throws if any fails
    // (after the others have finished).
    drogon::Task<> uploadAll(std::vector<ImageUpload> images, size_t parallel = 4);

    // Deletes every image whose public id starts with `prefix` (e.g. "books/<book>/<job>/").
    drogon::Task<> deleteByPrefix(std::string prefix);

    static std::string sign(Params params, const std::string& secret);

private:
    drogon::HttpRequestPtr uploadRequest(const ImageUpload& image) const;
    const drogon::HttpClientPtr& apiClient();

    std::string cloudName_;
    std::string apiKey_;
    std::string apiSecret_;
    drogon::HttpClientPtr delivery_;
    std::vector<drogon::HttpClientPtr> api_;   // one connection each, so uploads can run side by side
    std::atomic<size_t> nextClient_{0};

    static constexpr const char* ALLOWED_FORMATS = "jpg,jpeg,png,webp";
    static constexpr const char* INCOMING = "c_limit,w_1200,h_1800";
    static constexpr size_t API_CONNECTIONS = 4;
};
