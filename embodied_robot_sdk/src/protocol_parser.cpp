/**
 * @file protocol_parser.cpp
 * @brief 协议封包与 CRC16 校验实现
 */

#include "protocol_parser.hpp"
#include <cstring>

namespace embodied_robot {

constexpr uint16_t PROTOCOL_MAGIC = 0x5A5A;

std::vector<uint8_t> ProtocolParser::Pack(uint16_t msg_type, const uint8_t* payload, uint32_t payload_len) {
    ProtocolHeader header{PROTOCOL_MAGIC, msg_type, payload_len};
    size_t total_size = sizeof(ProtocolHeader) + payload_len + sizeof(uint16_t);
    std::vector<uint8_t> packet(total_size);

    std::memcpy(packet.data(), &header, sizeof(ProtocolHeader));
    if (payload && payload_len > 0) {
        std::memcpy(packet.data() + sizeof(ProtocolHeader), payload, payload_len);
    }

    uint16_t crc = CalculateCRC16(packet.data(), sizeof(ProtocolHeader) + payload_len);
    std::memcpy(packet.data() + sizeof(ProtocolHeader) + payload_len, &crc, sizeof(uint16_t));

    return packet;
}

uint16_t ProtocolParser::CalculateCRC16(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; ++i) {
        crc ^= static_cast<uint16_t>(data[i]);
        for (size_t j = 0; j < 8; ++j) {
            if (crc & 0x0001) crc = (crc >> 1) ^ 0xA001;
            else crc = crc >> 1;
        }
    }
    return crc;
}

} // namespace embodied_robot