#pragma once
#include "queues/TaskQueue.hpp"
#include "services/email/EmailService.hpp"
#include "services/Services.hpp"

void registerTaskHandlers(TaskQueue* queue, EmailService* email, Services* services);
