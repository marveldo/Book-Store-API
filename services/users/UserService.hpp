#pragma once
#include <drogon/orm/DbClient.h>
#include <drogon/utils/coroutine.h>
#include "models/Users.h"
#include "input/UserInputs.hpp"
#include <string>


    
   

class UserService {
    public :
       explicit UserService(drogon::orm::DbClientPtr dbClient) : dbClient_(std::move(dbClient)) {};

       drogon::Task<std::vector<drogon_model::bookstore::Users>> getAllUsers();
       
       drogon::Task<drogon_model::bookstore::Users> getUserById(const std::string& id);

       drogon::Task<drogon_model::bookstore::Users> createUser(const CreateUserInput& input);

       drogon::Task<drogon_model::bookstore::Users> updateUser(const std::string& id, const UpdateUserInput& input);

       drogon::Task<drogon_model::bookstore::Users > getUserByCredentials(const LoginUserInput& input);

       drogon::Task<bool> deleteUser(const std::string& id);

       std::string hashPassword(const std::string& password);

       bool verifyPassword(const std::string& password, const std::string& hash);


    private:
       drogon::orm::DbClientPtr dbClient_;
};