#pragma once
#include <cstddef>
#include <cstdint>
namespace embodied_robot {
// Injectable transport loop allows deterministic short-write/error tests.
// send_fn returns positive progress, 0 for retryable, negative for fatal.
// ready_fn must bound its wait and return false on cancellation/deadline.
template<class Send, class Ready>
bool SendAll(const uint8_t* data, size_t length, Send send_fn, Ready ready_fn) {
    size_t offset = 0;
    while (offset < length) {
        if (!ready_fn()) return false;
        const int count = send_fn(data+offset, length-offset);
        if (count < 0 || static_cast<size_t>(count) > length-offset) return false;
        offset += static_cast<size_t>(count);
    }
    return true;
}
}
