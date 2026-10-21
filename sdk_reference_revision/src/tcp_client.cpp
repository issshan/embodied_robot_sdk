#include "tcp_client.hpp"
#include "send_all.hpp"
#include <algorithm>
#include <stdexcept>
namespace embodied_robot {
TcpClient::TcpClient(std::string ip, int port, ClientOptions options)
    : ip_(std::move(ip)), port_(static_cast<uint16_t>(port)), options_(options) {
    in_addr address{};
    if (!platform::InitSockets() || port < 1 || port > 65535 ||
        !platform::ParseIPv4(ip_, address) ||
        !options.connect_timeout_ms || !options.send_timeout_ms ||
        !options.heartbeat_interval_ms || options.heartbeat_timeout_ms <= options.heartbeat_interval_ms ||
        !options.reconnect_initial_ms || options.reconnect_max_ms < options.reconnect_initial_ms ||
        !options.max_pending_packets) throw std::invalid_argument("invalid client configuration");
}
TcpClient::~TcpClient() { Stop(); }
bool TcpClient::Start() {
    platform::Lock guard(lifecycle_mutex_);
    if (running_) return true;
    running_ = true;
    try { worker_.Start([this] { Run(); }); }
    catch (...) { running_ = false; return false; }
    return true;
}
void TcpClient::Stop() {
    // Never destroy this object or call lifecycle methods from its callback.
    platform::Lock guard(lifecycle_mutex_);
    if (worker_.IsCurrent()) throw std::logic_error("Stop must be called outside callback");
    running_ = false;
    worker_.Join(); // Worker owns socket; polling and sleeps are cancellation-aware.
    connected_ = false;
    ClearQueue();
}
void TcpClient::Pause(unsigned ms) const {
    const auto end = platform::NowMs() + ms;
    while (running_) {
        const auto now = platform::NowMs();
        if (now >= end) break;
        platform::SleepMs(static_cast<unsigned>(std::min<uint64_t>(20, end-now)));
    }
}
bool TcpClient::WaitUntilConnected(unsigned timeout_ms) const {
    const auto end = platform::NowMs() + timeout_ms;
    while (running_ && !connected_ && platform::NowMs() < end) platform::SleepMs(5);
    return connected_;
}
bool TcpClient::SendPacket(const uint8_t* data, size_t len) {
    if (!data || !len || len > kHeaderSize + kMaxPayload + 2) return false;
    platform::Lock guard(queue_mutex_);
    if (!running_ || !connected_ || queue_.size() >= options_.max_pending_packets) return false;
    queue_.emplace_back(data, data+len);
    return true;
}
void TcpClient::ClearQueue() {
    platform::Lock guard(queue_mutex_);
    dropped_ += queue_.size(); queue_.clear();
}
void TcpClient::SetDataCallback(DataCallback cb) {
    platform::Lock guard(callback_mutex_); callback_ = std::move(cb);
}
bool TcpClient::WriteAll(platform::Socket socket, const std::vector<uint8_t>& bytes) {
    const auto deadline = platform::NowMs() + options_.send_timeout_ms;
    const bool complete = SendAll(bytes.data(), bytes.size(),
        [&](const uint8_t* data, size_t len) {
            const int count = platform::Send(socket, data, static_cast<int>(len));
            if (count > 0) return count;
            const int error = platform::LastError();
            return count < 0 && (platform::Pending(error) || platform::Interrupted(error)) ? 0 : -1;
        }, [&] {
            while (running_) {
                const auto now = platform::NowMs();
                if (now >= deadline) return false;
                const int ready = platform::Wait(socket, true, static_cast<unsigned>(std::min<uint64_t>(20, deadline-now)));
                if (ready < 0) return false;
                if (ready > 0) return true;
            }
            return false;
        });
    if (complete) ++sent_;
    return complete;
}
void TcpClient::Run() {
    unsigned backoff = options_.reconnect_initial_ms;
    while (running_) {
        auto socket = ::socket(AF_INET, SOCK_STREAM, 0);
        if (socket == platform::kInvalid) { Pause(backoff); backoff = static_cast<unsigned>(std::min<uint64_t>(uint64_t(backoff)*2, options_.reconnect_max_ms)); continue; }
        if (!platform::Nonblocking(socket) || !platform::Connect(socket, ip_, port_, options_.connect_timeout_ms, [this] { return running_.load(); })) {
            platform::Close(socket); Pause(backoff);
            backoff = static_cast<unsigned>(std::min<uint64_t>(uint64_t(backoff)*2, options_.reconnect_max_ms)); continue;
        }
        ProtocolParser parser;
        auto last_ack = platform::NowMs(), last_ping = uint64_t(0);
        bool healthy = true, acknowledged = false;
        connected_ = true; ++connections_;
        while (running_ && healthy) {
            auto now = platform::NowMs();
            if (now - last_ack >= options_.heartbeat_timeout_ms) { ++timeouts_; break; }
            if (now - last_ping >= options_.heartbeat_interval_ms) {
                healthy = WriteAll(socket, ProtocolParser::Pack(kHeartbeatRequest, nullptr, 0));
                last_ping = now;
                if (!healthy) break;
            }
            std::vector<uint8_t> pending;
            { platform::Lock guard(queue_mutex_); if (!queue_.empty()) { pending = std::move(queue_.front()); queue_.pop_front(); } }
            if (!pending.empty() && !WriteAll(socket, pending)) { ++dropped_; break; }
            const int ready = platform::Wait(socket, false, 10);
            if (ready < 0) break;
            if (!ready) continue;
            uint8_t bytes[1024];
            const int count = platform::Receive(socket, bytes, sizeof(bytes));
            if (count == 0) break;
            if (count < 0) {
                if (platform::Pending(platform::LastError()) || platform::Interrupted(platform::LastError())) continue;
                break;
            }
            if (!parser.Feed(bytes, static_cast<size_t>(count))) break; // Overflow invalidates this stream.
            Frame frame;
            while (running_ && parser.Next(frame)) {
                ++received_;
                if (frame.type == kHeartbeatReply && frame.payload.empty()) {
                    last_ack = platform::NowMs(); acknowledged = true;
                } else {
                    DataCallback cb;
                    { platform::Lock guard(callback_mutex_); cb = callback_; }
                    try { if (cb) cb(frame.type, frame.payload.data(), frame.payload.size()); }
                    catch (...) { ++callback_errors_; }
                }
            }
        }
        crc_ += parser.Stats().crc_errors; lengths_ += parser.Stats().invalid_lengths;
        connected_ = false; ++disconnects_;
        platform::Close(socket); ClearQueue();
        if (acknowledged) backoff = options_.reconnect_initial_ms;
        Pause(backoff);
        backoff = static_cast<unsigned>(std::min<uint64_t>(uint64_t(backoff)*2, options_.reconnect_max_ms));
    }
}
ClientStats TcpClient::Stats() const {
    return {connections_.load(), disconnects_.load(), sent_.load(), received_.load(), crc_.load(),
        lengths_.load(), timeouts_.load(), callback_errors_.load(), dropped_.load()};
}
}
