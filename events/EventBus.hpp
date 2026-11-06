#pragma once
#include <string>
#include <functional>
#include <unordered_map>>

template<typename T>

class EventBus {
    public :

      EventBus() : events_({}) {};

      void Publish(std::string eventName, T payload);

      void Subscribe(std::string eventName , std::function<void(T)> handler);

    private :
      std::unordered_map<string,std::function<void(T)>> events_;

};

void RegisterEvents();
