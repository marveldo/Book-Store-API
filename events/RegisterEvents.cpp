#include "RegisterEvents.hpp"
#include "Events.hpp"

void RegisterEvents(EventBus* bus, TaskQueue* queue) {
    bus->Subscribe<UserCreated>(USER_CREATED, [queue](UserCreated event) {
        queue->enqueue("send_email", {
            {"to", event.email},
            {"subject", "Welcome to Mbooks"},
            {"template", "welcome.html"},
            {"data", {{"username", event.username}}}
        });
    });

    bus->Subscribe<UserCreated>(USER_CREATED, [queue](UserCreated event) {
        queue->enqueue("create_user_settings", {{"user_id", event.id}});
    });
}
