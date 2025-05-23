#include "networkquests/tcp.hpp"

namespace networkquests::tcp {

TcpClient::TcpClient(AddressFamily family) : family_(family) {
    LOG_DEBUG("Created TCP client for address family {}", static_cast<int>(family));
}

Result<TcpConnection> TcpClient::connect(const SocketAddress& server_address) {
    LOG_INFO("Connecting to TCP server at {}", server_address.to_string());
    
    Socket socket(SocketType::TCP, family_);
    
    // Set timeout
    auto timeout_result = socket.set_timeout(timeout_);
    if (!timeout_result) {
        LOG_WARNING("Failed to set socket timeout: {}", timeout_result.error().message());
        // Continue anyway, timeout is not critical
    }
    
    // Connect to server
    auto connect_result = socket.connect(server_address);
    if (!connect_result) {
        LOG_ERROR("Failed to connect to {}: {}", server_address.to_string(), 
                  connect_result.error().message());
        return connect_result.error();
    }
    
    LOG_INFO("Successfully connected to {}", server_address.to_string());
    return TcpConnection(std::move(socket));
}

Result<TcpConnection> TcpClient::connect(std::string_view host, Port port) {
    LOG_DEBUG("Resolving hostname: {}", host);
    
    // Try to resolve hostname
    auto resolve_result = socket_utils::resolve_hostname(host, port, SocketType::TCP);
    if (!resolve_result) {
        LOG_ERROR("Failed to resolve hostname {}: {}", host, resolve_result.error().message());
        return resolve_result.error();
    }
    
    const auto& addresses = resolve_result.value();
    if (addresses.empty()) {
        LOG_ERROR("No addresses found for hostname {}", host);
        return make_error_code(NetworkError::InvalidAddress);
    }
    
    // Try to connect to each resolved address
    std::error_code last_error;
    for (const auto& address : addresses) {
        LOG_DEBUG("Trying to connect to {}", address.to_string());
        
        auto result = connect(address);
        if (result) {
            return result;
        }
        
        last_error = result.error();
        LOG_DEBUG("Connection to {} failed: {}", address.to_string(), last_error.message());
    }
    
    LOG_ERROR("Failed to connect to any address for {}", host);
    return last_error;
}

Result<std::string> TcpClient::send_request(const SocketAddress& server_address, 
                                           std::string_view request) {
    auto connection_result = connect(server_address);
    if (!connection_result) {
        return connection_result.error();
    }
    
    auto connection = std::move(connection_result.value());
    
    // Send request
    Message request_message(request);
    auto send_result = connection.send_message(request_message);
    if (!send_result) {
        LOG_ERROR("Failed to send request: {}", send_result.error().message());
        return send_result.error();
    }
    
    // Receive response
    auto response_result = connection.receive_message();
    if (!response_result) {
        LOG_ERROR("Failed to receive response: {}", response_result.error().message());
        return response_result.error();
    }
    
    return response_result.value().to_string();
}

Result<std::string> TcpClient::send_request(std::string_view host, Port port,
                                           std::string_view request) {
    auto connection_result = connect(host, port);
    if (!connection_result) {
        return connection_result.error();
    }
    
    auto connection = std::move(connection_result.value());
    
    // Send request
    Message request_message(request);
    auto send_result = connection.send_message(request_message);
    if (!send_result) {
        LOG_ERROR("Failed to send request: {}", send_result.error().message());
        return send_result.error();
    }
    
    // Receive response
    auto response_result = connection.receive_message();
    if (!response_result) {
        LOG_ERROR("Failed to receive response: {}", response_result.error().message());
        return response_result.error();
    }
    
    return response_result.value().to_string();
}

} // namespace networkquests::tcp