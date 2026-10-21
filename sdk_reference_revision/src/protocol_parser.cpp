#include "protocol_parser.hpp"
#include <stdexcept>
namespace embodied_robot {
namespace {
uint16_t Read16(const uint8_t* p) { return uint16_t(p[0]) | (uint16_t(p[1]) << 8); }
uint32_t Read32(const uint8_t* p) {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}
}
std::vector<uint8_t> ProtocolParser::Pack(uint16_t type, const uint8_t* data, uint32_t size) {
    if (size > kMaxPayload || (size && !data)) throw std::invalid_argument("invalid payload");
    std::vector<uint8_t> out(kHeaderSize + size + 2);
    out[0] = out[1] = 0x5A;
    out[2] = static_cast<uint8_t>(type); out[3] = static_cast<uint8_t>(type >> 8);
    for (unsigned i = 0; i < 4; ++i) out[4+i] = static_cast<uint8_t>(size >> (8*i));
    if (size) std::copy(data, data + size, out.begin() + kHeaderSize);
    const auto crc = CalculateCRC16(out.data(), out.size() - 2);
    out[out.size()-2] = static_cast<uint8_t>(crc); out.back() = static_cast<uint8_t>(crc >> 8);
    return out;
}
uint16_t ProtocolParser::CalculateCRC16(const uint8_t* data, size_t size) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (int j = 0; j < 8; ++j) crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
    }
    return crc;
}
bool ProtocolParser::Feed(const uint8_t* data, size_t size) {
    return size == 0 || buffer_.Write(data, size) == size;
}
bool ProtocolParser::Next(Frame& frame) {
    while (buffer_.Size() >= kHeaderSize) {
        uint8_t header[kHeaderSize]; buffer_.Peek(header, sizeof(header));
        if (header[0] != 0x5A || header[1] != 0x5A) {
            buffer_.Consume(1); ++stats_.discarded_bytes; continue;
        }
        const auto length = Read32(header + 4);
        if (length > kMaxPayload) {
            ++stats_.invalid_lengths; buffer_.Consume(1); ++stats_.discarded_bytes; continue;
        }
        const size_t total = kHeaderSize + length + 2;
        if (buffer_.Size() < total) return false;
        std::vector<uint8_t> bytes(total); buffer_.Peek(bytes.data(), total);
        if (Read16(bytes.data() + total - 2) != CalculateCRC16(bytes.data(), total - 2)) {
            ++stats_.crc_errors; buffer_.Consume(1); ++stats_.discarded_bytes; continue;
        }
        frame.type = Read16(header + 2);
        frame.payload.assign(bytes.begin() + kHeaderSize, bytes.end() - 2);
        buffer_.Consume(total); ++stats_.frames; return true;
    }
    return false;
}
void ProtocolParser::Reset() { buffer_.Clear(); stats_ = {}; }
}
