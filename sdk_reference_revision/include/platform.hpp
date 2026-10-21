#pragma once

// Small platform boundary. Linux uses the standard threading library;
// Win32 support also permits the preinstalled MinGW build without winpthreads.
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <functional>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <mutex>
#include <sys/select.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#endif

namespace embodied_robot {
namespace platform {
#ifdef _WIN32
using Socket = SOCKET;
constexpr Socket kInvalid = INVALID_SOCKET;
class Mutex {
public:
    Mutex() { InitializeCriticalSection(&mutex_); }
    ~Mutex() { DeleteCriticalSection(&mutex_); }
    Mutex(const Mutex&) = delete;
    Mutex& operator=(const Mutex&) = delete;
    void lock() { EnterCriticalSection(&mutex_); }
    void unlock() { LeaveCriticalSection(&mutex_); }
private:
    CRITICAL_SECTION mutex_;
};
class Thread {
public:
    Thread() = default;
    Thread(const Thread&) = delete;
    Thread& operator=(const Thread&) = delete;
    ~Thread() { if (handle_) std::terminate(); }
    void Start(std::function<void()> fn) {
        if (handle_) throw std::logic_error("thread already started");
        auto* task = new std::function<void()>(std::move(fn));
        handle_ = CreateThread(nullptr, 0, Run, task, 0, &thread_id_);
        if (!handle_) { delete task; throw std::runtime_error("CreateThread failed"); }
    }
    bool IsCurrent() const { return handle_ && thread_id_ == GetCurrentThreadId(); }
    void Join() {
        if (handle_) { WaitForSingleObject(handle_, INFINITE); CloseHandle(handle_); handle_ = nullptr; }
    }
private:
    static DWORD WINAPI Run(void* p) {
        auto* task = static_cast<std::function<void()>*>(p);
        try { (*task)(); } catch (...) { delete task; std::terminate(); }
        delete task;
        return 0;
    }
    HANDLE handle_ = nullptr;
    DWORD thread_id_ = 0;
};
inline void SleepMs(unsigned ms) { Sleep(ms); }
inline int LastError() { return WSAGetLastError(); }
inline bool Pending(int e) { return e == WSAEWOULDBLOCK || e == WSAEINPROGRESS; }
inline bool Interrupted(int e) { return e == WSAEINTR; }
inline void Close(Socket s) { closesocket(s); }
inline bool Nonblocking(Socket s) { u_long on = 1; return ioctlsocket(s, FIONBIO, &on) == 0; }
inline bool InitSockets() {
    static const bool ready = [] { WSADATA data; return WSAStartup(MAKEWORD(2, 2), &data) == 0; }();
    return ready;
}
inline int Send(Socket s, const uint8_t* p, int n) { return send(s, reinterpret_cast<const char*>(p), n, 0); }
inline int Receive(Socket s, uint8_t* p, int n) { return recv(s, reinterpret_cast<char*>(p), n, 0); }
#else
using Socket = int;
constexpr Socket kInvalid = -1;
using Mutex = std::mutex;
class Thread {
public:
    void Start(std::function<void()> fn) { thread_ = std::thread(std::move(fn)); }
    bool IsCurrent() const { return thread_.joinable() && thread_.get_id() == std::this_thread::get_id(); }
    void Join() { if (thread_.joinable()) thread_.join(); }
private:
    std::thread thread_;
};
inline void SleepMs(unsigned ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }
inline int LastError() { return errno; }
inline bool Pending(int e) { return e == EAGAIN || e == EWOULDBLOCK || e == EINPROGRESS; }
inline bool Interrupted(int e) { return e == EINTR; }
inline void Close(Socket s) { close(s); }
inline bool Nonblocking(Socket s) {
    const int flags = fcntl(s, F_GETFL, 0);
    return flags >= 0 && fcntl(s, F_SETFL, flags | O_NONBLOCK) == 0;
}
inline bool InitSockets() { return true; }
inline int Send(Socket s, const uint8_t* p, int n) { return static_cast<int>(send(s, p, n, MSG_NOSIGNAL)); }
inline int Receive(Socket s, uint8_t* p, int n) { return static_cast<int>(recv(s, p, n, 0)); }
#endif

class Lock {
public:
    explicit Lock(Mutex& m) : mutex_(m) { mutex_.lock(); }
    ~Lock() { mutex_.unlock(); }
    Lock(const Lock&) = delete;
    Lock& operator=(const Lock&) = delete;
private:
    Mutex& mutex_;
};
inline uint64_t NowMs() {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}
// Returns 1 for ready, 0 for timeout, -1 for error. Never resets the timeout
// on a signal: the caller owns its absolute deadline.
inline int Wait(Socket s, bool write, unsigned ms) {
#ifndef _WIN32
    if (s < 0 || s >= FD_SETSIZE) return -1;
#endif
    fd_set set;
    FD_ZERO(&set); FD_SET(s, &set);
    timeval tv{static_cast<long>(ms / 1000), static_cast<long>((ms % 1000) * 1000)};
    const int rc = select(static_cast<int>(s + 1), write ? nullptr : &set,
                          write ? &set : nullptr, nullptr, &tv);
    if (rc < 0 && Interrupted(LastError())) return 0;
    return rc < 0 ? -1 : (rc > 0 ? 1 : 0);
}
// Strict dotted-decimal IPv4 only. No DNS lookup or octal shorthand.
inline bool ParseIPv4(const std::string& text, in_addr& address) {
    uint32_t value = 0;
    size_t position = 0;
    for (unsigned part = 0; part < 4; ++part) {
        unsigned number = 0, digits = 0;
        while (position < text.size() && text[position] >= '0' && text[position] <= '9') {
            number = number * 10 + static_cast<unsigned>(text[position++] - '0');
            if (++digits > 3 || number > 255) return false;
        }
        if (!digits) return false;
        value = (value << 8) | number;
        if (part < 3 && (position >= text.size() || text[position++] != '.')) return false;
    }
    if (position != text.size()) return false;
    address.s_addr = htonl(value); return true;
}
inline bool Connect(Socket s, const std::string& ip, uint16_t port, unsigned timeout_ms,
                    const std::function<bool()>& running) {
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (!ParseIPv4(ip, addr.sin_addr)) return false;
    if (connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0) return true;
    if (!Pending(LastError())) return false;
    const auto deadline = NowMs() + timeout_ms;
    while (running()) {
        const auto now = NowMs();
        if (now >= deadline) break;
        const int rc = Wait(s, true, static_cast<unsigned>(std::min<uint64_t>(50, deadline - now)));
        if (rc < 0) return false;
        if (rc == 0) continue;
        int error = 0;
#ifdef _WIN32
        int size = sizeof(error);
        return getsockopt(s, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&error), &size) == 0 && error == 0;
#else
        socklen_t size = sizeof(error);
        return getsockopt(s, SOL_SOCKET, SO_ERROR, &error, &size) == 0 && error == 0;
#endif
    }
    return false;
}
} // namespace platform
} // namespace embodied_robot
