/**
 * @file robot_sdk.cpp
 * @brief 具身机器人 SDK 核心实现
 */

#include "robot_sdk.hpp"
#include "protocol_parser.hpp"
#include <iostream>
#include <cstring>

namespace embodied_robot {

RobotSDK::RobotSDK(std::string server_ip, int server_port)
    : ip_(std::move(server_ip)), port_(server_port) {
    client_ = std::make_unique<TcpClient>(ip_, port_);
}

RobotSDK::~RobotSDK() { Shutdown(); }

bool RobotSDK::Init() { return client_->Start(); }

void RobotSDK::Shutdown() { if (client_) client_->Stop(); }

bool RobotSDK::SetChassisVelocity(const ChassisVelocity& vel) {
    uint8_t payload[sizeof(ChassisVelocity)];
    std::memcpy(payload, &vel, sizeof(ChassisVelocity));
    auto packet = ProtocolParser::Pack(0x1001, payload, sizeof(ChassisVelocity));
    return client_->SendPacket(packet.data(), packet.size());
}

void RobotSDK::RegisterSensorCallback(TcpClient::DataCallback callback) {
    client_->SetDataCallback(std::move(callback));
}

} // namespace embodied_robot