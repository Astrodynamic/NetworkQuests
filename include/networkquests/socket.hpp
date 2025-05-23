#pragma once

#include "common.hpp"
#include "logger.hpp"

#include <string>
#include <span>
#include <vector>
#include <optional>
#include <chrono>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    using SocketHandle = SOCKET;
    constexpr SocketHandle INVALID_SOCKET_HANDLE = INVALID_SOCKET;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    using SocketHandle = int;
    constexpr SocketHandle INVALID_SOCKET_HANDLE = -1;
#endif

namespace networkquests {

enum class SocketType {
    TCP = SOCK_STREAM,
    UDP = SOCK_DGRAM
};

enum class AddressFamily {
    IPv4 = AF_INET,
    IPv6 = AF_INET6
};

class SocketAddress {
public:
    SocketAddress() = default;
    
    SocketAddress(std::string_view ip, Port port, AddressFamily family = AddressFamily::IPv4);
    
    SocketAddress(const sockaddr_in& addr);
    SocketAddress(const sockaddr_in6& addr);
    
    [[nodiscard]] std::string to_string() const;
    [[nodiscard]] Port port() const noexcept { return port_; }
    [[nodiscard]] AddressFamily family() const noexcept { return family_; }
    [[nodiscard]] const sockaddr* sockaddr_ptr() const noexcept;
    [[nodiscard]] socklen_t sockaddr_size() const noexcept;

    // Static factory methods
    [[nodiscard]] static Result<SocketAddress> from_ipv4(std::string_view ip, Port port);
    [[nodiscard]] static Result<SocketAddress> from_ipv6(std::string_view ip, Port port);
    [[nodiscard]] static Result<SocketAddress> resolve(std::string_view hostname, Port port);

private:
    std::string ip_;
    Port port_ = 0;
    AddressFamily family_ = AddressFamily::IPv4;
    mutable sockaddr_in addr4_{};
    mutable sockaddr_in6 addr6_{};
    mutable bool addr_initialized_ = false;
    
    void initialize_sockaddr() const;
};

class Socket {
public:
    Socket() = default;
    explicit Socket(SocketType type, AddressFamily family = AddressFamily::IPv4);
    
    // Move-only type
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    Socket(Socket&& other) noexcept;
    Socket& operator=(Socket&& other) noexcept;
    
    ~Socket();

    // Basic operations
    [[nodiscard]] Result<void> bind(const SocketAddress& addr);
    [[nodiscard]] Result<void> listen(int backlog = 5);
    [[nodiscard]] Result<Socket> accept();
    [[nodiscard]] Result<void> connect(const SocketAddress& addr);
    
    // Data transfer
    [[nodiscard]] Result<std::size_t> send(std::span<const std::byte> data);
    [[nodiscard]] Result<std::size_t> receive(std::span<std::byte> buffer);
    
    // UDP-specific operations
    [[nodiscard]] Result<std::size_t> send_to(std::span<const std::byte> data, 
                                               const SocketAddress& addr);
    [[nodiscard]] Result<std::pair<std::size_t, SocketAddress>> receive_from(std::span<std::byte> buffer);
    
    // Socket options
    [[nodiscard]] Result<void> set_reuse_address(bool enable = true);
    [[nodiscard]] Result<void> set_non_blocking(bool enable = true);
    [[nodiscard]] Result<void> set_timeout(const Timeout& timeout);
    [[nodiscard]] Result<void> set_broadcast(bool enable = true);
    [[nodiscard]] Result<void> set_receive_timeout(std::chrono::milliseconds timeout);
    [[nodiscard]] Result<void> set_send_timeout(std::chrono::milliseconds timeout);
    
    // Status and info
    [[nodiscard]] bool is_valid() const noexcept { return handle_ != INVALID_SOCKET_HANDLE; }
    [[nodiscard]] SocketHandle handle() const noexcept { return handle_; }
    [[nodiscard]] SocketHandle native_handle() const noexcept { return handle_; }
    [[nodiscard]] SocketType type() const noexcept { return type_; }
    [[nodiscard]] AddressFamily family() const noexcept { return family_; }
    
    // Get local and remote addresses
    [[nodiscard]] Result<SocketAddress> local_address() const;
    [[nodiscard]] Result<SocketAddress> remote_address() const;
    
    void close();

private:
    SocketHandle handle_ = INVALID_SOCKET_HANDLE;
    SocketType type_ = SocketType::TCP;
    AddressFamily family_ = AddressFamily::IPv4;
    
    explicit Socket(SocketHandle handle, SocketType type, AddressFamily family);
    
    [[nodiscard]] std::error_code get_last_socket_error() const;
};

// Utility functions
namespace socket_utils {

[[nodiscard]] Result<void> initialize_networking();
void cleanup_networking();

[[nodiscard]] Result<std::vector<SocketAddress>> resolve_hostname(
    std::string_view hostname, 
    Port port,
    SocketType type = SocketType::TCP
);

class NetworkingRAII {
public:
    NetworkingRAII();
    ~NetworkingRAII();
    
    NetworkingRAII(const NetworkingRAII&) = delete;
    NetworkingRAII& operator=(const NetworkingRAII&) = delete;
    
    [[nodiscard]] bool is_initialized() const noexcept { return initialized_; }

private:
    bool initialized_ = false;
};

} // namespace socket_utils

} // namespace networkquests