#include "networkquests/udp.hpp"

namespace networkquests::udp {

UdpClient::UdpClient(const SocketAddress& server_addr)
    : socket_(), server_address_(server_addr) {
    LOG_DEBUG("Created UDP client for server {}", server_addr.to_string());
}

UdpClient::UdpClient(std::string_view host, Port port) : socket_() {
    auto addr_result = SocketAddress::resolve(std::string(host), port);
    if (!addr_result) {
        throw std::runtime_error("Failed to resolve server address " + std::string(host) + ":" + std::to_string(port));
    }
    
    server_address_ = addr_result.value();
    LOG_DEBUG("Created UDP client for server {}:{} (resolved to {})", 
              host, port, server_address_.to_string());
}

Result<size_t> UdpClient::send(std::span<const uint8_t> data) {
    LOG_DEBUG("Sending {} bytes to server {}", data.size(), server_address_.to_string());
    return socket_.send_to(data, server_address_);
}

Result<size_t> UdpClient::send(std::string_view text) {
    auto data = std::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(text.data()), 
        text.size()
    );
    return send(data);
}

Result<std::vector<uint8_t>> UdpClient::receive(size_t max_size, std::optional<std::chrono::milliseconds> timeout) {
    auto datagram_result = socket_.receive_from(max_size, timeout);
    if (!datagram_result) {
        return make_error_result<std::vector<uint8_t>>(datagram_result.error());
    }
    
    auto datagram = datagram_result.value();
    
    // Verify that the response came from our server
    if (datagram.sender.to_string() != server_address_.to_string()) {
        LOG_WARNING("Received response from unexpected sender: {} (expected {})", 
                   datagram.sender.to_string(), server_address_.to_string());
    }
    
    LOG_DEBUG("Received {} bytes from server", datagram.data.size());
    return Result<std::vector<uint8_t>>::success(std::move(datagram.data));
}

Result<std::vector<uint8_t>> UdpClient::send_and_receive(
    std::span<const uint8_t> data,
    size_t max_response_size,
    std::optional<std::chrono::milliseconds> timeout
) {
    // Send data
    auto send_result = send(data);
    if (!send_result) {
        return make_error_result<std::vector<uint8_t>>(send_result.error());
    }
    
    // Receive response
    return receive(max_response_size, timeout);
}

Result<std::string> UdpClient::send_and_receive_text(
    std::string_view text,
    size_t max_response_size,
    std::optional<std::chrono::milliseconds> timeout
) {
    auto data = std::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(text.data()), 
        text.size()
    );
    
    auto response_result = send_and_receive(data, max_response_size, timeout);
    if (!response_result) {
        return make_error_result<std::string>(response_result.error());
    }
    
    auto response_data = response_result.value();
    std::string response(response_data.begin(), response_data.end());
    
    LOG_DEBUG("Received text response: {}", response);
    return Result<std::string>::success(std::move(response));
}

std::optional<SocketAddress> UdpClient::local_address() const {
    return socket_.local_address();
}

} // namespace networkquests::udp