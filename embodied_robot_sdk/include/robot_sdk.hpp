/**
 * @file robot_sdk.hpp
 * @brief 具身机器人二次开发 SDK 对外核心接口声明
 */

#pragma once

#include "tcp_client.hpp"
#include <string>
#include <memory>
#include <cstdint>

namespace embodied_robot {

struct ChassisVelocity {
    float linear_x;  // 线速度 X (m/s)
    float linear_y;  // 线速度 Y (m/s)
    float angular_z; // 角速度 Z (rad/s)
};

class RobotSDK {
public:
    RobotSDK(std::string server_ip, int server_port);
    ~RobotSDK();

    bool Init();
    void Shutdown();
    bool SetChassisVelocity(const ChassisVelocity& vel);
    void RegisterSensorCallback(TcpClient::DataCallback callback);

private:
    std::unique_ptr<TcpClient> client_;
    std::string ip_;
    int port_;
};

} // namespace embodied_robot