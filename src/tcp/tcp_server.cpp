#include "networkquests/tcp.hpp"

namespace networkquests::tcp {

TcpServer::TcpServer(Port port, AddressFamily family) : family_(family) {
    LOG_DEBUG("Created TCP server for port {} and address family {}", port, static_cast<int>(family));
    
    server_socket_ = Socket(SocketType::TCP, family);
    
    if (reuse_address_) {
        auto result = server_socket_.set_reuse_address(true);
        if (!result) {
            LOG_WARNING("Failed to set SO_REUSEADDR: {}", result.error().message());
        }
    }
    
    SocketAddress bind_address("0.0.0.0", port, family);
    auto bind_result = bind(bind_address);
    if (!bind_result) {
        LOG_ERROR("Failed to bind to port {}: {}", port, bind_result.error().message());
        throw std::system_error(bind_result.error());
    }
}

TcpServer::~TcpServer() {
    stop();
}

Result<void> TcpServer::bind(const SocketAddress& address) {
    if (!server_socket_.is_valid()) {
        server_socket_ = Socket(SocketType::TCP, family_);
        
        if (reuse_address_) {
            auto result = server_socket_.set_reuse_address(true);
            if (!result) {
                LOG_WARNING("Failed to set SO_REUSEADDR: {}", result.error().message());
            }
        }
    }
    
    auto result = server_socket_.bind(address);
    if (!result) {
        LOG_ERROR("Failed to bind to {}: {}", address.to_string(), result.error().message());
        return result.error();
    }
    
    LOG_INFO("TCP server bound to {}", address.to_string());
    return Result<void>{};
}

Result<void> TcpServer::bind(Port port) {
    SocketAddress address("0.0.0.0", port, family_);
    return bind(address);
}

Result<void> TcpServer::listen(int backlog) {
    if (!server_socket_.is_valid()) {
        return make_error_code(NetworkError::ListenFailed);
    }
    
    auto result = server_socket_.listen(backlog);
    if (!result) {
        LOG_ERROR("Failed to listen: {}", result.error().message());
        return result.error();
    }
    
    LOG_INFO("TCP server listening with backlog {}", backlog);
    return Result<void>{};
}

Result<void> TcpServer::start(ConnectionHandler handler) {
    if (running_) {
        LOG_WARNING("TCP server is already running");
        return Result<void>{};
    }
    
    if (!server_socket_.is_valid()) {
        return make_error_code(NetworkError::ListenFailed);
    }
    
    connection_handler_ = std::move(handler);
    running_ = true;
    
    // Start accept thread
    accept_thread_ = std::make_unique<std::thread>(&TcpServer::accept_loop, this);
    
    LOG_INFO("TCP server started");
    return Result<void>{};
}

void TcpServer::stop() {
    if (running_.exchange(false)) {
        LOG_INFO("Stopping TCP server");
        
        // Close server socket to break accept loop
        server_socket_.close();
        
        // Wait for accept thread to finish
        if (accept_thread_ && accept_thread_->joinable()) {
            accept_thread_->join();
            accept_thread_.reset();
        }
        
        LOG_INFO("TCP server stopped");
    }
}

Result<TcpConnection> TcpServer::accept_connection() {
    if (!server_socket_.is_valid()) {
        return make_error_code(NetworkError::AcceptFailed);
    }
    
    auto socket_result = server_socket_.accept();
    if (!socket_result) {
        return socket_result.error();
    }
    
    return TcpConnection(std::move(socket_result.value()));
}

Result<SocketAddress> TcpServer::local_address() const {
    return server_socket_.local_address();
}

void TcpServer::accept_loop() {
    LOG_DEBUG("TCP server accept loop started");
    
    while (running_) {
        auto connection_result = accept_connection();
        if (!connection_result) {
            if (running_) {
                LOG_ERROR("Failed to accept connection: {}", connection_result.error().message());
                // Small delay to prevent busy loop on persistent errors
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            continue;
        }
        
        auto connection = std::move(connection_result.value());
        auto remote_addr = connection.remote_address();
        if (remote_addr) {
            LOG_INFO("Accepted connection from {}", remote_addr.value().to_string());
        }
        
        // Handle connection in a separate thread
        std::thread connection_thread(&TcpServer::handle_connection, this, std::move(connection));
        connection_thread.detach();
    }
    
    LOG_DEBUG("TCP server accept loop ended");
}

void TcpServer::handle_connection(TcpConnection connection) {
    try {
        if (connection_handler_) {
            connection_handler_(std::move(connection));
        } else {
            LOG_WARNING("No connection handler set, closing connection");
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Exception in connection handler: {}", e.what());
    } catch (...) {
        LOG_ERROR("Unknown exception in connection handler");
    }
}

} // namespace networkquests::tcp