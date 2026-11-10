#pragma once 
#include "books/Books.hpp"
#include "users/UserService.hpp"
#include "orders/Orders.hpp"
#include "user-settings/UserSettings.hpp"
#include "payments/Payments.hpp"
#include "jwt/JwtService.hpp"
#include "settings/Settings.h"
#include "events/EventBus.hpp"
#include "media/CloudinaryService.hpp"
#include <drogon/orm/DbClient.h>

class Services {
    public:
        UserService* userService;
        BookService* bookService;
        OrderService* orderService;
        UserSettingsService* userSettingsService;
        PaymentService* paymentService;
        JwtService* jwtService;
        CloudinaryService* cloudinaryService;

    explicit Services(drogon::orm::DbClientPtr dbClient, Settings& settings, EventBus* events) {
        userService = new UserService(dbClient, events);
        bookService = new BookService(dbClient, events);
        orderService = new OrderService(dbClient, events);
        userSettingsService = new UserSettingsService(dbClient, events);
        paymentService = new PaymentService(dbClient, events);
        cloudinaryService = new CloudinaryService(
            settings.cloudinary.cloudName,
            settings.cloudinary.apiKey,
            settings.cloudinary.apiSecret
        );
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
        delete cloudinaryService;
    }
};

Services* getServices(drogon::orm::DbClientPtr dbClient, Settings& settings, EventBus* events);