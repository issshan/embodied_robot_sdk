/**
 * @file ring_buffer.hpp
 * @brief 线程安全的环形缓冲区（Ring Buffer）类声明
 * @note 解决 TCP 流式传输中的粘包、半包问题，支持高效的数据读写
 */

#pragma once

#include <vector>
#include <mutex>
#include <cstdint>
#include <cstddef>

namespace embodied_robot {

class RingBuffer {
public:
    explicit RingBuffer(size_t capacity);
    ~RingBuffer() = default;

    RingBuffer(const RingBuffer&) = delete;
    RingBuffer& operator=(const RingBuffer&) = delete;

    size_t Write(const uint8_t* data, size_t len);
    size_t Peek(uint8_t* data, size_t len) const;
    void Consume(size_t len);
    size_t Size() const;
    size_t FreeSpace() const;
    void Clear();

private:
    std::vector<uint8_t> buffer_;
    size_t capacity_;
    size_t head_;
    size_t tail_;
    size_t size_;
    mutable std::mutex mutex_;
};

} // namespace embodied_robot