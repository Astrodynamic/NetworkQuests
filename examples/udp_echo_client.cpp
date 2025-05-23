#include "networkquests/udp.hpp"
#include "networkquests/socket.hpp"
#include "networkquests/logger.hpp"
#include "networkquests/common.hpp"

#include <iostream>
#include <string>

using namespace networkquests;
using namespace networkquests::udp;

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
    std::string host = "localhost";
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
    
    std::cout << "Connecting to UDP server at " << host << ":" << port << "...\n";
    
    try {
        // Create UDP client
        UdpClient client(host, port);
        
        std::cout << "Connected! Type messages to send (empty line to quit):\n\n";
        
        std::string line;
        while (std::getline(std::cin, line)) {
            if (line.empty()) {
                break;
            }
            
            std::cout << "Sending: " << line << std::endl;
            
            // Send message and wait for echo
            auto response_result = client.send_and_receive_text(
                line, 
                4096, 
                std::chrono::seconds(5)
            );
            
            if (response_result) {
                std::string response = response_result.value();
                std::cout << "Echo: " << response << std::endl;
                
                // Verify it's the same message
                if (response == line) {
                    std::cout << "✓ Echo verified!" << std::endl;
                } else {
                    std::cout << "⚠ Echo mismatch!" << std::endl;
                }
            } else {
                std::cerr << "Failed to receive echo: " 
                          << response_result.error().message() << std::endl;
            }
            
            std::cout << std::endl;
        }
        
        std::cout << "Goodbye!\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Client error: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}