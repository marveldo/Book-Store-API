#include "CloudinaryService.hpp"
#include <drogon/utils/Utilities.h>
#include <json/json.h>
#include <openssl/evp.h>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>

namespace {
    std::string nowSeconds() {
        const auto now = std::chrono::system_clock::now().time_since_epoch();
        return std::to_string(std::chrono::duration_cast<std::chrono::seconds>(now).count());
    }

    // Cloudinary errors look like {"error":{"message":"..."}}.
    std::string errorOf(const drogon::HttpResponsePtr& resp) {
        if (auto json = resp->getJsonObject(); json && (*json)["error"].isObject()) {
            return (*json)["error"]["message"].asString();
        }
        return "HTTP " + std::to_string(static_cast<int>(resp->statusCode()));
    }

    // Keeps track of a batch of uploads running side by side.
    struct UploadBatch : std::enable_shared_from_this<UploadBatch> {
        std::vector<ImageUpload> images;
        std::function<drogon::HttpRequestPtr(const ImageUpload&)> buildRequest;
        std::vector<drogon::HttpClientPtr> clients;
        std::function<void()> finish;

        std::mutex mutex;
        size_t next = 0;        // next image to start
        size_t running = 0;
        bool finished = false;  // finish() must run exactly once
        std::string error;      // first failure

        // Starts the next image on `client`, or finishes the batch when nothing is left.
        void startNext(size_t client) {
            size_t index = 0;
            bool started = false;
            bool done = false;
            {
                std::lock_guard lock(mutex);
                if (next >= images.size() || !error.empty()) {
                    // Two uploads can finish at the same moment; only the last one out finishes the batch.
                    done = running == 0 && !finished;
                    if (done) finished = true;
                } else {
                    index = next++;
                    running++;
                    started = true;
                }
            }
            if (done) { finish(); return; }
            if (!started) return;   // nothing left for this client; another one finishes the batch

            // Built here rather than up front, so only a few base64 copies exist at once.
            auto self = shared_from_this();
            clients[client]->sendRequest(buildRequest(images[index]),
                [self, client, index](drogon::ReqResult result, const drogon::HttpResponsePtr& resp) {
                    {
                        std::lock_guard lock(self->mutex);
                        self->running--;
                        if (self->error.empty()) {
                            if (result != drogon::ReqResult::Ok) {
                                self->error = "Image upload failed for " + self->images[index].publicId + ": " + drogon::to_string(result);
                            } else if (resp->statusCode() != drogon::k200OK) {
                                self->error = "Image upload failed for " + self->images[index].publicId + ": " + errorOf(resp);
                            }
                        }
                    }
                    self->startNext(client);
                }, 120);
        }
    };

    struct BatchAwaiter : drogon::CallbackAwaiter<void> {
        explicit BatchAwaiter(std::shared_ptr<UploadBatch> batch, size_t parallel)
            : batch_(std::move(batch)), parallel_(parallel) {}

        void await_suspend(std::coroutine_handle<> handle) {
            batch_->finish = [this, handle]() {
                if (!batch_->error.empty()) setException(std::make_exception_ptr(std::runtime_error(batch_->error)));
                handle.resume();
            };
            // Locals only from here on: an upload can finish and resume the coroutine on another
            // thread while this loop is still running, and resuming destroys this awaiter.
            auto batch = batch_;
            const size_t start = std::min(parallel_, batch->images.size());
            for (size_t i = 0; i < start; i++) batch->startNext(i % batch->clients.size());
        }

    private:
        std::shared_ptr<UploadBatch> batch_;
        size_t parallel_;
    };
}

CloudinaryService::CloudinaryService(std::string cloudName, std::string apiKey, std::string apiSecret, std::string apiUrl)
    : cloudName_(std::move(cloudName)),
      apiKey_(std::move(apiKey)),
      apiSecret_(std::move(apiSecret)),
      delivery_(drogon::HttpClient::newHttpClient("https://res.cloudinary.com")) {
    for (size_t i = 0; i < API_CONNECTIONS; i++) api_.push_back(drogon::HttpClient::newHttpClient(apiUrl));
}

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

UploadSignature CloudinaryService::signUpload(const std::string& publicId, const std::string& transformation) const {
    Params params = {
        {"allowed_formats", ALLOWED_FORMATS},
        {"public_id", publicId},
        {"timestamp", nowSeconds()},
        {"transformation", transformation},
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

const drogon::HttpClientPtr& CloudinaryService::apiClient() {
    return api_[nextClient_++ % api_.size()];
}

// A signed upload with the file sent as a base64 data URI, which Cloudinary accepts in place
// of a multipart file. `format=webp` makes Cloudinary store it as WebP whatever we send.
drogon::HttpRequestPtr CloudinaryService::uploadRequest(const ImageUpload& image) const {
    const Params params = {
        {"format", "webp"},
        {"public_id", image.publicId},
        {"timestamp", nowSeconds()},
    };

    auto req = drogon::HttpRequest::newHttpFormPostRequest();
    req->setPath("/v1_1/" + cloudName_ + "/image/upload");
    for (const auto& [key, value] : params) req->setParameter(key, value);
    req->setParameter("api_key", apiKey_);
    req->setParameter("signature", sign(params, apiSecret_));
    req->setParameter("file", "data:" + image.mime + ";base64," +
        drogon::utils::base64Encode(reinterpret_cast<const unsigned char*>(image.data->data()), image.data->size()));
    return req;
}

drogon::Task<> CloudinaryService::uploadAll(std::vector<ImageUpload> images, size_t parallel) {
    if (images.empty()) co_return;

    auto batch = std::make_shared<UploadBatch>();
    batch->images = std::move(images);
    batch->clients = api_;
    batch->buildRequest = [this](const ImageUpload& image) { return uploadRequest(image); };
    co_await BatchAwaiter(batch, std::max<size_t>(1, std::min(parallel, api_.size())));
}

// Admin API. Deletes up to 1000 images per call and says `partial` when there are more.
drogon::Task<> CloudinaryService::deleteByPrefix(std::string prefix) {
    const std::string credentials = drogon::utils::base64Encode(apiKey_ + ":" + apiSecret_);
    for (int round = 0; round < 20; round++) {
        auto req = drogon::HttpRequest::newHttpRequest();
        req->setMethod(drogon::Delete);
        req->setPath("/v1_1/" + cloudName_ + "/resources/image/upload");
        req->setParameter("prefix", prefix);
        req->addHeader("Authorization", "Basic " + credentials);

        auto resp = co_await apiClient()->sendRequestCoro(req, 60);
        if (resp->statusCode() != drogon::k200OK) {
            throw std::runtime_error("Deleting images under " + prefix + " failed: " + errorOf(resp));
        }
        auto json = resp->getJsonObject();
        if (!json || !(*json)["partial"].asBool()) co_return;
    }
}
