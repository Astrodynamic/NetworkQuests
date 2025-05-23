#include "networkquests/socket.hpp"

#include <cstring>
#include <stdexcept>

#ifdef _WIN32
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <errno.h>
    #include <netdb.h>
#endif

namespace networkquests {

// SocketAddress implementation
SocketAddress::SocketAddress(std::string_view ip, Port port, AddressFamily family)
    : ip_(ip), port_(port), family_(family) {
    if (!utils::is_valid_port(port)) {
        throw std::invalid_argument("Invalid port number");
    }
    
    if (family == AddressFamily::IPv4 && !utils::is_valid_ipv4(ip)) {
        throw std::invalid_argument("Invalid IPv4 address");
    }
}

SocketAddress::SocketAddress(const sockaddr_in& addr) 
    : port_(ntohs(addr.sin_port)), family_(AddressFamily::IPv4) {
    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr.sin_addr, ip_str, INET_ADDRSTRLEN);
    ip_ = ip_str;
}

SocketAddress::SocketAddress(const sockaddr_in6& addr)
    : port_(ntohs(addr.sin6_port)), family_(AddressFamily::IPv6) {
    char ip_str[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET6, &addr.sin6_addr, ip_str, INET6_ADDRSTRLEN);
    ip_ = ip_str;
}

std::string SocketAddress::to_string() const {
    if (family_ == AddressFamily::IPv6) {
        return "[" + ip_ + "]:" + std::to_string(port_);
    }
    return ip_ + ":" + std::to_string(port_);
}

const sockaddr* SocketAddress::sockaddr_ptr() const noexcept {
    initialize_sockaddr();
    return family_ == AddressFamily::IPv4 
        ? reinterpret_cast<const sockaddr*>(&addr4_)
        : reinterpret_cast<const sockaddr*>(&addr6_);
}

socklen_t SocketAddress::sockaddr_size() const noexcept {
    return family_ == AddressFamily::IPv4 
        ? sizeof(sockaddr_in)
        : sizeof(sockaddr_in6);
}

void SocketAddress::initialize_sockaddr() const {
    if (addr_initialized_) return;
    
    if (family_ == AddressFamily::IPv4) {
        std::memset(&addr4_, 0, sizeof(addr4_));
        addr4_.sin_family = AF_INET;
        addr4_.sin_port = htons(port_);
        inet_pton(AF_INET, ip_.c_str(), &addr4_.sin_addr);
    } else {
        std::memset(&addr6_, 0, sizeof(addr6_));
        addr6_.sin6_family = AF_INET6;
        addr6_.sin6_port = htons(port_);
        inet_pton(AF_INET6, ip_.c_str(), &addr6_.sin6_addr);
    }
    
    addr_initialized_ = true;
}

Result<SocketAddress> SocketAddress::from_ipv4(std::string_view ip, Port port) {
    if (!utils::is_valid_port(port)) {
        return make_error_result<SocketAddress>(NetworkError::InvalidArgument);
    }
    
    if (!utils::is_valid_ipv4(ip)) {
        return make_error_result<SocketAddress>(NetworkError::InvalidAddress);
    }
    
    try {
        return Result<SocketAddress>::success(SocketAddress(ip, port, AddressFamily::IPv4));
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to create IPv4 address: {}", e.what());
        return make_error_result<SocketAddress>(NetworkError::InvalidAddress);
    }
}

Result<SocketAddress> SocketAddress::from_ipv6(std::string_view ip, Port port) {
    if (!utils::is_valid_port(port)) {
        return make_error_result<SocketAddress>(NetworkError::InvalidArgument);
    }
    
    try {
        return Result<SocketAddress>::success(SocketAddress(ip, port, AddressFamily::IPv6));
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to create IPv6 address: {}", e.what());
        return make_error_result<SocketAddress>(NetworkError::InvalidAddress);
    }
}

Result<SocketAddress> SocketAddress::resolve(std::string_view hostname, Port port) {
    auto addresses = socket_utils::resolve_hostname(hostname, port);
    if (!addresses) {
        return make_error_result<SocketAddress>(addresses.error());
    }
    
    if (addresses.value().empty()) {
        return make_error_result<SocketAddress>(NetworkError::AddressResolutionFailed);
    }
    
    // Return the first resolved address
    return Result<SocketAddress>::success(addresses.value()[0]);
}

// Socket implementation
Socket::Socket(SocketType type, AddressFamily family) 
    : type_(type), family_(family) {
    
    handle_ = socket(static_cast<int>(family), static_cast<int>(type), 0);
    if (handle_ == INVALID_SOCKET_HANDLE) {
        auto error = get_last_socket_error();
        LOG_ERROR("Failed to create socket: {}", error.message());
        throw std::system_error(error);
    }
    
    LOG_DEBUG("Created socket with handle {}", static_cast<int>(handle_));
}

Socket::Socket(SocketHandle handle, SocketType type, AddressFamily family)
    : handle_(handle), type_(type), family_(family) {
}

Socket::Socket(Socket&& other) noexcept {
    *this = std::move(other);
}

Socket& Socket::operator=(Socket&& other) noexcept {
    if (this != &other) {
        close();
        
        handle_ = other.handle_;
        type_ = other.type_;
        family_ = other.family_;
        
        other.handle_ = INVALID_SOCKET_HANDLE;
    }
    return *this;
}

Socket::~Socket() {
    close();
}

Result<void> Socket::bind(const SocketAddress& addr) {
    if (!is_valid()) {
        return make_error_code(NetworkError::BindFailed);
    }
    
    if (::bind(handle_, addr.sockaddr_ptr(), addr.sockaddr_size()) < 0) {
        auto error = get_last_socket_error();
        LOG_ERROR("Bind failed for address {}: {}", addr.to_string(), error.message());
        return error;
    }
    
    LOG_DEBUG("Successfully bound socket to {}", addr.to_string());
    return Result<void>{};
}

Result<void> Socket::listen(int backlog) {
    if (!is_valid()) {
        return make_error_code(NetworkError::ListenFailed);
    }
    
    if (::listen(handle_, backlog) < 0) {
        auto error = get_last_socket_error();
        LOG_ERROR("Listen failed: {}", error.message());
        return error;
    }
    
    LOG_DEBUG("Socket listening with backlog {}", backlog);
    return Result<void>{};
}

Result<Socket> Socket::accept() {
    if (!is_valid()) {
        return make_error_code(NetworkError::AcceptFailed);
    }
    
    sockaddr_storage client_addr{};
    socklen_t client_addr_len = sizeof(client_addr);
    
    SocketHandle client_handle = ::accept(handle_, 
        reinterpret_cast<sockaddr*>(&client_addr), &client_addr_len);
    
    if (client_handle == INVALID_SOCKET_HANDLE) {
        auto error = get_last_socket_error();
        LOG_ERROR("Accept failed: {}", error.message());
        return error;
    }
    
    LOG_DEBUG("Accepted new connection with handle {}", static_cast<int>(client_handle));
    return Socket(client_handle, type_, family_);
}

Result<void> Socket::connect(const SocketAddress& addr) {
    if (!is_valid()) {
        return make_error_code(NetworkError::ConnectionFailed);
    }
    
    if (::connect(handle_, addr.sockaddr_ptr(), addr.sockaddr_size()) < 0) {
        auto error = get_last_socket_error();
        LOG_ERROR("Connect failed to {}: {}", addr.to_string(), error.message());
        return error;
    }
    
    LOG_DEBUG("Successfully connected to {}", addr.to_string());
    return Result<void>{};
}

Result<std::size_t> Socket::send(std::span<const std::byte> data) {
    if (!is_valid()) {
        return make_error_code(NetworkError::SendFailed);
    }
    
    auto bytes_sent = ::send(handle_, 
        reinterpret_cast<const char*>(data.data()), 
        static_cast<int>(data.size()), 0);
    
    if (bytes_sent < 0) {
        auto error = get_last_socket_error();
        LOG_ERROR("Send failed: {}", error.message());
        return error;
    }
    
    LOG_DEBUG("Sent {} bytes", bytes_sent);
    return static_cast<std::size_t>(bytes_sent);
}

Result<std::size_t> Socket::receive(std::span<std::byte> buffer) {
    if (!is_valid()) {
        return make_error_code(NetworkError::ReceiveFailed);
    }
    
    auto bytes_received = ::recv(handle_,
        reinterpret_cast<char*>(buffer.data()),
        static_cast<int>(buffer.size()), 0);
    
    if (bytes_received < 0) {
        auto error = get_last_socket_error();
        LOG_ERROR("Receive failed: {}", error.message());
        return error;
    }
    
    LOG_DEBUG("Received {} bytes", bytes_received);
    return static_cast<std::size_t>(bytes_received);
}

Result<std::size_t> Socket::send_to(std::span<const std::byte> data, const SocketAddress& addr) {
    if (!is_valid()) {
        return make_error_code(NetworkError::SendFailed);
    }
    
    auto bytes_sent = ::sendto(handle_,
        reinterpret_cast<const char*>(data.data()),
        static_cast<int>(data.size()), 0,
        addr.sockaddr_ptr(), addr.sockaddr_size());
    
    if (bytes_sent < 0) {
        auto error = get_last_socket_error();
        LOG_ERROR("SendTo failed to {}: {}", addr.to_string(), error.message());
        return error;
    }
    
    LOG_DEBUG("Sent {} bytes to {}", bytes_sent, addr.to_string());
    return static_cast<std::size_t>(bytes_sent);
}

Result<std::pair<std::size_t, SocketAddress>> Socket::receive_from(std::span<std::byte> buffer) {
    if (!is_valid()) {
        return make_error_code(NetworkError::ReceiveFailed);
    }
    
    sockaddr_storage sender_addr{};
    socklen_t sender_addr_len = sizeof(sender_addr);
    
    auto bytes_received = ::recvfrom(handle_,
        reinterpret_cast<char*>(buffer.data()),
        static_cast<int>(buffer.size()), 0,
        reinterpret_cast<sockaddr*>(&sender_addr), &sender_addr_len);
    
    if (bytes_received < 0) {
        auto error = get_last_socket_error();
        LOG_ERROR("ReceiveFrom failed: {}", error.message());
        return error;
    }
    
    SocketAddress addr;
    if (sender_addr.ss_family == AF_INET) {
        addr = SocketAddress(*reinterpret_cast<sockaddr_in*>(&sender_addr));
    } else if (sender_addr.ss_family == AF_INET6) {
        addr = SocketAddress(*reinterpret_cast<sockaddr_in6*>(&sender_addr));
    }
    
    LOG_DEBUG("Received {} bytes from {}", bytes_received, addr.to_string());
    return std::make_pair(static_cast<std::size_t>(bytes_received), addr);
}

Result<void> Socket::set_reuse_address(bool enable) {
    if (!is_valid()) {
        return make_error_code(NetworkError::ProtocolError);
    }
    
    int opt = enable ? 1 : 0;
    if (setsockopt(handle_, SOL_SOCKET, SO_REUSEADDR, 
                   reinterpret_cast<const char*>(&opt), sizeof(opt)) < 0) {
        auto error = get_last_socket_error();
        LOG_ERROR("Failed to set SO_REUSEADDR: {}", error.message());
        return error;
    }
    
    return Result<void>{};
}

Result<void> Socket::set_non_blocking(bool enable) {
    if (!is_valid()) {
        return make_error_code(NetworkError::ProtocolError);
    }
    
#ifdef _WIN32
    u_long mode = enable ? 1 : 0;
    if (ioctlsocket(handle_, FIONBIO, &mode) != 0) {
        auto error = get_last_socket_error();
        LOG_ERROR("Failed to set non-blocking mode: {}", error.message());
        return error;
    }
#else
    int flags = fcntl(handle_, F_GETFL, 0);
    if (flags < 0) {
        auto error = get_last_socket_error();
        return error;
    }
    
    flags = enable ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
    if (fcntl(handle_, F_SETFL, flags) < 0) {
        auto error = get_last_socket_error();
        LOG_ERROR("Failed to set non-blocking mode: {}", error.message());
        return error;
    }
#endif
    
    return Result<void>{};
}

Result<void> Socket::set_timeout(const Timeout& timeout) {
    if (!is_valid()) {
        return make_error_code(NetworkError::ProtocolError);
    }
    
#ifdef _WIN32
    DWORD timeout_ms = static_cast<DWORD>(timeout.count());
#else
    struct timeval tv;
    tv.tv_sec = timeout.count() / 1000;
    tv.tv_usec = (timeout.count() % 1000) * 1000;
    const auto& timeout_ms = tv;
#endif
    
    if (setsockopt(handle_, SOL_SOCKET, SO_RCVTIMEO,
                   reinterpret_cast<const char*>(&timeout_ms), sizeof(timeout_ms)) < 0 ||
        setsockopt(handle_, SOL_SOCKET, SO_SNDTIMEO,
                   reinterpret_cast<const char*>(&timeout_ms), sizeof(timeout_ms)) < 0) {
        auto error = get_last_socket_error();
        LOG_ERROR("Failed to set socket timeout: {}", error.message());
        return error;
    }
    
    return Result<void>{};
}

Result<void> Socket::set_broadcast(bool enable) {
    if (!is_valid()) {
        return make_error_code(NetworkError::ProtocolError);
    }
    
    int opt = enable ? 1 : 0;
    if (setsockopt(handle_, SOL_SOCKET, SO_BROADCAST, 
                   reinterpret_cast<const char*>(&opt), sizeof(opt)) < 0) {
        auto error = get_last_socket_error();
        LOG_ERROR("Failed to set SO_BROADCAST: {}", error.message());
        return error;
    }
    
    LOG_DEBUG("Set SO_BROADCAST to {}", enable);
    return Result<void>{};
}

Result<void> Socket::set_receive_timeout(std::chrono::milliseconds timeout) {
    if (!is_valid()) {
        return make_error_code(NetworkError::ProtocolError);
    }
    
#ifdef _WIN32
    DWORD timeout_ms = static_cast<DWORD>(timeout.count());
    if (setsockopt(handle_, SOL_SOCKET, SO_RCVTIMEO,
                   reinterpret_cast<const char*>(&timeout_ms), sizeof(timeout_ms)) < 0) {
#else
    struct timeval tv;
    tv.tv_sec = timeout.count() / 1000;
    tv.tv_usec = (timeout.count() % 1000) * 1000;
    if (setsockopt(handle_, SOL_SOCKET, SO_RCVTIMEO,
                   reinterpret_cast<const char*>(&tv), sizeof(tv)) < 0) {
#endif
        auto error = get_last_socket_error();
        LOG_ERROR("Failed to set receive timeout: {}", error.message());
        return error;
    }
    
    LOG_DEBUG("Set receive timeout to {}ms", timeout.count());
    return Result<void>{};
}

Result<void> Socket::set_send_timeout(std::chrono::milliseconds timeout) {
    if (!is_valid()) {
        return make_error_code(NetworkError::ProtocolError);
    }
    
#ifdef _WIN32
    DWORD timeout_ms = static_cast<DWORD>(timeout.count());
    if (setsockopt(handle_, SOL_SOCKET, SO_SNDTIMEO,
                   reinterpret_cast<const char*>(&timeout_ms), sizeof(timeout_ms)) < 0) {
#else
    struct timeval tv;
    tv.tv_sec = timeout.count() / 1000;
    tv.tv_usec = (timeout.count() % 1000) * 1000;
    if (setsockopt(handle_, SOL_SOCKET, SO_SNDTIMEO,
                   reinterpret_cast<const char*>(&tv), sizeof(tv)) < 0) {
#endif
        auto error = get_last_socket_error();
        LOG_ERROR("Failed to set send timeout: {}", error.message());
        return error;
    }
    
    LOG_DEBUG("Set send timeout to {}ms", timeout.count());
    return Result<void>{};
}

Result<SocketAddress> Socket::local_address() const {
    sockaddr_storage addr{};
    socklen_t addr_len = sizeof(addr);
    
    if (getsockname(handle_, reinterpret_cast<sockaddr*>(&addr), &addr_len) < 0) {
        auto error = get_last_socket_error();
        return error;
    }
    
    if (addr.ss_family == AF_INET) {
        return SocketAddress(*reinterpret_cast<sockaddr_in*>(&addr));
    } else if (addr.ss_family == AF_INET6) {
        return SocketAddress(*reinterpret_cast<sockaddr_in6*>(&addr));
    }
    
    return make_error_code(NetworkError::ProtocolError);
}

Result<SocketAddress> Socket::remote_address() const {
    sockaddr_storage addr{};
    socklen_t addr_len = sizeof(addr);
    
    if (getpeername(handle_, reinterpret_cast<sockaddr*>(&addr), &addr_len) < 0) {
        auto error = get_last_socket_error();
        return error;
    }
    
    if (addr.ss_family == AF_INET) {
        return SocketAddress(*reinterpret_cast<sockaddr_in*>(&addr));
    } else if (addr.ss_family == AF_INET6) {
        return SocketAddress(*reinterpret_cast<sockaddr_in6*>(&addr));
    }
    
    return make_error_code(NetworkError::ProtocolError);
}

void Socket::close() {
    if (is_valid()) {
        LOG_DEBUG("Closing socket with handle {}", static_cast<int>(handle_));
#ifdef _WIN32
        closesocket(handle_);
#else
        ::close(handle_);
#endif
        handle_ = INVALID_SOCKET_HANDLE;
    }
}

std::error_code Socket::get_last_socket_error() const {
#ifdef _WIN32
    int error = WSAGetLastError();
    return std::error_code(error, std::system_category());
#else
    return std::error_code(errno, std::system_category());
#endif
}

// Socket utility functions
namespace socket_utils {

Result<void> initialize_networking() {
#ifdef _WIN32
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        LOG_ERROR("WSAStartup failed: {}", result);
        return std::error_code(result, std::system_category());
    }
    LOG_DEBUG("Winsock initialized successfully");
#endif
    return Result<void>{};
}

void cleanup_networking() {
#ifdef _WIN32
    WSACleanup();
    LOG_DEBUG("Winsock cleaned up");
#endif
}

NetworkingRAII::NetworkingRAII() {
    auto result = initialize_networking();
    initialized_ = result.has_value();
    if (!initialized_) {
        LOG_ERROR("Failed to initialize networking");
    }
}

NetworkingRAII::~NetworkingRAII() {
    if (initialized_) {
        cleanup_networking();
    }
}

Result<std::vector<SocketAddress>> resolve_hostname(
    std::string_view hostname, Port port, SocketType type) {
    
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;  // Allow both IPv4 and IPv6
    hints.ai_socktype = static_cast<int>(type);
    
    addrinfo* result = nullptr;
    std::string hostname_str{hostname};
    std::string port_str = std::to_string(port);
    
    int status = getaddrinfo(hostname_str.c_str(), port_str.c_str(), &hints, &result);
    if (status != 0) {
        LOG_ERROR("getaddrinfo failed for {}: {}", hostname, gai_strerror(status));
        return make_error_code(NetworkError::InvalidAddress);
    }
    
    std::vector<SocketAddress> addresses;
    for (addrinfo* ptr = result; ptr != nullptr; ptr = ptr->ai_next) {
        if (ptr->ai_family == AF_INET) {
            auto* addr_in = reinterpret_cast<sockaddr_in*>(ptr->ai_addr);
            addresses.emplace_back(*addr_in);
        } else if (ptr->ai_family == AF_INET6) {
            auto* addr_in6 = reinterpret_cast<sockaddr_in6*>(ptr->ai_addr);
            addresses.emplace_back(*addr_in6);
        }
    }
    
    freeaddrinfo(result);
    LOG_DEBUG("Resolved {} to {} addresses", hostname, addresses.size());
    return addresses;
}

} // namespace socket_utils

} // namespace networkquests