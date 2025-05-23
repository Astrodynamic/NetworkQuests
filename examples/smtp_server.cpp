#include "networkquests/smtp.hpp"

#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <thread>
#include <chrono>
#include <mutex>
#include <sstream>
#include <iomanip>

using namespace networkquests;
using namespace networkquests::smtp;

class MailStorage {
public:
    struct StoredMessage {
        std::string message_id;
        EmailAddress from;
        std::vector<EmailAddress> to;
        std::string subject;
        std::string body;
        std::chrono::system_clock::time_point received_time;
        std::string raw_content;
    };

    void store_message(const SmtpMessage& message, const SocketAddress& sender_addr) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        StoredMessage stored;
        stored.message_id = message.get_message_id();
        stored.from = message.from();
        stored.to = message.to();
        stored.subject = message.subject();
        stored.body = message.body();
        stored.received_time = std::chrono::system_clock::now();
        stored.raw_content = message.to_mime_string();
        
        messages_.push_back(stored);
        
        // Log the received message
        std::cout << "\n📧 New message received:" << std::endl;
        std::cout << "  From: " << stored.from.to_string() << std::endl;
        std::cout << "  To: ";
        for (size_t i = 0; i < stored.to.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << stored.to[i].to_string();
        }
        std::cout << std::endl;
        std::cout << "  Subject: " << stored.subject << std::endl;
        std::cout << "  Sender IP: " << sender_addr.to_string() << std::endl;
        std::cout << "  Message ID: " << stored.message_id << std::endl;
        std::cout << "  Size: " << stored.raw_content.length() << " bytes" << std::endl;
        
        // Save to file
        save_message_to_file(stored);
    }

    std::vector<StoredMessage> get_all_messages() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return messages_;
    }

    size_t get_message_count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return messages_.size();
    }

    void clear_messages() {
        std::lock_guard<std::mutex> lock(mutex_);
        messages_.clear();
        std::cout << "All stored messages cleared." << std::endl;
    }

private:
    mutable std::mutex mutex_;
    std::vector<StoredMessage> messages_;

    void save_message_to_file(const StoredMessage& message) {
        auto time_t = std::chrono::system_clock::to_time_t(message.received_time);
        auto tm = std::localtime(&time_t);
        
        std::ostringstream filename;
        filename << "mail_" << std::put_time(tm, "%Y%m%d_%H%M%S") 
                 << "_" << messages_.size() << ".eml";
        
        std::ofstream file(filename.str());
        if (file) {
            file << message.raw_content;
            std::cout << "  Saved to: " << filename.str() << std::endl;
        }
    }
};

class UserManager {
public:
    UserManager() {
        // Add some default users
        add_user("admin", "admin123", {"admin@localhost", "admin@example.com"});
        add_user("test", "test123", {"test@localhost", "user@example.com"});
        add_user("demo", "demo123", {"demo@localhost"});
    }

    void add_user(const std::string& username, const std::string& password, 
                  const std::vector<std::string>& email_addresses) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        User user;
        user.username = username;
        user.password = password;
        for (const auto& addr : email_addresses) {
            user.email_addresses.insert(addr);
        }
        
        users_[username] = user;
        
        // Add email to user mapping
        for (const auto& addr : email_addresses) {
            email_to_user_[addr] = username;
        }
    }

    bool authenticate(const std::string& username, const std::string& password) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = users_.find(username);
        if (it != users_.end()) {
            return it->second.password == password;
        }
        return false;
    }

    bool is_valid_recipient(const EmailAddress& email) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return email_to_user_.find(email.address()) != email_to_user_.end();
    }

    std::vector<std::string> get_all_users() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> usernames;
        for (const auto& [username, user] : users_) {
            usernames.push_back(username);
        }
        return usernames;
    }

    std::vector<std::string> get_user_emails(const std::string& username) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = users_.find(username);
        if (it != users_.end()) {
            return std::vector<std::string>(it->second.email_addresses.begin(), 
                                          it->second.email_addresses.end());
        }
        return {};
    }

private:
    struct User {
        std::string username;
        std::string password;
        std::unordered_set<std::string> email_addresses;
    };

    mutable std::mutex mutex_;
    std::unordered_map<std::string, User> users_;
    std::unordered_map<std::string, std::string> email_to_user_;
};

class SmtpServerDemo {
public:
    SmtpServerDemo() : server_(2525) {  // Use port 2525 to avoid conflicts
        setup_server();
    }

    void run() {
        std::cout << "=== NetworkQuests SMTP Server Demo ===" << std::endl;
        std::cout << "Simple Mail Transfer Protocol Server Implementation" << std::endl;
        std::cout << "Educational demonstration of email receiving" << std::endl;
        std::cout << std::endl;

        while (true) {
            display_menu();
            int choice = get_user_choice();
            
            switch (choice) {
                case 1:
                    start_server();
                    break;
                case 2:
                    stop_server();
                    break;
                case 3:
                    show_server_status();
                    break;
                case 4:
                    show_received_messages();
                    break;
                case 5:
                    manage_users();
                    break;
                case 6:
                    configure_server();
                    break;
                case 7:
                    clear_messages();
                    break;
                case 8:
                    show_server_statistics();
                    break;
                case 9:
                    test_client_connection();
                    break;
                case 0:
                    if (server_.is_running()) {
                        server_.stop();
                    }
                    std::cout << "Server stopped. Goodbye!" << std::endl;
                    return;
                default:
                    std::cout << "Invalid choice. Please try again." << std::endl;
            }
            
            std::cout << std::endl;
        }
    }

private:
    SmtpServer server_;
    MailStorage mail_storage_;
    UserManager user_manager_;
    std::chrono::steady_clock::time_point start_time_;
    std::atomic<size_t> total_connections_{0};
    std::atomic<size_t> successful_messages_{0};
    std::atomic<size_t> failed_messages_{0};

    void display_menu() {
        std::cout << "Choose an option:" << std::endl;
        std::cout << "1. Start Server" << std::endl;
        std::cout << "2. Stop Server" << std::endl;
        std::cout << "3. Show Server Status" << std::endl;
        std::cout << "4. Show Received Messages" << std::endl;
        std::cout << "5. Manage Users" << std::endl;
        std::cout << "6. Configure Server" << std::endl;
        std::cout << "7. Clear Messages" << std::endl;
        std::cout << "8. Show Statistics" << std::endl;
        std::cout << "9. Test Client Connection" << std::endl;
        std::cout << "0. Exit" << std::endl;
        std::cout << "Enter choice: ";
    }

    int get_user_choice() {
        int choice;
        std::cin >> choice;
        std::cin.ignore();
        return choice;
    }

    void setup_server() {
        server_.set_hostname("networkquests.local");
        server_.set_max_connections(50);
        server_.set_max_message_size(10 * 1024 * 1024); // 10MB
        server_.set_require_auth(false); // Allow unauthenticated for demo
        
        // Add SMTP extensions
        server_.add_supported_extension(SmtpExtension::AUTH);
        server_.add_supported_extension(SmtpExtension::SIZE);
        server_.add_supported_extension(SmtpExtension::ENHANCEDSTATUSCODES);
        
        // Set up event handlers
        server_.set_message_handler([this](const SmtpMessage& message, const SocketAddress& sender) {
            mail_storage_.store_message(message, sender);
            successful_messages_.fetch_add(1);
        });
        
        server_.set_auth_validator([this](std::string_view username, std::string_view password) {
            return user_manager_.authenticate(std::string(username), std::string(password));
        });
        
        server_.set_recipient_validator([this](const EmailAddress& email) {
            return user_manager_.is_valid_recipient(email);
        });
    }

    void start_server() {
        if (server_.is_running()) {
            std::cout << "Server is already running!" << std::endl;
            return;
        }

        std::cout << "Starting SMTP server on port 2525..." << std::endl;
        
        auto result = server_.start();
        if (result) {
            start_time_ = std::chrono::steady_clock::now();
            std::cout << "✓ SMTP server started successfully!" << std::endl;
            std::cout << "Server listening on: " << server_.local_address().to_string() << std::endl;
            std::cout << "\nTo test the server, you can use:" << std::endl;
            std::cout << "  telnet localhost 2525" << std::endl;
            std::cout << "  or the SMTP client example" << std::endl;
            std::cout << "\nExample SMTP session:" << std::endl;
            std::cout << "  EHLO test.client" << std::endl;
            std::cout << "  MAIL FROM:<sender@example.com>" << std::endl;
            std::cout << "  RCPT TO:<test@localhost>" << std::endl;
            std::cout << "  DATA" << std::endl;
            std::cout << "  Subject: Test Message" << std::endl;
            std::cout << "  " << std::endl;
            std::cout << "  Hello, this is a test message!" << std::endl;
            std::cout << "  ." << std::endl;
            std::cout << "  QUIT" << std::endl;
        } else {
            std::cout << "✗ Failed to start server: " << result.error().message << std::endl;
        }
    }

    void stop_server() {
        if (!server_.is_running()) {
            std::cout << "Server is not running!" << std::endl;
            return;
        }

        std::cout << "Stopping SMTP server..." << std::endl;
        server_.stop();
        
        // Wait a moment for cleanup
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        std::cout << "✓ SMTP server stopped." << std::endl;
    }

    void show_server_status() {
        std::cout << "\n=== Server Status ===" << std::endl;
        std::cout << "Running: " << (server_.is_running() ? "Yes" : "No") << std::endl;
        
        if (server_.is_running()) {
            std::cout << "Listen address: " << server_.local_address().to_string() << std::endl;
            std::cout << "Active connections: " << server_.active_connections() << std::endl;
            
            auto now = std::chrono::steady_clock::now();
            auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - start_time_);
            std::cout << "Uptime: " << uptime.count() << " seconds" << std::endl;
        }
        
        std::cout << "Total messages received: " << mail_storage_.get_message_count() << std::endl;
        std::cout << "Successful deliveries: " << successful_messages_.load() << std::endl;
        std::cout << "Failed deliveries: " << failed_messages_.load() << std::endl;
        std::cout << "Total connections: " << total_connections_.load() << std::endl;
    }

    void show_received_messages() {
        std::cout << "\n=== Received Messages ===" << std::endl;
        
        auto messages = mail_storage_.get_all_messages();
        if (messages.empty()) {
            std::cout << "No messages received yet." << std::endl;
            return;
        }

        std::cout << "Total messages: " << messages.size() << std::endl;
        std::cout << std::endl;

        for (size_t i = 0; i < messages.size(); ++i) {
            const auto& msg = messages[i];
            auto time_t = std::chrono::system_clock::to_time_t(msg.received_time);
            
            std::cout << "Message " << (i + 1) << ":" << std::endl;
            std::cout << "  Time: " << std::ctime(&time_t);
            std::cout << "  From: " << msg.from.to_string() << std::endl;
            std::cout << "  To: ";
            for (size_t j = 0; j < msg.to.size(); ++j) {
                if (j > 0) std::cout << ", ";
                std::cout << msg.to[j].to_string();
            }
            std::cout << std::endl;
            std::cout << "  Subject: " << msg.subject << std::endl;
            std::cout << "  Size: " << msg.raw_content.length() << " bytes" << std::endl;
            
            if (msg.body.length() > 100) {
                std::cout << "  Body: " << msg.body.substr(0, 100) << "..." << std::endl;
            } else {
                std::cout << "  Body: " << msg.body << std::endl;
            }
            std::cout << std::endl;
        }

        // Ask if user wants to see full message
        std::cout << "Enter message number to view full content (0 to continue): ";
        int msg_num;
        std::cin >> msg_num;
        std::cin.ignore();

        if (msg_num > 0 && msg_num <= static_cast<int>(messages.size())) {
            const auto& msg = messages[msg_num - 1];
            std::cout << "\n=== Full Message Content ===" << std::endl;
            std::cout << msg.raw_content << std::endl;
            std::cout << "=========================" << std::endl;
        }
    }

    void manage_users() {
        std::cout << "\n=== User Management ===" << std::endl;
        
        while (true) {
            std::cout << "\nUser options:" << std::endl;
            std::cout << "1. List users" << std::endl;
            std::cout << "2. Add user" << std::endl;
            std::cout << "3. Test authentication" << std::endl;
            std::cout << "0. Back to main menu" << std::endl;
            std::cout << "Choice: ";
            
            int choice;
            std::cin >> choice;
            std::cin.ignore();
            
            switch (choice) {
                case 1:
                    list_users();
                    break;
                case 2:
                    add_user();
                    break;
                case 3:
                    test_authentication();
                    break;
                case 0:
                    return;
                default:
                    std::cout << "Invalid choice." << std::endl;
            }
        }
    }

    void list_users() {
        auto users = user_manager_.get_all_users();
        std::cout << "\nRegistered users:" << std::endl;
        
        for (const auto& username : users) {
            auto emails = user_manager_.get_user_emails(username);
            std::cout << "  " << username << ": ";
            for (size_t i = 0; i < emails.size(); ++i) {
                if (i > 0) std::cout << ", ";
                std::cout << emails[i];
            }
            std::cout << std::endl;
        }
    }

    void add_user() {
        std::string username, password, email_list;
        
        std::cout << "Enter username: ";
        std::getline(std::cin, username);
        
        std::cout << "Enter password: ";
        std::getline(std::cin, password);
        
        std::cout << "Enter email addresses (comma-separated): ";
        std::getline(std::cin, email_list);
        
        // Parse email list
        std::vector<std::string> emails;
        std::istringstream iss(email_list);
        std::string email;
        
        while (std::getline(iss, email, ',')) {
            // Trim whitespace
            email.erase(0, email.find_first_not_of(" \t"));
            email.erase(email.find_last_not_of(" \t") + 1);
            if (!email.empty()) {
                emails.push_back(email);
            }
        }
        
        if (emails.empty()) {
            std::cout << "No valid email addresses provided." << std::endl;
            return;
        }
        
        user_manager_.add_user(username, password, emails);
        std::cout << "User added successfully!" << std::endl;
    }

    void test_authentication() {
        std::string username, password;
        
        std::cout << "Enter username: ";
        std::getline(std::cin, username);
        
        std::cout << "Enter password: ";
        std::getline(std::cin, password);
        
        bool result = user_manager_.authenticate(username, password);
        std::cout << "Authentication " << (result ? "successful" : "failed") << std::endl;
    }

    void configure_server() {
        std::cout << "\n=== Server Configuration ===" << std::endl;
        
        while (true) {
            std::cout << "\nConfiguration options:" << std::endl;
            std::cout << "1. Change hostname" << std::endl;
            std::cout << "2. Change max connections" << std::endl;
            std::cout << "3. Change max message size" << std::endl;
            std::cout << "4. Toggle authentication requirement" << std::endl;
            std::cout << "5. Show current configuration" << std::endl;
            std::cout << "0. Back to main menu" << std::endl;
            std::cout << "Choice: ";
            
            int choice;
            std::cin >> choice;
            std::cin.ignore();
            
            switch (choice) {
                case 1:
                    change_hostname();
                    break;
                case 2:
                    change_max_connections();
                    break;
                case 3:
                    change_max_message_size();
                    break;
                case 4:
                    toggle_auth_requirement();
                    break;
                case 5:
                    show_configuration();
                    break;
                case 0:
                    return;
                default:
                    std::cout << "Invalid choice." << std::endl;
            }
        }
    }

    void change_hostname() {
        std::string hostname;
        std::cout << "Enter new hostname: ";
        std::getline(std::cin, hostname);
        
        server_.set_hostname(hostname);
        std::cout << "Hostname changed to: " << hostname << std::endl;
        std::cout << "Note: Restart server for changes to take effect." << std::endl;
    }

    void change_max_connections() {
        int max_conn;
        std::cout << "Enter max connections: ";
        std::cin >> max_conn;
        std::cin.ignore();
        
        if (max_conn > 0) {
            server_.set_max_connections(max_conn);
            std::cout << "Max connections set to: " << max_conn << std::endl;
        } else {
            std::cout << "Invalid value." << std::endl;
        }
    }

    void change_max_message_size() {
        int size_mb;
        std::cout << "Enter max message size (MB): ";
        std::cin >> size_mb;
        std::cin.ignore();
        
        if (size_mb > 0) {
            server_.set_max_message_size(size_mb * 1024 * 1024);
            std::cout << "Max message size set to: " << size_mb << "MB" << std::endl;
        } else {
            std::cout << "Invalid value." << std::endl;
        }
    }

    void toggle_auth_requirement() {
        static bool require_auth = false;
        require_auth = !require_auth;
        
        server_.set_require_auth(require_auth);
        std::cout << "Authentication requirement: " << (require_auth ? "Enabled" : "Disabled") << std::endl;
        std::cout << "Note: Restart server for changes to take effect." << std::endl;
    }

    void show_configuration() {
        std::cout << "\nCurrent server configuration:" << std::endl;
        // Note: In a real implementation, these would be getter methods
        std::cout << "  Port: 2525" << std::endl;
        std::cout << "  Hostname: networkquests.local" << std::endl;
        std::cout << "  Max connections: 50" << std::endl;
        std::cout << "  Max message size: 10MB" << std::endl;
        std::cout << "  Authentication required: No" << std::endl;
        std::cout << "  Supported extensions: AUTH, SIZE, ENHANCEDSTATUSCODES" << std::endl;
    }

    void clear_messages() {
        std::cout << "Are you sure you want to clear all messages? (y/n): ";
        std::string confirm;
        std::getline(std::cin, confirm);
        
        if (confirm == "y" || confirm == "Y") {
            mail_storage_.clear_messages();
            successful_messages_.store(0);
            failed_messages_.store(0);
        }
    }

    void show_server_statistics() {
        std::cout << "\n=== Server Statistics ===" << std::endl;
        
        auto messages = mail_storage_.get_all_messages();
        
        std::cout << "Total messages: " << messages.size() << std::endl;
        std::cout << "Successful deliveries: " << successful_messages_.load() << std::endl;
        std::cout << "Failed deliveries: " << failed_messages_.load() << std::endl;
        std::cout << "Total connections: " << total_connections_.load() << std::endl;
        
        if (server_.is_running()) {
            auto now = std::chrono::steady_clock::now();
            auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - start_time_);
            std::cout << "Uptime: " << uptime.count() << " seconds" << std::endl;
            std::cout << "Active connections: " << server_.active_connections() << std::endl;
            
            if (uptime.count() > 0) {
                double msg_per_sec = static_cast<double>(messages.size()) / uptime.count();
                std::cout << "Messages per second: " << std::fixed << std::setprecision(2) << msg_per_sec << std::endl;
            }
        }
        
        // Message size statistics
        if (!messages.empty()) {
            size_t total_size = 0;
            size_t min_size = SIZE_MAX;
            size_t max_size = 0;
            
            for (const auto& msg : messages) {
                size_t size = msg.raw_content.length();
                total_size += size;
                min_size = std::min(min_size, size);
                max_size = std::max(max_size, size);
            }
            
            std::cout << "\nMessage size statistics:" << std::endl;
            std::cout << "  Total: " << total_size << " bytes" << std::endl;
            std::cout << "  Average: " << (total_size / messages.size()) << " bytes" << std::endl;
            std::cout << "  Minimum: " << min_size << " bytes" << std::endl;
            std::cout << "  Maximum: " << max_size << " bytes" << std::endl;
        }
    }

    void test_client_connection() {
        std::cout << "\n=== Test Client Connection ===" << std::endl;
        
        if (!server_.is_running()) {
            std::cout << "Server is not running. Start the server first." << std::endl;
            return;
        }
        
        std::cout << "Testing connection to localhost:2525..." << std::endl;
        
        try {
            SmtpClient client;
            client.set_timeout(std::chrono::seconds(10));
            
            auto connect_result = client.connect("localhost", 2525);
            if (connect_result) {
                std::cout << "✓ Connection successful!" << std::endl;
                
                // Test sending a message
                SmtpMessage test_msg;
                test_msg.set_from(EmailAddress("Test Client", "client@example.com"));
                test_msg.add_to(EmailAddress("test@localhost"));
                test_msg.set_subject("Test Message from SMTP Server Demo");
                test_msg.set_body("This is a test message sent from the server demo's built-in client test.");
                
                std::cout << "Sending test message..." << std::endl;
                auto send_result = client.send_message(test_msg);
                if (send_result) {
                    std::cout << "✓ Test message sent successfully!" << std::endl;
                } else {
                    std::cout << "✗ Failed to send test message: " << send_result.error().message << std::endl;
                }
                
                client.disconnect();
            } else {
                std::cout << "✗ Connection failed: " << connect_result.error().message << std::endl;
            }
            
        } catch (const std::exception& e) {
            std::cout << "Exception during test: " << e.what() << std::endl;
        }
    }
};

int main() {
    try {
        SmtpServerDemo demo;
        demo.run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}