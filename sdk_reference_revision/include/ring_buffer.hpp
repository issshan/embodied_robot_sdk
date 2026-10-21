#pragma once
#include "platform.hpp"
#include <cstddef>
#include <cstdint>
#include <vector>
namespace embodied_robot {
// Bounded, copying, mutex-protected queue. Methods are individually atomic;
// compound Peek/Consume sequences require external serialization.
class RingBuffer {
public:
    explicit RingBuffer(size_t capacity);
    size_t Write(const uint8_t* data, size_t len);
    size_t Peek(uint8_t* data, size_t len) const;
    void Consume(size_t len);
    size_t Size() const;
    size_t FreeSpace() const;
    void Clear();
private:
    std::vector<uint8_t> buffer_;
    size_t capacity_, head_ = 0, tail_ = 0, size_ = 0;
    mutable platform::Mutex mutex_;
};
}
