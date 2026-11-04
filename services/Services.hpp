#pragma once 
#include "books/Books.hpp"
#include "users/UserService.hpp"
#include "orders/Orders.hpp"
#include "user-settings/UserSettings.hpp"
#include "payments/Payments.hpp"
#include "jwt/JwtService.hpp"
#include "settings/Settings.h"
#include <drogon/orm/DbClient.h>

class Services {
    public:
        UserService* userService;
        BookService* bookService;
        OrderService* orderService;
        UserSettingsService* userSettingsService;
        PaymentService* paymentService;
        JwtService* jwtService;

    explicit Services(drogon::orm::DbClientPtr dbClient, Settings& settings) {
        userService = new UserService(dbClient);
        bookService = new BookService(dbClient);
        orderService = new OrderService(dbClient);
        userSettingsService = new UserSettingsService(dbClient);
        paymentService = new PaymentService(dbClient);
        jwtService = new JwtService(
            settings.jwt.accessSecret, 
            settings.jwt.refreshSecret, 
            settings.jwt.accessExpiration, 
            settings.jwt.refreshExpiration
        );
    }

    ~Services() {
        delete userService;
        delete bookService;
        delete orderService;
        delete userSettingsService;
        delete paymentService;
        delete jwtService;
    }
};

Services* getServices(drogon::orm::DbClientPtr dbClient, Settings& settings) ;