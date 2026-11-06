#pragma once
#include <string>
#include <functional>
#include <unordered_map>
#include <vector>
#include <any>
#include <shared_mutex>
#include <mutex>

class EventBus {
    public :

      EventBus() : events_({}) {};
    
      void Publish(std::string eventName, std::any payload);
      
      template<typename T>
      void Subscribe(std::string eventName , std::function<void(T)> handler);

    private :
      std::unordered_map<std::string,std::vector<std::function<void(std::any)>>> events_;
      std::shared_mutex mtx_;

};

void RegisterEvents(EventBus* eventOBj);

