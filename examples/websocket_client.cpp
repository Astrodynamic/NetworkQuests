#include "networkquests/websocket.hpp"
#include "networkquests/logger.hpp"

#include <iostream>
#include <sstream>
#include <thread>
#include <chrono>
#include <atomic>

using namespace networkquests;
using namespace networkquests::websocket;

class InteractiveWebSocketClient {
public:
    InteractiveWebSocketClient() : logger_("websocket_client") {}
    
    void run() {
        logger_.info("🌐 NetworkQuests WebSocket Client");
        logger_.info("==================================");
        
        show_help();
        
        while (running_) {
            std::cout << "\n> ";
            std::string command;
            std::getline(std::cin, command);
            
            if (command.empty()) continue;
            
            auto parts = split_command(command);
            if (parts.empty()) continue;
            
            handle_command(parts);
        }
    }
    
private:
    Logger logger_;
    std::atomic<bool> running_{true};
    std::unique_ptr<WebSocketConnection> connection_;
    
    void show_help() {
        std::cout << "\n📋 Available Commands:\n";
        std::cout << "  connect <url>           - Connect to WebSocket server\n";
        std::cout << "  send <message>          - Send text message\n";
        std::cout << "  binary <hex_data>       - Send binary message\n";
        std::cout << "  ping [data]             - Send ping frame\n";
        std::cout << "  close [code] [reason]   - Close connection\n";
        std::cout << "  status                  - Show connection status\n";
        std::cout << "  async [on|off]          - Toggle async receive mode\n";
        std::cout << "  test <url>              - Run connection test\n";
        std::cout << "  benchmark <url> <count> - Run performance benchmark\n";
        std::cout << "  help                    - Show this help\n";
        std::cout << "  quit                    - Exit application\n";
        std::cout << "\n📖 Examples:\n";
        std::cout << "  connect ws://echo.websocket.org\n";
        std::cout << "  connect ws://localhost:8080/chat\n";
        std::cout << "  send Hello, WebSocket!\n";
        std::cout << "  binary 48656C6C6F\n";
        std::cout << "  close 1000 Goodbye\n";
    }
    
    std::vector<std::string> split_command(const std::string& command) {
        std::vector<std::string> parts;
        std::istringstream iss(command);
        std::string part;
        
        while (iss >> part) {
            parts.push_back(part);
        }
        
        return parts;
    }
    
    void handle_command(const std::vector<std::string>& parts) {
        const std::string& cmd = parts[0];
        
        try {
            if (cmd == "connect") {
                handle_connect(parts);
            } else if (cmd == "send") {
                handle_send(parts);
            } else if (cmd == "binary") {
                handle_binary(parts);
            } else if (cmd == "ping") {
                handle_ping(parts);
            } else if (cmd == "close") {
                handle_close(parts);
            } else if (cmd == "status") {
                handle_status();
            } else if (cmd == "async") {
                handle_async(parts);
            } else if (cmd == "test") {
                handle_test(parts);
            } else if (cmd == "benchmark") {
                handle_benchmark(parts);
            } else if (cmd == "help") {
                show_help();
            } else if (cmd == "quit" || cmd == "exit") {
                handle_quit();
            } else {
                logger_.warn("Unknown command: {}", cmd);
                std::cout << "Type 'help' for available commands.\n";
            }
        } catch (const std::exception& e) {
            logger_.error("Command error: {}", e.what());
        }
    }
    
    void handle_connect(const std::vector<std::string>& parts) {
        if (parts.size() < 2) {
            logger_.error("Usage: connect <url>");
            return;
        }
        
        if (connection_ && connection_->is_connected()) {
            logger_.warn("Already connected. Close current connection first.");
            return;
        }
        
        const std::string& url = parts[1];
        logger_.info("🔗 Connecting to {}", url);
        
        WebSocketClient client;
        client.set_timeout(std::chrono::seconds(10));
        
        auto result = client.connect(url);
        if (!result) {
            logger_.error("❌ Connection failed: {}", result.error().message);
            return;
        }
        
        connection_ = std::move(result.value());
        logger_.info("✅ Connected successfully!");
        logger_.info("   Local:  {}", connection_->local_address().to_string());
        logger_.info("   Remote: {}", connection_->remote_address().to_string());
        
        // Set up event handlers
        setup_connection_handlers();
    }
    
    void handle_send(const std::vector<std::string>& parts) {
        if (!check_connection()) return;
        
        if (parts.size() < 2) {
            logger_.error("Usage: send <message>");
            return;
        }
        
        // Join all parts except the first (command)
        std::string message;
        for (size_t i = 1; i < parts.size(); ++i) {
            if (i > 1) message += " ";
            message += parts[i];
        }
        
        auto result = connection_->send_text(message);
        if (result) {
            logger_.info("📤 Sent: {}", message);
        } else {
            logger_.error("❌ Send failed: {}", result.error().message);
        }
    }
    
    void handle_binary(const std::vector<std::string>& parts) {
        if (!check_connection()) return;
        
        if (parts.size() < 2) {
            logger_.error("Usage: binary <hex_data>");
            return;
        }
        
        const std::string& hex_str = parts[1];
        auto binary_data = hex_to_bytes(hex_str);
        if (binary_data.empty() && !hex_str.empty()) {
            logger_.error("Invalid hex data: {}", hex_str);
            return;
        }
        
        auto result = connection_->send_binary(binary_data);
        if (result) {
            logger_.info("📤 Sent binary: {} bytes", binary_data.size());
        } else {
            logger_.error("❌ Send failed: {}", result.error().message);
        }
    }
    
    void handle_ping(const std::vector<std::string>& parts) {
        if (!check_connection()) return;
        
        std::string data;
        if (parts.size() > 1) {
            for (size_t i = 1; i < parts.size(); ++i) {
                if (i > 1) data += " ";
                data += parts[i];
            }
        }
        
        auto result = connection_->send_ping(data);
        if (result) {
            logger_.info("🏓 Ping sent: {}", data.empty() ? "(empty)" : data);
        } else {
            logger_.error("❌ Ping failed: {}", result.error().message);
        }
    }
    
    void handle_close(const std::vector<std::string>& parts) {
        if (!connection_) {
            logger_.warn("No connection to close");
            return;
        }
        
        WebSocketCloseCode code = WebSocketCloseCode::Normal;
        std::string reason;
        
        if (parts.size() > 1) {
            try {
                int code_int = std::stoi(parts[1]);
                code = static_cast<WebSocketCloseCode>(code_int);
            } catch (const std::exception&) {
                logger_.error("Invalid close code: {}", parts[1]);
                return;
            }
        }
        
        if (parts.size() > 2) {
            for (size_t i = 2; i < parts.size(); ++i) {
                if (i > 2) reason += " ";
                reason += parts[i];
            }
        }
        
        logger_.info("🔒 Closing connection (code: {}, reason: {})", 
                    static_cast<int>(code), reason.empty() ? "(none)" : reason);
        
        auto result = connection_->close(code, reason);
        if (!result) {
            logger_.error("❌ Close failed: {}", result.error().message);
        }
        
        connection_.reset();
    }
    
    void handle_status() {
        if (!connection_) {
            logger_.info("📊 Status: Not connected");
            return;
        }
        
        logger_.info("📊 Connection Status:");
        logger_.info("   State: {}", websocket_utils::state_to_string(connection_->state()));
        logger_.info("   Type:  {}", connection_->is_client() ? "Client" : "Server");
        logger_.info("   Local:  {}", connection_->local_address().to_string());
        logger_.info("   Remote: {}", connection_->remote_address().to_string());
    }
    
    void handle_async(const std::vector<std::string>& parts) {
        if (!check_connection()) return;
        
        if (parts.size() < 2) {
            logger_.error("Usage: async <on|off>");
            return;
        }
        
        if (parts[1] == "on") {
            connection_->start_async_receive();
            logger_.info("🔄 Async receive mode enabled");
        } else if (parts[1] == "off") {
            connection_->stop_async_receive();
            logger_.info("⏸️ Async receive mode disabled");
        } else {
            logger_.error("Use 'on' or 'off'");
        }
    }
    
    void handle_test(const std::vector<std::string>& parts) {
        if (parts.size() < 2) {
            logger_.error("Usage: test <url>");
            return;
        }
        
        const std::string& url = parts[1];
        logger_.info("🧪 Running connection test for {}", url);
        
        try {
            WebSocketClient client;
            client.set_timeout(std::chrono::seconds(5));
            
            auto start_time = std::chrono::high_resolution_clock::now();
            
            auto conn_result = client.connect(url);
            if (!conn_result) {
                logger_.error("❌ Test failed: {}", conn_result.error().message);
                return;
            }
            
            auto connection = std::move(conn_result.value());
            auto connect_time = std::chrono::high_resolution_clock::now();
            
            // Send test message
            std::string test_msg = "Hello from NetworkQuests!";
            auto send_result = connection->send_text(test_msg);
            if (!send_result) {
                logger_.error("❌ Send failed: {}", send_result.error().message);
                return;
            }
            
            // Try to receive echo
            auto receive_result = connection->receive_message(std::chrono::seconds(5));
            auto end_time = std::chrono::high_resolution_clock::now();
            
            auto connect_duration = std::chrono::duration_cast<std::chrono::milliseconds>(connect_time - start_time);
            auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            
            if (receive_result) {
                auto response = receive_result.value();
                logger_.info("✅ Test successful!");
                logger_.info("   Connect time: {}ms", connect_duration.count());
                logger_.info("   Total time: {}ms", total_duration.count());
                logger_.info("   Response: {}", response.as_string());
            } else {
                logger_.warn("⚠️ No response received (timeout)");
            }
            
            connection->close();
            
        } catch (const std::exception& e) {
            logger_.error("❌ Test exception: {}", e.what());
        }
    }
    
    void handle_benchmark(const std::vector<std::string>& parts) {
        if (parts.size() < 3) {
            logger_.error("Usage: benchmark <url> <count>");
            return;
        }
        
        const std::string& url = parts[1];
        int count;
        try {
            count = std::stoi(parts[2]);
        } catch (const std::exception&) {
            logger_.error("Invalid count: {}", parts[2]);
            return;
        }
        
        if (count <= 0 || count > 10000) {
            logger_.error("Count must be between 1 and 10000");
            return;
        }
        
        logger_.info("🏁 Running benchmark: {} messages to {}", count, url);
        
        try {
            WebSocketClient client;
            auto conn_result = client.connect(url);
            if (!conn_result) {
                logger_.error("❌ Connection failed: {}", conn_result.error().message);
                return;
            }
            
            auto connection = std::move(conn_result.value());
            
            auto start_time = std::chrono::high_resolution_clock::now();
            
            for (int i = 0; i < count; ++i) {
                std::string message = "Benchmark message " + std::to_string(i + 1);
                auto result = connection->send_text(message);
                if (!result) {
                    logger_.error("❌ Send failed at message {}: {}", i + 1, result.error().message);
                    break;
                }
                
                if ((i + 1) % 100 == 0) {
                    logger_.info("📤 Sent {} messages", i + 1);
                }
            }
            
            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            
            double messages_per_second = (static_cast<double>(count) * 1000.0) / duration.count();
            
            logger_.info("✅ Benchmark completed!");
            logger_.info("   Messages: {}", count);
            logger_.info("   Duration: {}ms", duration.count());
            logger_.info("   Rate: {:.2f} messages/second", messages_per_second);
            
            connection->close();
            
        } catch (const std::exception& e) {
            logger_.error("❌ Benchmark exception: {}", e.what());
        }
    }
    
    void handle_quit() {
        logger_.info("👋 Goodbye!");
        if (connection_) {
            connection_->close();
        }
        running_ = false;
    }
    
    bool check_connection() {
        if (!connection_ || !connection_->is_connected()) {
            logger_.error("❌ Not connected. Use 'connect <url>' first.");
            return false;
        }
        return true;
    }
    
    void setup_connection_handlers() {
        connection_->set_message_handler([this](const WebSocketMessage& message) {
            switch (message.type()) {
                case WebSocketFrameType::Text:
                    logger_.info("📥 Received text: {}", message.as_string());
                    break;
                case WebSocketFrameType::Binary:
                    logger_.info("📥 Received binary: {} bytes", message.size());
                    break;
                default:
                    logger_.info("📥 Received control message");
                    break;
            }
        });
        
        connection_->set_close_handler([this](WebSocketCloseCode code, std::string_view reason) {
            logger_.info("🔒 Connection closed (code: {}, reason: {})", 
                        static_cast<int>(code), 
                        reason.empty() ? "(none)" : std::string(reason));
        });
        
        connection_->set_error_handler([this](const std::string& error) {
            logger_.error("❌ Connection error: {}", error);
        });
    }
    
    std::vector<uint8_t> hex_to_bytes(const std::string& hex) {
        std::vector<uint8_t> bytes;
        
        if (hex.length() % 2 != 0) {
            return bytes; // Invalid hex string
        }
        
        for (size_t i = 0; i < hex.length(); i += 2) {
            std::string byte_str = hex.substr(i, 2);
            try {
                uint8_t byte = static_cast<uint8_t>(std::stoi(byte_str, nullptr, 16));
                bytes.push_back(byte);
            } catch (const std::exception&) {
                return {}; // Invalid hex character
            }
        }
        
        return bytes;
    }
};

int main() {
    try {
        // Initialize networking
        if (!Socket::initialize_platform()) {
            std::cerr << "Failed to initialize networking\n";
            return 1;
        }
        
        InteractiveWebSocketClient client;
        client.run();
        
        Socket::cleanup_platform();
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }
}