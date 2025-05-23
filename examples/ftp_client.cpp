#include "networkquests/ftp.hpp"
#include "networkquests/logger.hpp"
#include <iostream>
#include <string>
#include <sstream>
#include <filesystem>
#include <chrono>
#include <iomanip>

using namespace NetworkQuests;
using namespace NetworkQuests::Ftp;

class FtpClientDemo {
public:
    FtpClientDemo() : client_(), connected_(false), logged_in_(false) {
        Logger::set_level(Logger::Level::INFO);
        
        // Set up progress callback
        client_.set_progress_callback([this](size_t transferred, size_t total) {
            show_progress(transferred, total);
        });
    }
    
    void run() {
        print_welcome();
        
        std::string command;
        while (true) {
            std::cout << "ftp> ";
            std::getline(std::cin, command);
            
            if (std::cin.eof() || command == "quit" || command == "exit") {
                break;
            }
            
            process_command(command);
        }
        
        if (connected_) {
            client_.disconnect();
        }
        
        std::cout << "\nGoodbye!\n";
    }

private:
    FtpClient client_;
    bool connected_;
    bool logged_in_;
    std::string current_host_;
    uint16_t current_port_;
    
    void print_welcome() {
        std::cout << R"(
╔══════════════════════════════════════════════════════════════════════════════╗
║                          NetworkQuests FTP Client                           ║
║                       Educational FTP Client Demo                           ║
╚══════════════════════════════════════════════════════════════════════════════╝

Interactive FTP client with full RFC 959 compliance.
Type 'help' for a list of available commands.

Features:
• File upload/download with progress tracking
• Directory navigation and management
• Both ASCII and binary transfer modes
• Active and passive connection modes
• Comprehensive error handling

)" << std::endl;
    }
    
    void process_command(const std::string& command) {
        if (command.empty()) {
            return;
        }
        
        std::istringstream iss(command);
        std::string cmd;
        iss >> cmd;
        
        // Convert command to lowercase for consistency
        std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);
        
        try {
            if (cmd == "help") {
                show_help();
            } else if (cmd == "connect" || cmd == "open") {
                handle_connect(iss);
            } else if (cmd == "disconnect" || cmd == "close") {
                handle_disconnect();
            } else if (cmd == "login" || cmd == "user") {
                handle_login(iss);
            } else if (cmd == "anonymous") {
                handle_anonymous_login();
            } else if (cmd == "status") {
                show_status();
            } else if (cmd == "ascii") {
                handle_ascii_mode();
            } else if (cmd == "binary" || cmd == "bin") {
                handle_binary_mode();
            } else if (cmd == "passive") {
                handle_passive_mode();
            } else if (cmd == "active") {
                handle_active_mode();
            } else if (cmd == "pwd") {
                handle_pwd();
            } else if (cmd == "ls" || cmd == "dir") {
                handle_ls(iss);
            } else if (cmd == "cd" || cmd == "cwd") {
                handle_cd(iss);
            } else if (cmd == "cdup") {
                handle_cdup();
            } else if (cmd == "mkdir") {
                handle_mkdir(iss);
            } else if (cmd == "rmdir") {
                handle_rmdir(iss);
            } else if (cmd == "get" || cmd == "recv") {
                handle_get(iss);
            } else if (cmd == "put" || cmd == "send") {
                handle_put(iss);
            } else if (cmd == "delete" || cmd == "del") {
                handle_delete(iss);
            } else if (cmd == "rename" || cmd == "mv") {
                handle_rename(iss);
            } else if (cmd == "size") {
                handle_size(iss);
            } else if (cmd == "time") {
                handle_time(iss);
            } else if (cmd == "syst") {
                handle_syst();
            } else if (cmd == "noop") {
                handle_noop();
            } else if (cmd == "quote") {
                handle_quote(iss);
            } else {
                std::cout << "Unknown command: " << cmd << ". Type 'help' for available commands.\n";
            }
        } catch (const std::exception& e) {
            std::cout << "Error: " << e.what() << std::endl;
        }
    }
    
    void show_help() {
        std::cout << R"(
Available Commands:

Connection Management:
  connect <host> [port]    - Connect to FTP server (default port 21)
  disconnect               - Disconnect from server
  login <user> [password]  - Login with username and password
  anonymous                - Login as anonymous user
  status                   - Show connection status

Transfer Settings:
  ascii                    - Set ASCII transfer mode
  binary                   - Set binary transfer mode
  passive                  - Use passive connection mode (default)
  active                   - Use active connection mode

Directory Operations:
  pwd                      - Print working directory
  ls [path]                - List directory contents
  cd <directory>           - Change directory
  cdup                     - Change to parent directory
  mkdir <directory>        - Create directory
  rmdir <directory>        - Remove directory

File Operations:
  get <remote> [local]     - Download file from server
  put <local> [remote]     - Upload file to server
  delete <file>            - Delete file on server
  rename <old> <new>       - Rename file on server
  size <file>              - Get file size
  time <file>              - Get file modification time

System Commands:
  syst                     - Get server system type
  noop                     - Send no-operation command
  quote <command>          - Send raw FTP command

General:
  help                     - Show this help message
  quit                     - Exit the client

)" << std::endl;
    }
    
    void handle_connect(std::istringstream& iss) {
        std::string host;
        uint16_t port = 21;
        
        if (!(iss >> host)) {
            std::cout << "Usage: connect <host> [port]\n";
            return;
        }
        
        iss >> port; // Optional port
        
        if (connected_) {
            std::cout << "Already connected. Disconnect first.\n";
            return;
        }
        
        std::cout << "Connecting to " << host << ":" << port << "...\n";
        
        auto result = client_.connect(host, port);
        if (result) {
            connected_ = true;
            current_host_ = host;
            current_port_ = port;
            std::cout << "Connected successfully.\n";
        } else {
            std::cout << "Connection failed: " << result.error().message() << std::endl;
        }
    }
    
    void handle_disconnect() {
        if (!connected_) {
            std::cout << "Not connected.\n";
            return;
        }
        
        client_.disconnect();
        connected_ = false;
        logged_in_ = false;
        std::cout << "Disconnected.\n";
    }
    
    void handle_login(std::istringstream& iss) {
        if (!connected_) {
            std::cout << "Not connected. Use 'connect' first.\n";
            return;
        }
        
        std::string username, password;
        if (!(iss >> username)) {
            std::cout << "Usage: login <username> [password]\n";
            return;
        }
        
        if (!(iss >> password)) {
            std::cout << "Password: ";
            std::getline(std::cin, password);
        }
        
        auto result = client_.login(username, password);
        if (result && result->is_success()) {
            logged_in_ = true;
            std::cout << "Login successful: " << result->get_message() << std::endl;
        } else {
            std::cout << "Login failed: " << (result ? result->get_message() : "Unknown error") << std::endl;
        }
    }
    
    void handle_anonymous_login() {
        if (!connected_) {
            std::cout << "Not connected. Use 'connect' first.\n";
            return;
        }
        
        auto result = client_.login_anonymous();
        if (result && result->is_success()) {
            logged_in_ = true;
            std::cout << "Anonymous login successful: " << result->get_message() << std::endl;
        } else {
            std::cout << "Anonymous login failed: " << (result ? result->get_message() : "Unknown error") << std::endl;
        }
    }
    
    void show_status() {
        std::cout << "\n=== FTP Client Status ===\n";
        std::cout << "Connected: " << (connected_ ? "Yes" : "No") << std::endl;
        if (connected_) {
            std::cout << "Host: " << current_host_ << ":" << current_port_ << std::endl;
            std::cout << "Logged in: " << (logged_in_ ? "Yes" : "No") << std::endl;
        }
        std::cout << "========================\n\n";
    }
    
    void handle_ascii_mode() {
        client_.set_transfer_mode(FtpTransferMode::ASCII);
        std::cout << "Transfer mode set to ASCII.\n";
    }
    
    void handle_binary_mode() {
        client_.set_transfer_mode(FtpTransferMode::BINARY);
        std::cout << "Transfer mode set to BINARY.\n";
    }
    
    void handle_passive_mode() {
        client_.set_connection_mode(FtpConnectionMode::PASSIVE);
        std::cout << "Connection mode set to PASSIVE.\n";
    }
    
    void handle_active_mode() {
        client_.set_connection_mode(FtpConnectionMode::ACTIVE);
        std::cout << "Connection mode set to ACTIVE.\n";
    }
    
    void handle_pwd() {
        check_logged_in();
        
        auto result = client_.print_working_directory();
        if (result && result->is_success()) {
            std::cout << "Current directory: " << result->get_message() << std::endl;
        } else {
            std::cout << "PWD failed: " << (result ? result->get_message() : "Unknown error") << std::endl;
        }
    }
    
    void handle_ls(std::istringstream& iss) {
        check_logged_in();
        
        std::string path;
        iss >> path; // Optional path
        
        auto result = client_.list_directory(path);
        if (result) {
            std::cout << "\n" << *result;
            if (!result->empty() && result->back() != '\n') {
                std::cout << std::endl;
            }
        } else {
            std::cout << "LIST failed: " << result.error().message() << std::endl;
        }
    }
    
    void handle_cd(std::istringstream& iss) {
        check_logged_in();
        
        std::string path;
        if (!(iss >> path)) {
            std::cout << "Usage: cd <directory>\n";
            return;
        }
        
        auto result = client_.change_directory(path);
        if (result && result->is_success()) {
            std::cout << "Directory changed: " << result->get_message() << std::endl;
        } else {
            std::cout << "CD failed: " << (result ? result->get_message() : "Unknown error") << std::endl;
        }
    }
    
    void handle_cdup() {
        check_logged_in();
        
        auto result = client_.change_to_parent_directory();
        if (result && result->is_success()) {
            std::cout << "Changed to parent directory: " << result->get_message() << std::endl;
        } else {
            std::cout << "CDUP failed: " << (result ? result->get_message() : "Unknown error") << std::endl;
        }
    }
    
    void handle_mkdir(std::istringstream& iss) {
        check_logged_in();
        
        std::string path;
        if (!(iss >> path)) {
            std::cout << "Usage: mkdir <directory>\n";
            return;
        }
        
        auto result = client_.make_directory(path);
        if (result && result->is_success()) {
            std::cout << "Directory created: " << result->get_message() << std::endl;
        } else {
            std::cout << "MKDIR failed: " << (result ? result->get_message() : "Unknown error") << std::endl;
        }
    }
    
    void handle_rmdir(std::istringstream& iss) {
        check_logged_in();
        
        std::string path;
        if (!(iss >> path)) {
            std::cout << "Usage: rmdir <directory>\n";
            return;
        }
        
        auto result = client_.remove_directory(path);
        if (result && result->is_success()) {
            std::cout << "Directory removed: " << result->get_message() << std::endl;
        } else {
            std::cout << "RMDIR failed: " << (result ? result->get_message() : "Unknown error") << std::endl;
        }
    }
    
    void handle_get(std::istringstream& iss) {
        check_logged_in();
        
        std::string remote_file, local_file;
        if (!(iss >> remote_file)) {
            std::cout << "Usage: get <remote_file> [local_file]\n";
            return;
        }
        
        iss >> local_file; // Optional local filename
        
        std::cout << "Downloading " << remote_file << "...\n";
        
        auto start_time = std::chrono::steady_clock::now();
        auto result = client_.download_file(remote_file, local_file);
        auto end_time = std::chrono::steady_clock::now();
        
        if (result && result->is_success()) {
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            std::cout << "\nDownload completed successfully in " << duration.count() << "ms\n";
            std::cout << "Server response: " << result->get_message() << std::endl;
        } else {
            std::cout << "\nDownload failed: " << (result ? result->get_message() : "Unknown error") << std::endl;
        }
    }
    
    void handle_put(std::istringstream& iss) {
        check_logged_in();
        
        std::string local_file, remote_file;
        if (!(iss >> local_file)) {
            std::cout << "Usage: put <local_file> [remote_file]\n";
            return;
        }
        
        iss >> remote_file; // Optional remote filename
        
        if (!std::filesystem::exists(local_file)) {
            std::cout << "Local file not found: " << local_file << std::endl;
            return;
        }
        
        std::cout << "Uploading " << local_file << "...\n";
        
        auto start_time = std::chrono::steady_clock::now();
        auto result = client_.upload_file(local_file, remote_file);
        auto end_time = std::chrono::steady_clock::now();
        
        if (result && result->is_success()) {
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            std::cout << "\nUpload completed successfully in " << duration.count() << "ms\n";
            std::cout << "Server response: " << result->get_message() << std::endl;
        } else {
            std::cout << "\nUpload failed: " << (result ? result->get_message() : "Unknown error") << std::endl;
        }
    }
    
    void handle_delete(std::istringstream& iss) {
        check_logged_in();
        
        std::string filename;
        if (!(iss >> filename)) {
            std::cout << "Usage: delete <filename>\n";
            return;
        }
        
        auto result = client_.delete_file(filename);
        if (result && result->is_success()) {
            std::cout << "File deleted: " << result->get_message() << std::endl;
        } else {
            std::cout << "DELETE failed: " << (result ? result->get_message() : "Unknown error") << std::endl;
        }
    }
    
    void handle_rename(std::istringstream& iss) {
        check_logged_in();
        
        std::string old_name, new_name;
        if (!(iss >> old_name >> new_name)) {
            std::cout << "Usage: rename <old_name> <new_name>\n";
            return;
        }
        
        auto result = client_.rename_file(old_name, new_name);
        if (result && result->is_success()) {
            std::cout << "File renamed: " << result->get_message() << std::endl;
        } else {
            std::cout << "RENAME failed: " << (result ? result->get_message() : "Unknown error") << std::endl;
        }
    }
    
    void handle_size(std::istringstream& iss) {
        check_logged_in();
        
        std::string filename;
        if (!(iss >> filename)) {
            std::cout << "Usage: size <filename>\n";
            return;
        }
        
        auto result = client_.get_file_size(filename);
        if (result && result->is_success()) {
            std::cout << "File size: " << result->get_message() << " bytes" << std::endl;
        } else {
            std::cout << "SIZE failed: " << (result ? result->get_message() : "Unknown error") << std::endl;
        }
    }
    
    void handle_time(std::istringstream& iss) {
        check_logged_in();
        
        std::string filename;
        if (!(iss >> filename)) {
            std::cout << "Usage: time <filename>\n";
            return;
        }
        
        auto result = client_.get_modification_time(filename);
        if (result && result->is_success()) {
            std::cout << "File modification time: " << result->get_message() << std::endl;
        } else {
            std::cout << "MDTM failed: " << (result ? result->get_message() : "Unknown error") << std::endl;
        }
    }
    
    void handle_syst() {
        check_connected();
        
        auto result = client_.system_type();
        if (result && result->is_success()) {
            std::cout << "Server system: " << result->get_message() << std::endl;
        } else {
            std::cout << "SYST failed: " << (result ? result->get_message() : "Unknown error") << std::endl;
        }
    }
    
    void handle_noop() {
        check_connected();
        
        auto result = client_.noop();
        if (result && result->is_success()) {
            std::cout << "NOOP: " << result->get_message() << std::endl;
        } else {
            std::cout << "NOOP failed: " << (result ? result->get_message() : "Unknown error") << std::endl;
        }
    }
    
    void handle_quote(std::istringstream& iss) {
        check_connected();
        
        std::string command;
        std::getline(iss, command);
        command = Utils::trim(command);
        
        if (command.empty()) {
            std::cout << "Usage: quote <ftp_command>\n";
            return;
        }
        
        // Parse command and arguments
        std::istringstream cmd_iss(command);
        std::string cmd, args;
        cmd_iss >> cmd;
        std::getline(cmd_iss, args);
        args = Utils::trim(args);
        
        auto result = client_.send_command(cmd, args);
        if (result) {
            std::cout << "Server response: " << result->get_code_number() << " " << result->get_message() << std::endl;
        } else {
            std::cout << "Command failed: " << result.error().message() << std::endl;
        }
    }
    
    void check_connected() {
        if (!connected_) {
            throw std::runtime_error("Not connected. Use 'connect' first.");
        }
    }
    
    void check_logged_in() {
        check_connected();
        if (!logged_in_) {
            throw std::runtime_error("Not logged in. Use 'login' or 'anonymous' first.");
        }
    }
    
    void show_progress(size_t transferred, size_t total) {
        if (total > 0) {
            double percent = (static_cast<double>(transferred) / total) * 100.0;
            std::cout << "\rProgress: " << std::fixed << std::setprecision(1) 
                      << percent << "% (" << transferred << "/" << total << " bytes)" << std::flush;
        } else {
            std::cout << "\rTransferred: " << transferred << " bytes" << std::flush;
        }
    }
};

int main(int argc, char* argv[]) {
    try {
        std::cout << "Starting NetworkQuests FTP Client...\n";
        
        FtpClientDemo demo;
        demo.run();
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}