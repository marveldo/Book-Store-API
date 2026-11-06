#pragma once
#include <string>
#include <functional>
#include <unordered_map>
#include <vector>
#include <any>


class EventBus {
    public :

      EventBus() : events_({}) {};
      template<typename T>
      void Publish(std::string eventName, T payload);
      template<typename T>
      void Subscribe(std::string eventName , std::function<void(T)> handler);

    private :
      std::unordered_map<std::string,std::vector<std::function<void(std::any)>>> events_;

};

void RegisterEvents(EventBus* eventOBj);

