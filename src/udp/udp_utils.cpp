#include "networkquests/udp.hpp"
#include <thread>

namespace networkquests::udp::udp_utils {

Result<std::string> send_udp_message(
    std::string_view host,
    Port port,
    std::string_view message,
    std::optional<std::chrono::milliseconds> timeout
) {
    LOG_DEBUG("Sending UDP message to {}:{}: {}", host, port, message);
    
    try {
        UdpClient client(host, port);
        return client.send_and_receive_text(message, 4096, timeout);
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to send UDP message to {}:{}: {}", host, port, e.what());
        return make_error_result<std::string>(NetworkError::ConnectionFailed);
    }
}

void run_echo_server(Port port, std::atomic<bool>& should_stop) {
    LOG_INFO("Starting UDP echo server on port {}", port);
    
    try {
        UdpServer server(port);
        
        auto handler = [](UdpDatagram datagram, UdpSocket& socket) {
            std::string message = datagram.to_string();
            LOG_INFO("Echo server received from {}: {}", 
                    datagram.sender.to_string(), message);
            
            // Echo the message back
            auto result = socket.send_to(datagram);
            if (!result) {
                LOG_ERROR("Failed to echo message to {}: {}", 
                         datagram.sender.to_string(), result.error().message());
            } else {
                LOG_DEBUG("Echoed {} bytes back to {}", 
                         datagram.size(), datagram.sender.to_string());
            }
        };
        
        // Start server in a loop that checks for stop condition
        while (!should_stop) {
            auto start_result = server.start(handler);
            if (!start_result) {
                LOG_ERROR("UDP echo server failed: {}", start_result.error().message());
                break;
            }
            
            // Small delay to prevent busy waiting
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        server.stop();
        LOG_INFO("UDP echo server stopped");
        
    } catch (const std::exception& e) {
        LOG_ERROR("UDP echo server error: {}", e.what());
    }
}

Result<void> broadcast_message(Port port, std::string_view message) {
    LOG_INFO("Broadcasting message on port {}: {}", port, message);
    
    try {
        UdpSocket socket;
        
        // Enable broadcasting
        auto broadcast_result = socket.enable_broadcast();
        if (!broadcast_result) {
            LOG_ERROR("Failed to enable broadcast: {}", broadcast_result.error().message());
            return broadcast_result;
        }
        
        // Create broadcast address
        auto broadcast_addr = SocketAddress::from_ipv4("255.255.255.255", port);
        if (!broadcast_addr) {
            LOG_ERROR("Failed to create broadcast address");
            return make_error_result<void>(NetworkError::AddressResolutionFailed);
        }
        
        // Send the broadcast message
        auto send_result = socket.send_to(message, broadcast_addr.value());
        if (!send_result) {
            LOG_ERROR("Failed to send broadcast message: {}", send_result.error().message());
            return make_error_result<void>(send_result.error());
        }
        
        LOG_INFO("Broadcast message sent successfully");
        return Result<void>::success();
        
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to broadcast message: {}", e.what());
        return make_error_result<void>(NetworkError::SocketError);
    }
}

Result<std::vector<UdpDatagram>> listen_for_broadcasts(
    Port port,
    std::chrono::milliseconds duration,
    size_t max_messages
) {
    LOG_INFO("Listening for broadcasts on port {} for {}ms", port, duration.count());
    
    try {
        UdpSocket socket(port);
        
        std::vector<UdpDatagram> messages;
        messages.reserve(max_messages);
        
        auto start_time = std::chrono::steady_clock::now();
        auto end_time = start_time + duration;
        
        while (std::chrono::steady_clock::now() < end_time && messages.size() < max_messages) {
            auto remaining_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                end_time - std::chrono::steady_clock::now()
            );
            
            if (remaining_time <= std::chrono::milliseconds(0)) {
                break;
            }
            
            // Receive with timeout
            auto datagram_result = socket.receive_from(4096, remaining_time);
            
            if (datagram_result) {
                auto datagram = datagram_result.value();
                LOG_INFO("Received broadcast from {}: {}", 
                        datagram.sender.to_string(), datagram.to_string());
                messages.push_back(std::move(datagram));
            } else {
                auto error = datagram_result.error();
                // Timeouts are expected, but log other errors
                if (error != std::errc::timed_out && error != std::errc::operation_would_block) {
                    LOG_WARNING("Error receiving broadcast: {}", error.message());
                }
            }
        }
        
        LOG_INFO("Finished listening for broadcasts, received {} messages", messages.size());
        return Result<std::vector<UdpDatagram>>::success(std::move(messages));
        
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to listen for broadcasts: {}", e.what());
        return make_error_result<std::vector<UdpDatagram>>(NetworkError::SocketError);
    }
}

} // namespace networkquests::udp::udp_utils