#include "EventBus.hpp"
#include <trantor/utils/Logger.h>
#include <format>

template<typename T>
void EventBus::Subscribe(std::string eventName , std::function<void(T)> handler){
     std::unique_lock<std::shared_mutex> wlock(mtx_);
     auto anyHandler = [handler, eventName](std::any payload) {
        if (auto* valuePtr = std::any_cast<T>(&payload)) {
            handler(*valuePtr); //safely passing the payload into the handler using a cast pointer
        } else {
           LOG_WARN << format("Type Mismatch for a handler payload of eventName : {}", eventName);
        }
    };

     events_[eventName].push_back(anyHandler); 
     
}

void EventBus::Publish(std::string eventName, std::any payload){
    std::vector<std::function<void(std::any)>> handlers ;

    {
      std::shared_lock<std::shared_mutex> rlock(mtx_) ;
       
      if(events_.find(eventName) != events_.end()){
          handlers = events_[eventName];

      }
      
    }

    for (size_t i = 0 ; i < handlers.size(); i++){
        handlers[i](payload);
    }
}


