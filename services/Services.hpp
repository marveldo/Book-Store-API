#pragma once 
#include "books/Books.hpp"
#include "book-pages/BookPages.hpp"
#include "book-toc/BookToc.hpp"
#include "book-comments/BookComments.hpp"
#include "users/UserService.hpp"
#include "orders/Orders.hpp"
#include "user-settings/UserSettings.hpp"
#include "payments/Payments.hpp"
#include "jwt/JwtService.hpp"
#include "settings/Settings.h"
#include "events/EventBus.hpp"
#include "media/CloudinaryService.hpp"
#include "jobs/JobService.hpp"
#include "pdf/PdfExtractor.hpp"
#include <drogon/orm/DbClient.h>

class Services {
    public:
        UserService* userService;
        BookService* bookService;
        BookPageService* bookPageService;
        BookTocService* bookTocService;
        BookCommentService* bookCommentService;
        OrderService* orderService;
        UserSettingsService* userSettingsService;
        PaymentService* paymentService;
        JwtService* jwtService;
        CloudinaryService* cloudinaryService;
        JobService* jobService;
        PdfExtractor* pdfExtractor;

    explicit Services(drogon::orm::DbClientPtr dbClient, Settings& settings, EventBus* events) {
        userService = new UserService(dbClient, events);
        bookService = new BookService(dbClient, events);
        bookPageService = new BookPageService(dbClient, events);
        bookTocService = new BookTocService(dbClient, events);
        bookCommentService = new BookCommentService(dbClient, events);
        orderService = new OrderService(dbClient, events);
        userSettingsService = new UserSettingsService(dbClient, events);
        paymentService = new PaymentService(dbClient, events);
        cloudinaryService = new CloudinaryService(
            settings.cloudinary.cloudName,
            settings.cloudinary.apiKey,
            settings.cloudinary.apiSecret,
            settings.cloudinary.apiUrl
        );
        jobService = new JobService(settings.redis.host, settings.redis.port, settings.redis.password,
                                    settings.redis.username, settings.redis.db);
        pdfExtractor = new PdfExtractor(PdfLimits{settings.uploads.pdfMaxPages, settings.uploads.pdfTimeoutSeconds,
                                                  settings.uploads.pdfMaxImages});
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
        delete bookPageService;
        delete bookTocService;
        delete bookCommentService;
        delete orderService;
        delete userSettingsService;
        delete paymentService;
        delete jwtService;
        delete cloudinaryService;
        delete jobService;
        delete pdfExtractor;
    }
};

Services* getServices(drogon::orm::DbClientPtr dbClient, Settings& settings, EventBus* events);