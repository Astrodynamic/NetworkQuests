#include "networkquests/tcp.hpp"

#include <array>
#include <cstring>

#ifdef _WIN32
    #include <winsock2.h>
#else
    #include <arpa/inet.h>
#endif

namespace networkquests::tcp {

TcpConnection::TcpConnection(Socket socket) : socket_(std::move(socket)) {
    LOG_DEBUG("Created TCP connection");
}

TcpConnection::TcpConnection(TcpConnection&& other) noexcept 
    : socket_(std::move(other.socket_)), connected_(other.connected_.load()) {
    other.connected_.store(false);
}

TcpConnection& TcpConnection::operator=(TcpConnection&& other) noexcept {
    if (this != &other) {
        close();
        socket_ = std::move(other.socket_);
        connected_.store(other.connected_.load());
        other.connected_.store(false);
    }
    return *this;
}

TcpConnection::~TcpConnection() {
    close();
}

Result<void> TcpConnection::send_message(const Message& message) {
    if (!is_connected()) {
        return make_error_code(NetworkError::ConnectionFailed);
    }
    
    LOG_DEBUG("Sending message of {} bytes", message.size());
    return send_raw(std::span<const std::byte>(message.data));
}

Result<Message> TcpConnection::receive_message() {
    if (!is_connected()) {
        return make_error_code(NetworkError::ConnectionFailed);
    }
    
    auto data_result = receive_raw();
    if (!data_result) {
        return data_result.error();
    }
    
    Message message(std::move(data_result.value()));
    LOG_DEBUG("Received message of {} bytes", message.size());
    return message;
}

Result<std::size_t> TcpConnection::send(std::span<const std::byte> data) {
    if (!is_connected()) {
        return make_error_code(NetworkError::SendFailed);
    }
    
    return socket_.send(data);
}

Result<std::size_t> TcpConnection::receive(std::span<std::byte> buffer) {
    if (!is_connected()) {
        return make_error_code(NetworkError::ReceiveFailed);
    }
    
    return socket_.receive(buffer);
}

void TcpConnection::close() {
    if (connected_.exchange(false)) {
        socket_.close();
        LOG_DEBUG("TCP connection closed");
    }
}

bool TcpConnection::is_connected() const noexcept {
    return connected_.load() && socket_.is_valid();
}

Result<SocketAddress> TcpConnection::local_address() const {
    return socket_.local_address();
}

Result<SocketAddress> TcpConnection::remote_address() const {
    return socket_.remote_address();
}

Result<void> TcpConnection::send_raw(std::span<const std::byte> data) {
    // Send length prefix (32-bit big-endian)
    std::uint32_t length = static_cast<std::uint32_t>(data.size());
    std::uint32_t network_length = htonl(length);
    
    auto length_bytes = std::span<const std::byte>(
        reinterpret_cast<const std::byte*>(&network_length), 
        sizeof(network_length)
    );
    
    // Send length prefix
    auto length_result = socket_.send(length_bytes);
    if (!length_result) {
        connected_ = false;
        return length_result.error();
    }
    
    if (length_result.value() != sizeof(network_length)) {
        connected_ = false;
        return make_error_code(NetworkError::SendFailed);
    }
    
    // Send data
    if (!data.empty()) {
        std::size_t total_sent = 0;
        while (total_sent < data.size()) {
            auto remaining = data.subspan(total_sent);
            auto result = socket_.send(remaining);
            if (!result) {
                connected_ = false;
                return result.error();
            }
            
            total_sent += result.value();
            if (result.value() == 0) {
                // Connection closed by peer
                connected_ = false;
                return make_error_code(NetworkError::ConnectionFailed);
            }
        }
    }
    
    return Result<void>{};
}

Result<std::vector<std::byte>> TcpConnection::receive_raw() {
    // Receive length prefix
    std::array<std::byte, sizeof(std::uint32_t)> length_buffer;
    std::size_t total_received = 0;
    
    while (total_received < length_buffer.size()) {
        auto remaining = std::span<std::byte>(length_buffer).subspan(total_received);
        auto result = socket_.receive(remaining);
        if (!result) {
            connected_ = false;
            return result.error();
        }
        
        total_received += result.value();
        if (result.value() == 0) {
            // Connection closed by peer
            connected_ = false;
            return make_error_code(NetworkError::ConnectionFailed);
        }
    }
    
    // Extract length from network byte order
    std::uint32_t network_length;
    std::memcpy(&network_length, length_buffer.data(), sizeof(network_length));
    std::uint32_t length = ntohl(network_length);
    
    LOG_DEBUG("Expecting to receive {} bytes of data", length);
    
    // Sanity check for message length (max 10MB)
    constexpr std::uint32_t MAX_MESSAGE_SIZE = 10 * 1024 * 1024;
    if (length > MAX_MESSAGE_SIZE) {
        connected_ = false;
        LOG_ERROR("Message length {} exceeds maximum allowed size {}", length, MAX_MESSAGE_SIZE);
        return make_error_code(NetworkError::ProtocolError);
    }
    
    // Receive data
    std::vector<std::byte> data(length);
    if (length > 0) {
        total_received = 0;
        while (total_received < length) {
            auto remaining = std::span<std::byte>(data).subspan(total_received);
            auto result = socket_.receive(remaining);
            if (!result) {
                connected_ = false;
                return result.error();
            }
            
            total_received += result.value();
            if (result.value() == 0) {
                // Connection closed by peer
                connected_ = false;
                return make_error_code(NetworkError::ConnectionFailed);
            }
        }
    }
    
    return data;
}

} // namespace networkquests::tcp