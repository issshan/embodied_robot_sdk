#pragma once
#include "tcp_client.hpp"
#include <memory>
namespace embodied_robot {
struct ChassisVelocity { float linear_x, linear_y, angular_z; };
// Payload: three IEEE754 binary32 values encoded little-endian, 12 bytes.
std::vector<uint8_t> EncodeVelocity(const ChassisVelocity& velocity);
bool DecodeVelocity(const uint8_t* data, size_t len, ChassisVelocity& velocity);
class RobotSDK {
public:
    RobotSDK(std::string ip, int port, ClientOptions options = {});
    ~RobotSDK();
    bool Init();
    void Shutdown();
    bool WaitUntilConnected(unsigned timeout_ms) const;
    bool IsConnected() const;
    bool SetChassisVelocity(const ChassisVelocity& velocity);
    void RegisterSensorCallback(TcpClient::DataCallback callback);
    ClientStats Stats() const;
private:
    std::unique_ptr<TcpClient> client_;
};
}
