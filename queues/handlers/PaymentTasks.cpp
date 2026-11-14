#include "TaskHandlers.hpp"
#include "TaskMiddlewares.hpp"
#include "queues/TaskErrors.hpp"
#include "utils/Errors.hpp"
#include <trantor/utils/Logger.h>

using nlohmann::json;
using drogon::Task;

void registerPaymentTasks(TaskQueue* queue, Services* services) {
    queue->registerHandler("verify_payment", [services](const json& data) -> Task<> {
        try {
            co_await services->paymentService->settle(data.at("reference").get<std::string>(), SettleBy::Request);
        } catch (const NotFound& e) {
            throw PermanentTaskError(e.what());
        }
    }, {timed("verify_payment")}, nullptr, "payments");

    queue->registerHandler("reconcile_payments", [services](const json&) -> Task<> {
        auto& paymentService = *services->paymentService;

        const auto recordFailure = [&](const std::string& reference) -> Task<> {
            try {
                co_await paymentService.recordFailedCheck(reference);
            } catch (const std::exception& e) {
                LOG_WARN << "reconcile " << reference << ": could not record the failed check: " << e.what();
            }
        };

        const auto check = [&](const std::string& reference) -> Task<> {
            bool failed = false;
            try {
                co_await paymentService.settle(reference, SettleBy::Poller);
            } catch (const std::exception& e) {
                LOG_WARN << "reconcile " << reference << ": " << e.what();
                failed = true;
            }
            if (failed) co_await recordFailure(reference);
        };

        for (int batch = 0; batch < 4; batch++) {
            const auto due = co_await paymentService.claimDue(50);
            for (const auto& reference : due) co_await check(reference);
            if (due.size() < 50) break;
        }
    }, {timed("reconcile_payments")}, nullptr, "payments");

    queue->registerHandler("refund_payment", [services](const json& data) -> Task<> {
        co_await services->paymentService->refund(data.at("reference").get<std::string>());
    }, {timed("refund_payment")},
    [](const json& data, const std::string& error) -> Task<> {
        LOG_ERROR << "Refund for " << data.value("reference", "?") << " gave up: " << error << ". Refund it by hand";
        co_return;
    }, "payments");
}
