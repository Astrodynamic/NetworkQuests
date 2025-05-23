#include "networkquests/udp.hpp"
#include <thread>

namespace networkquests::udp {

UdpServer::UdpServer(const SocketAddress& listen_addr)
    : socket_(listen_addr), listen_address_(listen_addr) {
    LOG_INFO("Created UDP server listening on {}", listen_addr.to_string());
}

UdpServer::UdpServer(Port port) : socket_(port) {
    auto local_addr = socket_.local_address();
    if (local_addr) {
        listen_address_ = local_addr.value();
        LOG_INFO("Created UDP server listening on {}", listen_address_.to_string());
    } else {
        // Fallback - create a generic address
        auto addr = SocketAddress::from_ipv4("0.0.0.0", port);
        if (addr) {
            listen_address_ = addr.value();
        } else {
            throw std::runtime_error("Failed to create UDP server address for port " + std::to_string(port));
        }
        LOG_INFO("Created UDP server listening on port {}", port);
    }
}

Result<void> UdpServer::start(DatagramHandler handler, size_t max_datagram_size) {
    if (!handler) {
        return make_error_result<void>(NetworkError::InvalidArgument);
    }
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (running_) {
            return make_error_result<void>(NetworkError::AlreadyConnected);
        }
        running_ = true;
    }
    
    LOG_INFO("Starting UDP server on {}", listen_address_.to_string());
    
    try {
        while (running_) {
            // Receive datagram
            auto datagram_result = socket_.receive_from(max_datagram_size, std::chrono::milliseconds(100));
            
            if (!datagram_result) {
                auto error = datagram_result.error();
                
                // Check if it's just a timeout (expected when no data)
                if (error == std::errc::timed_out || error == std::errc::operation_would_block) {
                    continue;
                }
                
                LOG_ERROR("Failed to receive UDP datagram: {}", error.message());
                
                // For other errors, continue but log them
                continue;
            }
            
            auto datagram = datagram_result.value();
            
            LOG_DEBUG("Received datagram from {}: {} bytes", 
                     datagram.sender.to_string(), datagram.size());
            
            // Handle the datagram in a separate thread to avoid blocking
            std::thread handler_thread([handler, datagram = std::move(datagram), &socket = socket_]() mutable {
                try {
                    handler(std::move(datagram), socket);
                } catch (const std::exception& e) {
                    LOG_ERROR("Exception in datagram handler: {}", e.what());
                } catch (...) {
                    LOG_ERROR("Unknown exception in datagram handler");
                }
            });
            
            handler_thread.detach(); // Let the handler run independently
        }
        
        LOG_INFO("UDP server stopped");
        return Result<void>::success();
        
    } catch (const std::exception& e) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            running_ = false;
        }
        
        LOG_ERROR("UDP server error: {}", e.what());
        return make_error_result<void>(NetworkError::SocketError);
    }
}

void UdpServer::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_) {
            return;
        }
        running_ = false;
    }
    
    LOG_INFO("Stopping UDP server");
}

bool UdpServer::is_running() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return running_;
}

std::optional<SocketAddress> UdpServer::local_address() const {
    return socket_.local_address();
}

Result<void> UdpServer::enable_broadcast() {
    return socket_.enable_broadcast();
}

} // namespace networkquests::udp