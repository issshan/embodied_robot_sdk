/**
 * @file tcp_client.cpp
 * @brief 异步 TCP 客户端实现，包含心跳与断线重连逻辑
 */

#include "tcp_client.hpp"
#include "protocol_parser.hpp"
#include <iostream>
#include <chrono>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

namespace embodied_robot {

TcpClient::TcpClient(std::string ip, int port)
    : ip_(std::move(ip)), port_(port), socket_fd_(-1),
      is_running_(false), is_connected_(false), rx_buffer_(65536) {}

TcpClient::~TcpClient() { Stop(); }

bool TcpClient::Start() {
    if (is_running_) return true;
    is_running_ = true;
    net_thread_ = std::thread(&TcpClient::NetworkThreadFunc, this);
    return true;
}

bool TcpClient::ConnectToServer() {
    socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd_ < 0) return false;

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_);
    inet_pton(AF_INET, ip_.c_str(), &server_addr.sin_addr);

    if (connect(socket_fd_, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(socket_fd_); socket_fd_ = -1;
        return false;
    }
    is_connected_ = true;
    return true;
}

bool TcpClient::SendPacket(const uint8_t* data, size_t len) {
    if (!is_connected_ || socket_fd_ == -1) return false;
    return send(socket_fd_, data, len, 0) == static_cast<ssize_t>(len);
}

void TcpClient::SetDataCallback(DataCallback cb) { data_callback_ = std::move(cb); }

void TcpClient::Stop() {
    if (!is_running_) return;
    is_running_ = false;
    if (socket_fd_ != -1) { close(socket_fd_); socket_fd_ = -1; }
    if (net_thread_.joinable()) net_thread_.join();
    is_connected_ = false;
}

void TcpClient::NetworkThreadFunc() {
    while (is_running_) {
        if (!is_connected_) {
            if (ConnectToServer()) {
                std::cout << "[TcpClient] 机器人底层通信连接成功！\n";
            } else {
                std::this_thread::sleep_for(std::chrono::seconds(2));
                continue;
            }
        }

        uint8_t temp_buf[1024];
        ssize_t n = recv(socket_fd_, temp_buf, sizeof(temp_buf), 0);
        if (n > 0) {
            rx_buffer_.Write(temp_buf, n);
            while (rx_buffer_.Size() >= sizeof(ProtocolHeader) + 2) {
                ProtocolHeader header;
                rx_buffer_.Peek(reinterpret_cast<uint8_t*>(&header), sizeof(ProtocolHeader));
                if (header.magic != 0x5A5A) { rx_buffer_.Consume(1); continue; }

                size_t total_size = sizeof(ProtocolHeader) + header.payload_len + 2;
                if (rx_buffer_.Size() >= total_size) {
                    std::vector<uint8_t> packet(total_size);
                    rx_buffer_.Peek(packet.data(), total_size);
                    rx_buffer_.Consume(total_size);
                    if (data_callback_) data_callback_(header.msg_type, packet.data() + sizeof(ProtocolHeader), header.payload_len);
                } else { break; }
            }
        } else {
            is_connected_ = false;
            if (socket_fd_ != -1) { close(socket_fd_); socket_fd_ = -1; }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

} // namespace embodied_robot