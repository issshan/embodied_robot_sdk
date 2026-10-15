/**
 * @file protocol_parser.hpp
 * @brief 自定义二进制通信协议解析与封包类
 * @note 协议格式：[Magic(2B)] [MsgType(2B)] [PayloadLen(4B)] [Payload(N B)] [CRC16(2B)]
 */

#pragma once

#include <vector>
#include <cstdint>
#include <cstddef>

namespace embodied_robot {

#pragma pack(push, 1)
struct ProtocolHeader {
    uint16_t magic;       // 魔数，固定为 0x5A5A
    uint16_t msg_type;    // 消息类型
    uint32_t payload_len; // 净荷数据长度
};
#pragma pack(pop)

class ProtocolParser {
public:
    static std::vector<uint8_t> Pack(uint16_t msg_type, const uint8_t* payload, uint32_t payload_len);
    static uint16_t CalculateCRC16(const uint8_t* data, size_t len);
};

} // namespace embodied_robot