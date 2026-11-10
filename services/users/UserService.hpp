#pragma once
#include <drogon/orm/DbClient.h>
#include <drogon/utils/coroutine.h>
#include "models/Users.h"
#include "input/UserInputs.hpp"
#include "events/EventBus.hpp"
#include <string>


    
   

class UserService {
    public :
       UserService(drogon::orm::DbClientPtr dbClient, EventBus* events) : dbClient_(std::move(dbClient)), events_(events) {};

       drogon::Task<std::vector<drogon_model::bookstore::Users>> getAllUsers();
       
       drogon::Task<drogon_model::bookstore::Users> getUserById(const std::string& id);

       drogon::Task<drogon_model::bookstore::Users> createUser(const CreateUserInput& input);

       drogon::Task<drogon_model::bookstore::Users> updateUser(const std::string& id, const UpdateUserInput& input);

       drogon::Task<drogon_model::bookstore::Users > getUserByCredentials(const LoginUserInput& input);

       drogon::Task<bool> deleteUser(const std::string& id);

       drogon::Task<std::string> hashPassword(std::string password);

       drogon::Task<bool> verifyPassword(std::string password, std::string hash);


    private:
       drogon::orm::DbClientPtr dbClient_;
       EventBus* events_;
};