#pragma once
#include "queues/TaskQueue.hpp"
#include <string>

TaskQueue::Middleware timed(const std::string& name);
