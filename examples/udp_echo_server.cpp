#include "networkquests/udp.hpp"
#include "networkquests/socket.hpp"
#include "networkquests/logger.hpp"
#include "networkquests/common.hpp"

#include <iostream>
#include <signal.h>
#include <thread>

using namespace networkquests;
using namespace networkquests::udp;

std::atomic<bool> should_stop{false};

void signal_handler(int) {
    should_stop = true;
    std::cout << "\nReceived signal, shutting down...\n";
}

void handle_datagram(UdpDatagram datagram, UdpSocket& socket) {
    std::string message = datagram.to_string();
    std::string client_info = datagram.sender.to_string();
    
    LOG_INFO("Received from {}: {}", client_info, message);
    std::cout << "From " << client_info << ": " << message << std::endl;
    
    // Echo the message back to the sender
    auto result = socket.send_to(datagram);
    if (!result) {
        LOG_ERROR("Failed to echo message to {}: {}", 
                 client_info, result.error().message());
        std::cerr << "Failed to echo to " << client_info << ": " 
                  << result.error().message() << std::endl;
    } else {
        LOG_DEBUG("Echoed {} bytes back to {}", datagram.size(), client_info);
    }
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
        // Create UDP server
        UdpServer server(port);
        
        auto local_addr = server.local_address();
        if (local_addr) {
            std::cout << "UDP Echo Server listening on " << local_addr.value().to_string() << "\n";
        } else {
            std::cout << "UDP Echo Server listening on port " << port << "\n";
        }
        
        std::cout << "Press Ctrl+C to stop the server\n\n";
        
        // Start server with datagram handler in a separate thread
        std::thread server_thread([&server]() {
            auto start_result = server.start(handle_datagram);
            if (!start_result) {
                std::cerr << "Failed to start UDP server: " << start_result.error().message() << "\n";
            }
        });
        
        // Wait for shutdown signal
        while (!should_stop) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        std::cout << "Shutting down server...\n";
        server.stop();
        
        // Wait for server thread to finish
        if (server_thread.joinable()) {
            server_thread.join();
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Server error: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}