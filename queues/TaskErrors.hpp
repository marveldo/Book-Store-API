#pragma once
#include <stdexcept>

// Throw this from a task handler when retrying can't help (a corrupt file, a deleted book).
// The queue skips the remaining attempts and moves the task straight to tasks:dead.
struct PermanentTaskError : std::runtime_error {
    using std::runtime_error::runtime_error;
};
