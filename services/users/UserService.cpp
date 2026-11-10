#include "UserService.hpp"
#include <drogon/orm/CoroMapper.h>
#include "models/Users.h"
#include <drogon/utils/coroutine.h>
#include "utils/Security.hpp"
#include "utils/Errors.hpp"
#include "events/Events.hpp"

using namespace drogon::orm;
using namespace drogon_model::bookstore;
using drogon::Task;
using std::vector;

Task<vector<Users>> UserService::getAllUsers() {

     CoroMapper<Users> users(dbClient_);

     auto all_users = co_await users.findBy(
        Criteria(Users::Cols::_is_active, CompareOperator::EQ, true)
    );

     co_return all_users;

}

Task<Users> UserService::getUserById(const std::string& id) {

    CoroMapper<Users> users(dbClient_);

    auto user = co_await users.findOne(
        Criteria(Users::Cols::_id,        CompareOperator::EQ, id) &&
        Criteria(Users::Cols::_is_active, CompareOperator::EQ, true)
    );
    
    co_return user;

}

Task<Users> UserService::createUser(const CreateUserInput& input) {

    CoroMapper<Users> users(dbClient_);

    Users newUser;

    auto taken = co_await users.count(
       (Criteria(Users::Cols::_email,    CompareOperator::EQ, input.email) ||
        Criteria(Users::Cols::_username, CompareOperator::EQ, input.username)) &&
        Criteria(Users::Cols::_is_active, CompareOperator::EQ, true)
    );
    if (taken > 0) throw Conflict("username or email already taken");

    newUser.setUsername(input.username);

    newUser.setEmail(input.email);
    
    newUser.setPasswordHash(co_await hashPassword(input.password));
    // TODO: newUser.setPasswordHash(password::hash(input.password)); once libsodium is set up.
    // Until then the insert fails, because password_hash is NOT NULL. Never store input.password as-is.
    
    auto createdUser = co_await users.insert(newUser);

    events_->Publish(USER_CREATED, UserCreated{createdUser.getValueOfId(), createdUser.getValueOfEmail(), createdUser.getValueOfUsername()});

    co_return createdUser;

}


Task<Users> UserService::updateUser(const std::string& id, const UpdateUserInput& input) {
    CoroMapper<Users> users(dbClient_);
    auto user = co_await getUserById(id);     

    if (input.username) user.setUsername(*input.username);

    if (input.email)    user.setEmail(*input.email);

       

    co_await users.update(user);

    user = co_await getUserById(id);

    co_return user;
}

Task<bool> UserService::deleteUser(const std::string& id){
    CoroMapper<Users> users(dbClient_);

    auto user = co_await getUserById(id);

    user.setIsActive(false);

    co_await users.update(user);

    co_return true;

}

Task<Users> UserService::getUserByCredentials(const LoginUserInput& input){
    CoroMapper<Users> users(dbClient_);

    auto user = co_await users.findOne(
        Criteria(Users::Cols::_email, CompareOperator::EQ, input.email) &&
        Criteria(Users::Cols::_is_active, CompareOperator::EQ, true)
    );
    
    co_return user;
}

Task<std::string> UserService::hashPassword(std::string password) {
    co_return co_await security::hashAsync(std::move(password));
}

Task<bool> UserService::verifyPassword(std::string password, std::string hash) {
    co_return co_await security::verifyAsync(std::move(password), std::move(hash));
}



