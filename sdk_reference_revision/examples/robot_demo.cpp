#include "robot_sdk.hpp"
#include <atomic>
#include <cstdlib>
#include <iostream>
int main(int argc, char** argv) {
    try {
        const int port = argc > 1 ? std::stoi(argv[1]) : 8888;
        std::atomic<unsigned> states{0};
        embodied_robot::RobotSDK sdk("127.0.0.1", port);
        sdk.RegisterSensorCallback([&](uint16_t type, const uint8_t* bytes, size_t size) {
            embodied_robot::ChassisVelocity v{};
            if (type == embodied_robot::kRobotState && embodied_robot::DecodeVelocity(bytes, size, v)) {
                ++states; std::cout << "state vx=" << v.linear_x << " vy=" << v.linear_y << " wz=" << v.angular_z << '\n';
            }
        });
        if (!sdk.Init() || !sdk.WaitUntilConnected(3000)) { std::cerr << "connection failed\n"; return 1; }
        for (int i = 1; i <= 3; ++i) {
            if (!sdk.SetChassisVelocity({0.2f*i, 0, 0.1f*i})) { std::cerr << "queue rejected\n"; return 2; }
            embodied_robot::platform::SleepMs(100);
        }
        const auto end = embodied_robot::platform::NowMs()+2000;
        while (states < 3 && embodied_robot::platform::NowMs() < end) embodied_robot::platform::SleepMs(10);
        sdk.Shutdown();
        std::cout << "received_states=" << states << '\n';
        return states == 3 ? 0 : 3;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 4; }
}
