#include "EventBus.hpp"
#include <trantor/utils/Logger.h>
#include <format>

void EventBus::Publish(std::string eventName, std::any payload){
    std::vector<std::function<void(const std::any&)>> handlers ;

    {
      std::shared_lock<std::shared_mutex> rlock(mtx_) ;
       
      if(events_.find(eventName) != events_.end()){
          handlers = events_.at(eventName);
      }
      
    }

    for (size_t i = 0 ; i < handlers.size(); i++){
       try { handlers[i](payload); }
       catch (const std::exception& e) { LOG_ERROR << "handler for " << eventName << " failed: " << e.what(); }
    }
}


