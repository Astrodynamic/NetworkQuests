#include "networkquests/ftp.hpp"
#include "networkquests/logger.hpp"
#include <iostream>
#include <string>
#include <sstream>
#include <filesystem>
#include <thread>
#include <chrono>
#include <signal.h>

using namespace NetworkQuests;
using namespace NetworkQuests::Ftp;

class FtpServerDemo {
public:
    FtpServerDemo() : server_(21), running_(false) {
        Logger::set_level(Logger::Level::INFO);
        setup_signal_handlers();
        setup_default_configuration();
    }
    
    void run() {
        print_welcome();
        
        // Start server
        std::cout << "Starting FTP server on port " << server_.get_port() << "...\n";
        
        auto result = server_.start();
        if (!result) {
            std::cerr << "Failed to start server: " << result.error().message() << std::endl;
            return;
        }
        
        running_ = true;
        std::cout << "FTP server started successfully!\n";
        std::cout << "Root directory: " << server_.get_root_directory() << "\n";
        std::cout << "Type 'help' for available commands.\n\n";
        
        // Start management console
        run_console();
        
        // Stop server
        std::cout << "\nShutting down server...\n";
        server_.stop();
        running_ = false;
    }

private:
    FtpServer server_;
    std::atomic<bool> running_;
    
    void print_welcome() {
        std::cout << R"(
╔══════════════════════════════════════════════════════════════════════════════╗
║                          NetworkQuests FTP Server                           ║
║                       Educational FTP Server Demo                           ║
╚══════════════════════════════════════════════════════════════════════════════╝

Production-ready FTP server with full RFC 959 compliance.

Features:
• Multi-threaded client handling
• User authentication and permissions
• Anonymous access support
• Active and passive data connections
• File and directory operations
• Session management and logging

)" << std::endl;
    }
    
    void setup_signal_handlers() {
        signal(SIGINT, [](int) {
            std::cout << "\nReceived interrupt signal. Shutting down...\n";
            exit(0);
        });
        
        signal(SIGTERM, [](int) {
            std::cout << "\nReceived termination signal. Shutting down...\n";
            exit(0);
        });
    }
    
    void setup_default_configuration() {
        // Set server root directory
        std::string root_dir = "./ftp_root";
        try {
            std::filesystem::create_directories(root_dir);
            server_.set_root_directory(root_dir);
        } catch (const std::exception& e) {
            std::cerr << "Warning: Failed to create root directory: " << e.what() << std::endl;
        }
        
        // Enable anonymous access with read-only permissions
        FtpUserPermissions anon_perms;
        anon_perms.can_read = true;
        anon_perms.can_list = true;
        anon_perms.can_write = false;
        anon_perms.can_delete = false;
        anon_perms.can_create_dirs = false;
        anon_perms.can_rename = false;
        anon_perms.root_directory = root_dir;
        
        server_.enable_anonymous_access(true, anon_perms);
        
        // Add a test user with full permissions
        FtpUserPermissions user_perms;
        user_perms.can_read = true;
        user_perms.can_write = true;
        user_perms.can_delete = true;
        user_perms.can_list = true;
        user_perms.can_create_dirs = true;
        user_perms.can_rename = true;
        user_perms.root_directory = root_dir;
        
        FtpUser test_user("testuser", "testpass", user_perms);
        server_.add_user(test_user);
        
        // Create some sample content
        create_sample_content(root_dir);
    }
    
    void create_sample_content(const std::string& root_dir) {
        try {
            // Create directories
            std::filesystem::create_directories(root_dir + "/public");
            std::filesystem::create_directories(root_dir + "/uploads");
            std::filesystem::create_directories(root_dir + "/documents");
            
            // Create sample files
            {
                std::ofstream file(root_dir + "/README.txt");
                file << "Welcome to NetworkQuests FTP Server!\n\n";
                file << "This is a demonstration of the NetworkQuests FTP implementation.\n";
                file << "You can upload and download files, navigate directories, and more.\n\n";
                file << "Anonymous users have read-only access.\n";
                file << "Use 'testuser' / 'testpass' for full access.\n\n";
                file << "Directories:\n";
                file << "  /public    - Public files for all users\n";
                file << "  /uploads   - Upload area for authenticated users\n";
                file << "  /documents - Document storage\n";
            }
            
            {
                std::ofstream file(root_dir + "/public/info.txt");
                file << "This is a public file accessible to all users.\n";
                file << "FTP Server Features:\n";
                file << "- RFC 959 compliant\n";
                file << "- Multi-threaded\n";
                file << "- User authentication\n";
                file << "- Permission management\n";
                file << "- Active/Passive modes\n";
                file << "- ASCII/Binary transfers\n";
            }
            
            {
                std::ofstream file(root_dir + "/documents/sample.txt");
                file << "This is a sample document.\n";
                file << "You can download this file using:\n";
                file << "  get documents/sample.txt\n";
            }
            
        } catch (const std::exception& e) {
            std::cerr << "Warning: Failed to create sample content: " << e.what() << std::endl;
        }
    }
    
    void run_console() {
        std::string command;
        
        while (running_) {
            std::cout << "server> ";
            if (!std::getline(std::cin, command)) {
                break; // EOF
            }
            
            if (command.empty()) {
                continue;
            }
            
            process_console_command(command);
        }
    }
    
    void process_console_command(const std::string& command) {
        std::istringstream iss(command);
        std::string cmd;
        iss >> cmd;
        
        // Convert command to lowercase
        std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);
        
        try {
            if (cmd == "help") {
                show_console_help();
            } else if (cmd == "status") {
                show_server_status();
            } else if (cmd == "users") {
                list_users();
            } else if (cmd == "adduser") {
                handle_add_user(iss);
            } else if (cmd == "deluser") {
                handle_delete_user(iss);
            } else if (cmd == "setroot") {
                handle_set_root(iss);
            } else if (cmd == "anonymous") {
                handle_anonymous_toggle(iss);
            } else if (cmd == "connections") {
                show_connections();
            } else if (cmd == "logs") {
                show_logs();
            } else if (cmd == "config") {
                show_configuration();
            } else if (cmd == "stop" || cmd == "quit" || cmd == "exit") {
                running_ = false;
            } else if (cmd == "test") {
                run_self_test();
            } else {
                std::cout << "Unknown command: " << cmd << ". Type 'help' for available commands.\n";
            }
        } catch (const std::exception& e) {
            std::cout << "Error: " << e.what() << std::endl;
        }
    }
    
    void show_console_help() {
        std::cout << R"(
Server Management Commands:

Server Control:
  status                   - Show server status
  stop                     - Stop the server and exit
  config                   - Show current configuration

User Management:
  users                    - List all users
  adduser <name> <pass>    - Add a new user
  deluser <name>           - Delete a user
  anonymous <on|off>       - Enable/disable anonymous access

Server Configuration:
  setroot <path>           - Set server root directory
  connections              - Show active connections
  logs                     - Show recent log entries

Testing:
  test                     - Run self-test

General:
  help                     - Show this help message
  quit                     - Stop server and exit

)" << std::endl;
    }
    
    void show_server_status() {
        std::cout << "\n=== FTP Server Status ===\n";
        std::cout << "Running: " << (running_ ? "Yes" : "No") << std::endl;
        std::cout << "Port: " << server_.get_port() << std::endl;
        std::cout << "Root Directory: " << server_.get_root_directory() << std::endl;
        std::cout << "Max Connections: " << server_.get_max_connections() << std::endl;
        std::cout << "Anonymous Access: " << (server_.is_anonymous_enabled() ? "Enabled" : "Disabled") << std::endl;
        std::cout << "Total Connections: " << server_.get_total_connections() << std::endl;
        std::cout << "Active Sessions: " << server_.get_active_sessions() << std::endl;
        std::cout << "========================\n\n";
    }
    
    void list_users() {
        std::cout << "\n=== Registered Users ===\n";
        std::cout << "anonymous     - Anonymous access user\n";
        std::cout << "testuser      - Test user with full permissions\n";
        std::cout << "========================\n\n";
    }
    
    void handle_add_user(std::istringstream& iss) {
        std::string username, password;
        if (!(iss >> username >> password)) {
            std::cout << "Usage: adduser <username> <password>\n";
            return;
        }
        
        // Create user with default permissions
        FtpUserPermissions perms;
        perms.can_read = true;
        perms.can_write = true;
        perms.can_list = true;
        perms.can_delete = false;
        perms.can_create_dirs = false;
        perms.can_rename = false;
        perms.root_directory = server_.get_root_directory();
        
        FtpUser user(username, password, perms);
        server_.add_user(user);
        
        std::cout << "User '" << username << "' added successfully.\n";
    }
    
    void handle_delete_user(std::istringstream& iss) {
        std::string username;
        if (!(iss >> username)) {
            std::cout << "Usage: deluser <username>\n";
            return;
        }
        
        if (username == "anonymous" || username == "testuser") {
            std::cout << "Cannot delete system user '" << username << "'.\n";
            return;
        }
        
        server_.remove_user(username);
        std::cout << "User '" << username << "' deleted.\n";
    }
    
    void handle_set_root(std::istringstream& iss) {
        std::string path;
        std::getline(iss, path);
        path = trim(path);
        
        if (path.empty()) {
            std::cout << "Usage: setroot <path>\n";
            return;
        }
        
        try {
            std::filesystem::create_directories(path);
            server_.set_root_directory(path);
            std::cout << "Root directory set to: " << path << std::endl;
        } catch (const std::exception& e) {
            std::cout << "Failed to set root directory: " << e.what() << std::endl;
        }
    }
    
    void handle_anonymous_toggle(std::istringstream& iss) {
        std::string state;
        if (!(iss >> state)) {
            std::cout << "Usage: anonymous <on|off>\n";
            return;
        }
        
        std::transform(state.begin(), state.end(), state.begin(), ::tolower);
        
        if (state == "on") {
            FtpUserPermissions anon_perms;
            anon_perms.can_read = true;
            anon_perms.can_list = true;
            anon_perms.root_directory = server_.get_root_directory();
            
            server_.enable_anonymous_access(true, anon_perms);
            std::cout << "Anonymous access enabled.\n";
        } else if (state == "off") {
            server_.enable_anonymous_access(false);
            std::cout << "Anonymous access disabled.\n";
        } else {
            std::cout << "Invalid state. Use 'on' or 'off'.\n";
        }
    }
    
    void show_connections() {
        std::cout << "\n=== Active Connections ===\n";
        std::cout << "Active Sessions: " << server_.get_active_sessions() << std::endl;
        std::cout << "Max Connections: " << server_.get_max_connections() << std::endl;
        std::cout << "=========================\n\n";
    }
    
    void show_logs() {
        std::cout << "\n=== Recent Activity ===\n";
        std::cout << "Server running since startup\n";
        std::cout << "Total connections handled: " << server_.get_total_connections() << std::endl;
        std::cout << "======================\n\n";
    }
    
    void show_configuration() {
        std::cout << "\n=== Server Configuration ===\n";
        std::cout << "Port: " << server_.get_port() << std::endl;
        std::cout << "Root Directory: " << server_.get_root_directory() << std::endl;
        std::cout << "Max Connections: " << server_.get_max_connections() << std::endl;
        std::cout << "Session Timeout: " << server_.get_session_timeout().count() << " minutes" << std::endl;
        std::cout << "Anonymous Access: " << (server_.is_anonymous_enabled() ? "Enabled" : "Disabled") << std::endl;
        std::cout << "Welcome Message: " << server_.get_welcome_message() << std::endl;
        std::cout << "============================\n\n";
    }
    
    void run_self_test() {
        std::cout << "Running self-test...\n";
        
        try {
            // Test FTP client connection
            FtpClient test_client;
            
            std::cout << "1. Testing connection to localhost... ";
            auto connect_result = test_client.connect("127.0.0.1", server_.get_port());
            if (connect_result) {
                std::cout << "OK\n";
                
                std::cout << "2. Testing anonymous login... ";
                auto login_result = test_client.login_anonymous();
                if (login_result && login_result->is_success()) {
                    std::cout << "OK\n";
                    
                    std::cout << "3. Testing PWD command... ";
                    auto pwd_result = test_client.print_working_directory();
                    if (pwd_result && pwd_result->is_success()) {
                        std::cout << "OK\n";
                        
                        std::cout << "4. Testing directory listing... ";
                        auto list_result = test_client.list_directory();
                        if (list_result) {
                            std::cout << "OK\n";
                            
                            std::cout << "5. Testing system type... ";
                            auto syst_result = test_client.system_type();
                            if (syst_result && syst_result->is_success()) {
                                std::cout << "OK\n";
                                std::cout << "Self-test completed successfully!\n";
                            } else {
                                std::cout << "FAILED\n";
                            }
                        } else {
                            std::cout << "FAILED\n";
                        }
                    } else {
                        std::cout << "FAILED\n";
                    }
                } else {
                    std::cout << "FAILED\n";
                }
                
                test_client.disconnect();
            } else {
                std::cout << "FAILED\n";
                std::cout << "Connection error: " << connect_result.error().message() << std::endl;
            }
            
        } catch (const std::exception& e) {
            std::cout << "Self-test failed: " << e.what() << std::endl;
        }
    }
    
    std::string trim(const std::string& str) {
        size_t start = str.find_first_not_of(" \t");
        if (start == std::string::npos) return "";
        size_t end = str.find_last_not_of(" \t");
        return str.substr(start, end - start + 1);
    }
};

int main(int argc, char* argv[]) {
    try {
        std::cout << "Starting NetworkQuests FTP Server...\n";
        
        // Parse command line arguments
        uint16_t port = 21;
        if (argc > 1) {
            port = static_cast<uint16_t>(std::stoi(argv[1]));
        }
        
        FtpServerDemo demo;
        demo.run();
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}