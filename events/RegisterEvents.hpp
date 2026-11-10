#pragma once
#include "EventBus.hpp"
#include "queues/TaskQueue.hpp"

void RegisterEvents(EventBus* bus, TaskQueue* queue);
