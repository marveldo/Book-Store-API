#pragma once
#include <json/json.h>
#include "models/Books.h"
#include "models/Orders.h"
#include "models/Payments.h"
#include "utils/Money.hpp"

inline Json::Value bookJson(const drogon_model::bookstore::Books& book) {
    Json::Value json = book.toJson();
    json["price"] = money::toMajor(book.getValueOfPrice());
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
    return json;
}
