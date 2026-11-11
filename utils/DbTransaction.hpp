#pragma once
#include <drogon/orm/DbClient.h>
#include <drogon/utils/coroutine.h>
#include <memory>

struct CommitAwaiter : public drogon::CallbackAwaiter<bool> {
    explicit CommitAwaiter(std::shared_ptr<drogon::orm::Transaction> tx) : tx_(std::move(tx)) {}

    void await_suspend(std::coroutine_handle<> handle) {
        tx_->setCommitCallback([this, handle](bool committed) {
            setValue(committed);
            handle.resume();
        });
        tx_.reset();
    }

private:
    std::shared_ptr<drogon::orm::Transaction> tx_;
};
