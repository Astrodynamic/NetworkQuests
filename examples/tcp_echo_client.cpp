#include "networkquests/tcp.hpp"
#include "networkquests/socket.hpp"
#include "networkquests/logger.hpp"
#include "networkquests/common.hpp"

#include <iostream>
#include <string>

using namespace networkquests;
using namespace networkquests::tcp;

int main(int argc, char* argv[]) {
    // Initialize networking
    socket_utils::NetworkingRAII networking;
    if (!networking.is_initialized()) {
        std::cerr << "Failed to initialize networking\n";
        return 1;
    }
    
    // Set logging level
    Logger::instance().set_level(LogLevel::Info);
    
    // Parse command line arguments
    std::string host = "127.0.0.1";
    Port port = 8080;
    
    if (argc > 1) {
        host = argv[1];
    }
    if (argc > 2) {
        try {
            port = static_cast<Port>(std::stoi(argv[2]));
        } catch (const std::exception& e) {
            std::cerr << "Invalid port number: " << argv[2] << "\n";
            return 1;
        }
    }
    
    try {
        // Create TCP client
        TcpClient client;
        
        std::cout << "Connecting to " << host << ":" << port << "...\n";
        
        // Connect to server
        auto connection_result = client.connect(host, port);
        if (!connection_result) {
            std::cerr << "Failed to connect to server: " << connection_result.error().message() << "\n";
            return 1;
        }
        
        auto connection = std::move(connection_result.value());
        
        auto local_addr = connection.local_address();
        auto remote_addr = connection.remote_address();
        
        std::cout << "Connected successfully!\n";
        if (local_addr && remote_addr) {
            std::cout << "Local address: " << local_addr.value().to_string() << "\n";
            std::cout << "Remote address: " << remote_addr.value().to_string() << "\n";
        }
        
        std::cout << "\nEnter messages to send to the echo server (type 'quit' to exit):\n";
        
        std::string input;
        while (std::getline(std::cin, input)) {
            if (input == "quit" || input == "exit") {
                break;
            }
            
            if (input.empty()) {
                continue;
            }
            
            // Send message to server
            Message message(input);
            auto send_result = connection.send_message(message);
            if (!send_result) {
                std::cerr << "Failed to send message: " << send_result.error().message() << "\n";
                break;
            }
            
            std::cout << "Sent: " << input << "\n";
            
            // Receive echo response
            auto response_result = connection.receive_message();
            if (!response_result) {
                std::cerr << "Failed to receive response: " << response_result.error().message() << "\n";
                break;
            }
            
            auto response = response_result.value();
            std::cout << "Echo: " << response.to_string() << "\n\n";
        }
        
        std::cout << "Disconnecting...\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Client error: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}