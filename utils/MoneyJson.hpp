#pragma once
#include <json/json.h>
#include "models/Books.h"
#include "models/Orders.h"
#include "models/Payments.h"
#include "utils/Money.hpp"
#include "utils/Media.hpp"

inline Json::Value bookJson(const drogon_model::bookstore::Books& book) {
    Json::Value json = book.toJson();
    json["price"] = money::toMajor(book.getValueOfPrice());
    if (json.isMember("cover_key") && json["cover_key"].isString()) {
        json["cover_url"] = imageUrl(json["cover_key"].asString());
    }
    return json;
}

inline Json::Value orderJson(const drogon_model::bookstore::Orders& order) {
    Json::Value json = order.toJson();
    json["total_price"] = money::toMajor(order.getValueOfTotalPrice());
    return json;
}

inline Json::Value paymentJson(const drogon_model::bookstore::Payments& payment) {
    Json::Value json = payment.toJson();
    json["amount"] = money::toMajor(payment.getValueOfAmount());
    json.removeMember("next_check_at");
    json.removeMember("check_count");
    if (payment.getValueOfStatus() != "pending") json["checkout_url"] = Json::nullValue;
    return json;
}
