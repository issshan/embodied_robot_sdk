#include "robot_sdk.hpp"
#include <cmath>
#include <cstring>
#include <limits>
namespace embodied_robot {
std::vector<uint8_t> EncodeVelocity(const ChassisVelocity& v) {
    static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559, "IEEE754 required");
    if (!std::isfinite(v.linear_x) || !std::isfinite(v.linear_y) || !std::isfinite(v.angular_z))
        throw std::invalid_argument("velocity must be finite");
    std::vector<uint8_t> bytes(12);
    const float values[] = {v.linear_x, v.linear_y, v.angular_z};
    for (unsigned i = 0; i < 3; ++i) {
        uint32_t bits; std::memcpy(&bits, &values[i], 4);
        for (unsigned j = 0; j < 4; ++j) bytes[i*4+j] = static_cast<uint8_t>(bits >> (j*8));
    }
    return bytes;
}
bool DecodeVelocity(const uint8_t* bytes, size_t len, ChassisVelocity& v) {
    if (!bytes || len != 12) return false;
    float values[3];
    for (unsigned i = 0; i < 3; ++i) {
        uint32_t bits = 0;
        for (unsigned j = 0; j < 4; ++j) bits |= uint32_t(bytes[i*4+j]) << (j*8);
        std::memcpy(&values[i], &bits, 4);
        if (!std::isfinite(values[i])) return false;
    }
    v = {values[0], values[1], values[2]}; return true;
}
RobotSDK::RobotSDK(std::string ip, int port, ClientOptions options)
    : client_(std::make_unique<TcpClient>(std::move(ip), port, options)) {}
RobotSDK::~RobotSDK() { Shutdown(); }
bool RobotSDK::Init() { return client_->Start(); }
void RobotSDK::Shutdown() { client_->Stop(); }
bool RobotSDK::WaitUntilConnected(unsigned ms) const { return client_->WaitUntilConnected(ms); }
bool RobotSDK::IsConnected() const { return client_->IsConnected(); }
bool RobotSDK::SetChassisVelocity(const ChassisVelocity& v) {
    const auto payload = EncodeVelocity(v);
    const auto packet = ProtocolParser::Pack(kVelocityCommand, payload.data(), static_cast<uint32_t>(payload.size()));
    return client_->SendPacket(packet.data(), packet.size());
}
void RobotSDK::RegisterSensorCallback(TcpClient::DataCallback cb) { client_->SetDataCallback(std::move(cb)); }
ClientStats RobotSDK::Stats() const { return client_->Stats(); }
}
