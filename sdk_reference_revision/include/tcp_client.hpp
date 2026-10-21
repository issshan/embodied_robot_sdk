#pragma once
#include "platform.hpp"
#include "protocol_parser.hpp"
#include <atomic>
#include <deque>
#include <functional>
#include <string>
#include <vector>
namespace embodied_robot {
struct ClientOptions {
    unsigned connect_timeout_ms = 500;
    unsigned send_timeout_ms = 500;
    unsigned heartbeat_interval_ms = 250;
    unsigned heartbeat_timeout_ms = 1000;
    unsigned reconnect_initial_ms = 100;
    unsigned reconnect_max_ms = 1600;
    size_t max_pending_packets = 64;
};
struct ClientStats {
    uint64_t connections, disconnects, sent_packets, received_frames;
    uint64_t crc_errors, invalid_lengths, heartbeat_timeouts, callback_errors, dropped_packets;
};
class TcpClient {
public:
    using DataCallback = std::function<void(uint16_t, const uint8_t*, size_t)>;
    TcpClient(std::string ip, int port, ClientOptions options = {});
    ~TcpClient();
    // Start means worker started, NOT connected. Use IsConnected/WaitUntilConnected.
    bool Start();
    void Stop();
    bool IsConnected() const { return connected_.load(); }
    bool WaitUntilConnected(unsigned timeout_ms) const;
    // True means queued, NOT acknowledged or executed by a robot.
    bool SendPacket(const uint8_t* data, size_t len);
    void SetDataCallback(DataCallback cb);
    ClientStats Stats() const;
private:
    void Run();
    void Pause(unsigned ms) const;
    bool WriteAll(platform::Socket socket, const std::vector<uint8_t>& bytes);
    void ClearQueue();
    std::string ip_;
    uint16_t port_;
    ClientOptions options_;
    std::atomic<bool> running_{false}, connected_{false};
    platform::Thread worker_;
    platform::Mutex lifecycle_mutex_, queue_mutex_, callback_mutex_;
    std::deque<std::vector<uint8_t>> queue_;
    DataCallback callback_;
    std::atomic<uint64_t> connections_{0}, disconnects_{0}, sent_{0}, received_{0};
    std::atomic<uint64_t> crc_{0}, lengths_{0}, timeouts_{0}, callback_errors_{0}, dropped_{0};
};
}
