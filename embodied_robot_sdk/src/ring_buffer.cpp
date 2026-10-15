/**
 * @file ring_buffer.cpp
 * @brief 线程安全的环形缓冲区实现
 */

#include "ring_buffer.hpp"
#include <algorithm>
#include <cstring>

namespace embodied_robot {

RingBuffer::RingBuffer(size_t capacity)
    : capacity_(capacity), head_(0), tail_(0), size_(0) {
    buffer_.resize(capacity_);
}

size_t RingBuffer::Write(const uint8_t* data, size_t len) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (len == 0 || size_ + len > capacity_) return 0;

    size_t first_chunk = std::min(len, capacity_ - tail_);
    std::memcpy(&buffer_[tail_], data, first_chunk);

    size_t second_chunk = len - first_chunk;
    if (second_chunk > 0) {
        std::memcpy(&buffer_[0], data + first_chunk, second_chunk);
    }

    tail_ = (tail_ + len) % capacity_;
    size_ += len;
    return len;
}

size_t RingBuffer::Peek(uint8_t* data, size_t len) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (len == 0 || size_ == 0) return 0;

    size_t read_len = std::min(len, size_);
    size_t first_chunk = std::min(read_len, capacity_ - head_);
    std::memcpy(data, &buffer_[head_], first_chunk);

    size_t second_chunk = read_len - first_chunk;
    if (second_chunk > 0) {
        std::memcpy(data + first_chunk, &buffer_[0], second_chunk);
    }

    return read_len;
}

void RingBuffer::Consume(size_t len) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (len >= size_) {
        head_ = 0; tail_ = 0; size_ = 0;
    } else {
        head_ = (head_ + len) % capacity_;
        size_ -= len;
    }
}

size_t RingBuffer::Size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return size_;
}

size_t RingBuffer::FreeSpace() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return capacity_ - size_;
}

void RingBuffer::Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    head_ = 0; tail_ = 0; size_ = 0;
}

} // namespace embodied_robot