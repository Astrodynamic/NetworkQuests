#include "networkquests/websocket.hpp"
#include "networkquests/logger.hpp"

#include <iostream>
#include <sstream>
#include <thread>
#include <chrono>
#include <atomic>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <memory>

using namespace networkquests;
using namespace networkquests::websocket;

class ChatRoom {
public:
    struct Client {
        std::string id;
        std::string name;
        std::shared_ptr<WebSocketConnection> connection;
        std::chrono::system_clock::time_point joined_at;
    };
    
    ChatRoom(const std::string& name) : name_(name) {}
    
    void add_client(const std::string& id, const std::string& name, 
                   std::shared_ptr<WebSocketConnection> connection) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        Client client;
        client.id = id;
        client.name = name;
        client.connection = connection;
        client.joined_at = std::chrono::system_clock::now();
        
        clients_[id] = client;
        
        // Notify others
        broadcast_message(name + " joined the room", id);
        
        // Send welcome message to new client
        send_private_message(id, "Welcome to " + name_ + "! " + 
                           std::to_string(clients_.size()) + " users online.");
        
        // Send user list
        send_user_list(id);
    }
    
    void remove_client(const std::string& id) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = clients_.find(id);
        if (it != clients_.end()) {
            std::string name = it->second.name;
            clients_.erase(it);
            
            // Notify others
            broadcast_message(name + " left the room", id);
        }
    }
    
    void broadcast_message(const std::string& message, const std::string& sender_id = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto timestamp = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(timestamp);
        auto tm = *std::localtime(&time_t);
        
        std::ostringstream oss;
        oss << "[" << std::put_time(&tm, "%H:%M:%S") << "] " << message;
        std::string formatted_message = oss.str();
        
        for (auto& [id, client] : clients_) {
            if (id != sender_id && client.connection && client.connection->is_connected()) {
                client.connection->send_text(formatted_message);
            }
        }
    }
    
    void send_private_message(const std::string& client_id, const std::string& message) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = clients_.find(client_id);
        if (it != clients_.end() && it->second.connection && it->second.connection->is_connected()) {
            it->second.connection->send_text("📢 " + message);
        }
    }
    
    void handle_message(const std::string& client_id, const std::string& message) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = clients_.find(client_id);
        if (it == clients_.end()) return;
        
        const auto& client = it->second;
        
        // Handle commands
        if (message.starts_with("/")) {
            handle_command(client_id, message);
            return;
        }
        
        // Regular chat message
        std::string formatted = client.name + ": " + message;
        
        // Broadcast to all other clients (excluding sender)
        for (auto& [id, other_client] : clients_) {
            if (id != client_id && other_client.connection && other_client.connection->is_connected()) {
                other_client.connection->send_text(formatted);
            }
        }
    }
    
    void handle_command(const std::string& client_id, const std::string& command) {
        auto parts = split_string(command);
        if (parts.empty()) return;
        
        const std::string& cmd = parts[0];
        
        if (cmd == "/help") {
            send_private_message(client_id, "Available commands:");
            send_private_message(client_id, "/help - Show this help");
            send_private_message(client_id, "/users - List online users");
            send_private_message(client_id, "/me <action> - Send action message");
            send_private_message(client_id, "/ping - Test connection");
            send_private_message(client_id, "/time - Show server time");
        } else if (cmd == "/users") {
            send_user_list(client_id);
        } else if (cmd == "/me" && parts.size() > 1) {
            auto it = clients_.find(client_id);
            if (it != clients_.end()) {
                std::string action;
                for (size_t i = 1; i < parts.size(); ++i) {
                    if (i > 1) action += " ";
                    action += parts[i];
                }
                broadcast_message("* " + it->second.name + " " + action, client_id);
            }
        } else if (cmd == "/ping") {
            send_private_message(client_id, "Pong! Server is alive.");
        } else if (cmd == "/time") {
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            auto tm = *std::localtime(&time_t);
            
            std::ostringstream oss;
            oss << "Server time: " << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
            send_private_message(client_id, oss.str());
        } else {
            send_private_message(client_id, "Unknown command: " + cmd + ". Type /help for available commands.");
        }
    }
    
    void send_user_list(const std::string& client_id) {
        std::ostringstream oss;
        oss << "Online users (" << clients_.size() << "):";
        
        for (const auto& [id, client] : clients_) {
            auto duration = std::chrono::system_clock::now() - client.joined_at;
            auto minutes = std::chrono::duration_cast<std::chrono::minutes>(duration).count();
            oss << "\n  • " << client.name << " (online " << minutes << "m)";
        }
        
        send_private_message(client_id, oss.str());
    }
    
    size_t client_count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return clients_.size();
    }
    
    std::vector<std::string> get_client_names() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> names;
        for (const auto& [id, client] : clients_) {
            names.push_back(client.name);
        }
        return names;
    }
    
private:
    std::string name_;
    std::unordered_map<std::string, Client> clients_;
    mutable std::mutex mutex_;
    
    std::vector<std::string> split_string(const std::string& str) {
        std::vector<std::string> parts;
        std::istringstream iss(str);
        std::string part;
        
        while (iss >> part) {
            parts.push_back(part);
        }
        
        return parts;
    }
};

class WebSocketChatServer {
public:
    WebSocketChatServer(Port port) : logger_("websocket_server"), port_(port), chat_room_("General") {}
    
    void run() {
        logger_.info("🌐 NetworkQuests WebSocket Chat Server");
        logger_.info("======================================");
        
        if (!start_server()) {
            return;
        }
        
        show_help();
        run_console();
        
        stop_server();
    }
    
private:
    Logger logger_;
    Port port_;
    std::unique_ptr<WebSocketServer> server_;
    ChatRoom chat_room_;
    std::atomic<bool> running_{true};
    std::atomic<size_t> connection_counter_{0};
    
    bool start_server() {
        logger_.info("🚀 Starting WebSocket server on port {}", port_);
        
        try {
            server_ = std::make_unique<WebSocketServer>(port_);
            
            // Set connection handler
            server_->set_connection_handler([this](std::unique_ptr<WebSocketConnection> connection) {
                handle_new_connection(std::move(connection));
            });
            
            // Set handshake validator
            server_->set_handshake_validator([this](const http::HttpRequest& request) {
                return validate_handshake(request);
            });
            
            auto result = server_->start();
            if (!result) {
                logger_.error("❌ Failed to start server: {}", result.error().message);
                return false;
            }
            
            logger_.info("✅ Server started successfully!");
            logger_.info("   Listening on: {}", server_->local_address().to_string());
            logger_.info("   WebSocket URL: ws://localhost:{}/chat", port_);
            
            return true;
            
        } catch (const std::exception& e) {
            logger_.error("❌ Server startup exception: {}", e.what());
            return false;
        }
    }
    
    void stop_server() {
        if (server_) {
            logger_.info("🛑 Stopping server...");
            server_->stop();
            server_.reset();
        }
    }
    
    void handle_new_connection(std::unique_ptr<WebSocketConnection> connection) {
        auto client_id = "client_" + std::to_string(connection_counter_.fetch_add(1));
        auto shared_connection = std::shared_ptr<WebSocketConnection>(std::move(connection));
        
        logger_.info("👋 New client connected: {} from {}", 
                    client_id, shared_connection->remote_address().to_string());
        
        // Send welcome and request name
        shared_connection->send_text("🎉 Welcome to NetworkQuests Chat Server!\n"
                                   "Please enter your name:");
        
        // Set up connection handlers
        setup_connection_handlers(client_id, shared_connection);
    }
    
    void setup_connection_handlers(const std::string& client_id, 
                                 std::shared_ptr<WebSocketConnection> connection) {
        bool name_set = false;
        std::string client_name;
        
        connection->set_message_handler([this, client_id, connection, &name_set, &client_name]
                                      (const WebSocketMessage& message) {
            if (message.type() != WebSocketFrameType::Text) {
                return;
            }
            
            std::string text = message.as_string();
            
            // Trim whitespace
            text.erase(0, text.find_first_not_of(" \t\r\n"));
            text.erase(text.find_last_not_of(" \t\r\n") + 1);
            
            if (!name_set) {
                // First message is the client name
                if (text.empty() || text.length() > 20) {
                    connection->send_text("❌ Please enter a valid name (1-20 characters):");
                    return;
                }
                
                client_name = text;
                name_set = true;
                
                // Add to chat room
                chat_room_.add_client(client_id, client_name, connection);
                
                logger_.info("✅ Client {} set name: {}", client_id, client_name);
                
                // Send chat instructions
                connection->send_text("🎯 You are now in the chat room! Type /help for commands.");
            } else {
                // Regular chat message
                chat_room_.handle_message(client_id, text);
            }
        });
        
        connection->set_close_handler([this, client_id, &client_name]
                                    (WebSocketCloseCode code, std::string_view reason) {
            logger_.info("👋 Client {} ({}) disconnected (code: {}, reason: {})", 
                        client_id, client_name.empty() ? "unnamed" : client_name,
                        static_cast<int>(code), 
                        reason.empty() ? "(none)" : std::string(reason));
            
            chat_room_.remove_client(client_id);
        });
        
        connection->set_error_handler([this, client_id](const std::string& error) {
            logger_.error("❌ Client {} error: {}", client_id, error);
        });
        
        // Start async receive
        connection->start_async_receive();
    }
    
    bool validate_handshake(const http::HttpRequest& request) {
        // Basic validation - could be enhanced with authentication
        logger_.info("🔍 Validating handshake from {}", 
                    request.get_header("Host").value_or("unknown"));
        
        // Check path
        if (request.uri() != "/chat" && request.uri() != "/") {
            logger_.warn("❌ Invalid path: {}", request.uri());
            return false;
        }
        
        return true;
    }
    
    void run_console() {
        std::thread console_thread([this]() {
            while (running_) {
                std::cout << "\nserver> ";
                std::string command;
                std::getline(std::cin, command);
                
                if (command.empty()) continue;
                
                handle_console_command(command);
            }
        });
        
        console_thread.join();
    }
    
    void handle_console_command(const std::string& command) {
        auto parts = split_command(command);
        if (parts.empty()) return;
        
        const std::string& cmd = parts[0];
        
        if (cmd == "help") {
            show_help();
        } else if (cmd == "status") {
            show_status();
        } else if (cmd == "users") {
            show_users();
        } else if (cmd == "broadcast") {
            handle_broadcast(parts);
        } else if (cmd == "announce") {
            handle_announce(parts);
        } else if (cmd == "test") {
            handle_test();
        } else if (cmd == "stop" || cmd == "quit") {
            handle_stop();
        } else {
            logger_.warn("Unknown command: {}. Type 'help' for available commands.", cmd);
        }
    }
    
    void show_help() {
        std::cout << "\n📋 Server Console Commands:\n";
        std::cout << "  help                    - Show this help\n";
        std::cout << "  status                  - Show server status\n";
        std::cout << "  users                   - List connected users\n";
        std::cout << "  broadcast <message>     - Send message to all users\n";
        std::cout << "  announce <message>      - Send server announcement\n";
        std::cout << "  test                    - Run server self-test\n";
        std::cout << "  stop                    - Stop server and exit\n";
    }
    
    void show_status() {
        logger_.info("📊 Server Status:");
        logger_.info("   Running: {}", server_ && server_->is_running() ? "Yes" : "No");
        logger_.info("   Port: {}", port_);
        logger_.info("   Connected clients: {}", chat_room_.client_count());
        logger_.info("   Active connections: {}", server_ ? server_->active_connections() : 0);
        
        if (server_) {
            logger_.info("   Local address: {}", server_->local_address().to_string());
        }
    }
    
    void show_users() {
        auto names = chat_room_.get_client_names();
        
        if (names.empty()) {
            logger_.info("📋 No users currently connected");
            return;
        }
        
        logger_.info("📋 Connected users ({}):", names.size());
        for (const auto& name : names) {
            logger_.info("   • {}", name);
        }
    }
    
    void handle_broadcast(const std::vector<std::string>& parts) {
        if (parts.size() < 2) {
            logger_.error("Usage: broadcast <message>");
            return;
        }
        
        std::string message;
        for (size_t i = 1; i < parts.size(); ++i) {
            if (i > 1) message += " ";
            message += parts[i];
        }
        
        chat_room_.broadcast_message("📢 Server: " + message);
        logger_.info("📡 Broadcast sent: {}", message);
    }
    
    void handle_announce(const std::vector<std::string>& parts) {
        if (parts.size() < 2) {
            logger_.error("Usage: announce <message>");
            return;
        }
        
        std::string message;
        for (size_t i = 1; i < parts.size(); ++i) {
            if (i > 1) message += " ";
            message += parts[i];
        }
        
        chat_room_.broadcast_message("🔔 ANNOUNCEMENT: " + message);
        logger_.info("📢 Announcement sent: {}", message);
    }
    
    void handle_test() {
        logger_.info("🧪 Running server self-test...");
        
        try {
            // Test client connection
            WebSocketClient client;
            std::string url = "ws://localhost:" + std::to_string(port_) + "/chat";
            
            auto conn_result = client.connect(url);
            if (!conn_result) {
                logger_.error("❌ Self-test failed: {}", conn_result.error().message);
                return;
            }
            
            auto connection = std::move(conn_result.value());
            
            // Send test name
            connection->send_text("TestBot");
            
            // Send test message
            connection->send_text("Hello from self-test!");
            
            // Wait a bit
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            // Close connection
            connection->close();
            
            logger_.info("✅ Self-test completed successfully!");
            
        } catch (const std::exception& e) {
            logger_.error("❌ Self-test exception: {}", e.what());
        }
    }
    
    void handle_stop() {
        logger_.info("🛑 Stopping server...");
        running_ = false;
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
};

int main(int argc, char* argv[]) {
    try {
        // Initialize networking
        if (!Socket::initialize_platform()) {
            std::cerr << "Failed to initialize networking\n";
            return 1;
        }
        
        // Parse command line arguments
        Port port = 8080;
        if (argc > 1) {
            try {
                port = static_cast<Port>(std::stoi(argv[1]));
            } catch (const std::exception&) {
                std::cerr << "Invalid port number: " << argv[1] << "\n";
                return 1;
            }
        }
        
        if (port < 1024 || port > 65535) {
            std::cerr << "Port must be between 1024 and 65535\n";
            return 1;
        }
        
        WebSocketChatServer server(port);
        server.run();
        
        Socket::cleanup_platform();
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }
}