#pragma once
#include <string>
#include <functional>
#include <unordered_map>
#include <vector>
#include <any>
#include <shared_mutex>
#include <mutex>
#include <format>
#include <trantor/utils/Logger.h>

class EventBus
{
public:
  EventBus() : events_({}) {};

  void Publish(std::string eventName, std::any payload);

  template <typename T>
  void Subscribe(std::string eventName, std::function<void(T)> handler)
  {
    auto anyHandler = [handler = std::move(handler), eventName](const std::any &payload)
    {
      if (auto *valuePtr = std::any_cast<T>(&payload))
      {
        handler(*valuePtr); // safely passing the payload into the handler using a cast pointer
      }
      else
      {
        LOG_WARN << std::format("Type Mismatch for a handler payload of eventName : {}", eventName);
      }
    };
    std::unique_lock<std::shared_mutex> wlock(mtx_);
    events_[eventName].push_back(anyHandler);
  }

private:
  std::unordered_map<std::string, std::vector<std::function<void(const std::any&)>>> events_;
  std::shared_mutex mtx_;
};
