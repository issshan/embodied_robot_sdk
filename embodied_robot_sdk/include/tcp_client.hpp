/**
 * @file tcp_client.hpp
 * @brief 异步 TCP 客户端与 Socket 通信管理类
 */

#pragma once

#include "ring_buffer.hpp"
#include <string>
#include <thread>
#include <atomic>
#include <functional>
#include <cstdint>

namespace embodied_robot {

class TcpClient {
public:
    using DataCallback = std::function<void(uint16_t msg_type, const uint8_t* data, size_t len)>;

    TcpClient(std::string ip, int port);
    ~TcpClient();

    bool Start();
    void Stop();
    bool SendPacket(const uint8_t* data, size_t len);
    void SetDataCallback(DataCallback cb);
    bool IsConnected() const { return is_connected_; }

private:
    void NetworkThreadFunc();
    bool ConnectToServer();

    std::string ip_;
    int port_;
    int socket_fd_;
    std::atomic<bool> is_running_;
    std::atomic<bool> is_connected_;

    std::thread net_thread_;
    RingBuffer rx_buffer_;
    DataCallback data_callback_;
};

} // namespace embodied_robot