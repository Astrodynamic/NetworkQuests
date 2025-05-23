#include "networkquests/udp.hpp"
#include <stdexcept>
#include <cstring>

namespace networkquests::udp {

UdpSocket::UdpSocket() 
    : socket_(std::make_unique<Socket>(SocketType::UDP)) {
    if (!socket_->is_valid()) {
        throw std::runtime_error("Failed to create UDP socket");
    }
}

UdpSocket::UdpSocket(const SocketAddress& bind_addr) 
    : UdpSocket() {
    auto result = bind(bind_addr);
    if (!result) {
        throw std::runtime_error("Failed to bind UDP socket: " + result.error().message());
    }
}

UdpSocket::UdpSocket(Port port) 
    : UdpSocket() {
    auto result = bind(port);
    if (!result) {
        throw std::runtime_error("Failed to bind UDP socket to port " + std::to_string(port) + ": " + result.error().message());
    }
}

UdpSocket::UdpSocket(UdpSocket&& other) noexcept 
    : socket_(std::move(other.socket_))
    , bound_address_(std::move(other.bound_address_)) {
}

UdpSocket& UdpSocket::operator=(UdpSocket&& other) noexcept {
    if (this != &other) {
        std::lock_guard<std::mutex> lock1(mutex_);
        std::lock_guard<std::mutex> lock2(other.mutex_);
        
        socket_ = std::move(other.socket_);
        bound_address_ = std::move(other.bound_address_);
    }
    return *this;
}

Result<void> UdpSocket::bind(const SocketAddress& addr) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!socket_ || !socket_->is_valid()) {
        return make_error_result<void>(NetworkError::SocketError);
    }
    
    auto result = socket_->bind(addr);
    if (result) {
        bound_address_ = addr;
        LOG_DEBUG("UDP socket bound to {}", addr.to_string());
    } else {
        LOG_ERROR("Failed to bind UDP socket to {}: {}", addr.to_string(), result.error().message());
    }
    
    return result;
}

Result<void> UdpSocket::bind(Port port) {
    // Try IPv4 first, then IPv6
    auto ipv4_addr = SocketAddress::from_ipv4("0.0.0.0", port);
    if (ipv4_addr) {
        auto result = bind(ipv4_addr.value());
        if (result) {
            return result;
        }
    }
    
    auto ipv6_addr = SocketAddress::from_ipv6("::", port);
    if (ipv6_addr) {
        return bind(ipv6_addr.value());
    }
    
    return make_error_result<void>(NetworkError::AddressResolutionFailed);
}

Result<size_t> UdpSocket::send_to(std::span<const uint8_t> data, const SocketAddress& target) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!socket_ || !socket_->is_valid()) {
        return make_error_result<size_t>(NetworkError::SocketError);
    }
    
    LOG_DEBUG("Sending {} bytes to {}", data.size(), target.to_string());
    
    // Convert uint8_t span to std::byte span
    auto byte_data = std::span<const std::byte>(
        reinterpret_cast<const std::byte*>(data.data()), 
        data.size()
    );
    
    auto result = socket_->send_to(byte_data, target);
    if (result) {
        LOG_DEBUG("Sent {} bytes to {}", result.value(), target.to_string());
    } else {
        LOG_ERROR("Failed to send to {}: {}", target.to_string(), result.error().message());
    }
    
    return result;
}

Result<size_t> UdpSocket::send_to(std::string_view text, const SocketAddress& target) {
    auto data = std::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(text.data()), 
        text.size()
    );
    return send_to(data, target);
}

Result<size_t> UdpSocket::send_to(const UdpDatagram& datagram) {
    return send_to(std::span<const uint8_t>(datagram.data), datagram.sender);
}

Result<UdpDatagram> UdpSocket::receive_from(size_t max_size, std::optional<std::chrono::milliseconds> timeout) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!socket_ || !socket_->is_valid()) {
        return make_error_result<UdpDatagram>(NetworkError::SocketError);
    }
    
    // Set timeout if specified
    if (timeout) {
        auto timeout_result = socket_->set_receive_timeout(timeout.value());
        if (!timeout_result) {
            LOG_ERROR("Failed to set receive timeout: {}", timeout_result.error().message());
        }
    }
    
    LOG_DEBUG("Receiving UDP datagram (max {} bytes)", max_size);
    
    std::vector<std::byte> buffer(max_size);
    auto result = socket_->receive_from(std::span<std::byte>(buffer));
    
    if (!result) {
        LOG_ERROR("Failed to receive UDP datagram: {}", result.error().message());
        return make_error_result<UdpDatagram>(result.error());
    }
    
    auto [bytes_received, sender] = result.value();
    
    // Convert std::byte vector to uint8_t vector
    std::vector<uint8_t> data(bytes_received);
    std::memcpy(data.data(), buffer.data(), bytes_received);
    
    LOG_DEBUG("Received {} bytes from {}", bytes_received, sender.to_string());
    
    return Result<UdpDatagram>::success(UdpDatagram{std::move(data), sender});
}

bool UdpSocket::is_bound() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return bound_address_.has_value();
}

std::optional<SocketAddress> UdpSocket::local_address() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!socket_ || !socket_->is_valid()) {
        return std::nullopt;
    }
    
    auto result = socket_->local_address();
    if (result) {
        return result.value();
    }
    
    return bound_address_;
}

Result<void> UdpSocket::enable_broadcast() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!socket_ || !socket_->is_valid()) {
        return make_error_result<void>(NetworkError::SocketError);
    }
    
    auto result = socket_->set_broadcast(true);
    if (result) {
        LOG_DEBUG("Enabled broadcast for UDP socket");
    } else {
        LOG_ERROR("Failed to enable broadcast: {}", result.error().message());
    }
    
    return result;
}

Result<void> UdpSocket::set_non_blocking(bool non_blocking) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!socket_ || !socket_->is_valid()) {
        return make_error_result<void>(NetworkError::SocketError);
    }
    
    return socket_->set_non_blocking(non_blocking);
}

Result<void> UdpSocket::set_receive_timeout(std::chrono::milliseconds timeout) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!socket_ || !socket_->is_valid()) {
        return make_error_result<void>(NetworkError::SocketError);
    }
    
    return socket_->set_receive_timeout(timeout);
}

Result<void> UdpSocket::set_send_timeout(std::chrono::milliseconds timeout) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!socket_ || !socket_->is_valid()) {
        return make_error_result<void>(NetworkError::SocketError);
    }
    
    return socket_->set_send_timeout(timeout);
}

SocketHandle UdpSocket::handle() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!socket_) {
        return INVALID_SOCKET_HANDLE;
    }
    
    return socket_->handle();
}

} // namespace networkquests::udp