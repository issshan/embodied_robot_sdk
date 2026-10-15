/**
 * @file robot_demo.cpp
 * @brief 具身机器人 SDK 使用示例 Demo
 */

#include "robot_sdk.hpp"
#include <iostream>
#include <chrono>
#include <thread>

int main() {
    std::cout << "=== 具身机器人二次开发 SDK 演示程序启动 ===\n";

    embodied_robot::RobotSDK sdk("127.0.0.1", 8888);

    sdk.RegisterSensorCallback([](uint16_t msg_type, const uint8_t* data, size_t len) {
        std::cout << "[回调通知] 收到下位机数据，消息类型: 0x" << std::hex << msg_type 
                  << std::dec << "，数据长度: " << len << " 字节\n";
    });

    if (!sdk.Init()) {
        std::cerr << "SDK 初始化失败，无法连接下位机服务器！\n";
        return -1;
    }

    for (int i = 1; i <= 3; ++i) {
        embodied_robot::ChassisVelocity vel{0.2f * i, 0.0f, 0.1f * i};
        std::cout << "正在下发底盘控制指令 -> 线速度X: " << vel.linear_x << " m/s\n";
        sdk.SetChassisVelocity(vel);
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    sdk.Shutdown();
    std::cout << "演示程序安全退出。\n";
    return 0;
}