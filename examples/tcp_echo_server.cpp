#include "networkquests/tcp.hpp"
#include "networkquests/socket.hpp"
#include "networkquests/logger.hpp"
#include "networkquests/common.hpp"

#include <iostream>
#include <signal.h>

using namespace networkquests;
using namespace networkquests::tcp;

std::atomic<bool> should_stop{false};

void signal_handler(int) {
    should_stop = true;
    std::cout << "\nReceived signal, shutting down...\n";
}

void handle_client_connection(TcpConnection connection) {
    auto remote_addr = connection.remote_address();
    std::string client_info = remote_addr ? remote_addr.value().to_string() : "unknown";
    
    LOG_INFO("Handling connection from {}", client_info);
    
    try {
        while (connection.is_connected() && !should_stop) {
            // Receive message from client
            auto message_result = connection.receive_message();
            if (!message_result) {
                if (message_result.error() == make_error_code(NetworkError::ConnectionFailed)) {
                    LOG_INFO("Client {} disconnected", client_info);
                } else {
                    LOG_ERROR("Failed to receive message from {}: {}", 
                             client_info, message_result.error().message());
                }
                break;
            }
            
            auto message = message_result.value();
            std::string message_text = message.to_string();
            
            LOG_INFO("Received from {}: {}", client_info, message_text);
            
            // Echo the message back to client
            auto echo_result = connection.send_message(message);
            if (!echo_result) {
                LOG_ERROR("Failed to echo message to {}: {}", 
                         client_info, echo_result.error().message());
                break;
            }
            
            LOG_DEBUG("Echoed message back to {}", client_info);
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Exception handling client {}: {}", client_info, e.what());
    }
    
    LOG_INFO("Connection handler for {} finished", client_info);
}

int main(int argc, char* argv[]) {
    // Set up signal handling
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Initialize networking
    socket_utils::NetworkingRAII networking;
    if (!networking.is_initialized()) {
        std::cerr << "Failed to initialize networking\n";
        return 1;
    }
    
    // Set logging level
    Logger::instance().set_level(LogLevel::Info);
    
    // Parse command line arguments
    Port port = 8080;
    if (argc > 1) {
        try {
            port = static_cast<Port>(std::stoi(argv[1]));
        } catch (const std::exception& e) {
            std::cerr << "Invalid port number: " << argv[1] << "\n";
            return 1;
        }
    }
    
    try {
        // Create TCP server
        TcpServer server(port);
        
        auto local_addr = server.local_address();
        if (local_addr) {
            std::cout << "TCP Echo Server listening on " << local_addr.value().to_string() << "\n";
        } else {
            std::cout << "TCP Echo Server listening on port " << port << "\n";
        }
        
        std::cout << "Press Ctrl+C to stop the server\n\n";
        
        // Start server with connection handler
        auto start_result = server.listen();
        if (!start_result) {
            std::cerr << "Failed to start listening: " << start_result.error().message() << "\n";
            return 1;
        }
        
        auto run_result = server.start(handle_client_connection);
        if (!run_result) {
            std::cerr << "Failed to start server: " << run_result.error().message() << "\n";
            return 1;
        }
        
        // Wait for shutdown signal
        while (!should_stop) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        std::cout << "Shutting down server...\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Server error: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}