#pragma once
#include "ring_buffer.hpp"
#include <cstddef>
#include <cstdint>
#include <vector>
namespace embodied_robot {
constexpr size_t kHeaderSize = 8;
constexpr size_t kMaxPayload = 4096;
constexpr uint16_t kHeartbeatRequest = 0x0001;
constexpr uint16_t kHeartbeatReply = 0x0002;
constexpr uint16_t kVelocityCommand = 0x1001;
constexpr uint16_t kRobotState = 0x2001;
// Explicit little-endian wire layout; no packed-struct serialization.
struct Frame { uint16_t type = 0; std::vector<uint8_t> payload; };
struct ParserStats { uint64_t frames = 0, crc_errors = 0, invalid_lengths = 0, discarded_bytes = 0; };
class ProtocolParser {
public:
    ProtocolParser() : buffer_(2 * (kHeaderSize + kMaxPayload + 2)) {}
    static std::vector<uint8_t> Pack(uint16_t type, const uint8_t* data, uint32_t size);
    static uint16_t CalculateCRC16(const uint8_t* data, size_t size);
    bool Feed(const uint8_t* data, size_t size);
    bool Next(Frame& frame);
    void Reset();
    const ParserStats& Stats() const { return stats_; }
private:
    RingBuffer buffer_;
    ParserStats stats_;
};
}
