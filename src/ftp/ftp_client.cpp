#include "networkquests/ftp.hpp"
#include <random>
#include <sstream>

namespace NetworkQuests::Ftp {

// FtpClient implementation
FtpClient::~FtpClient() {
    disconnect();
}

Result<void> FtpClient::connect(const std::string& host, uint16_t port) {
    disconnect(); // Close any existing connection
    
    control_socket_ = std::make_unique<TcpSocket>();
    
    auto result = control_socket_->connect(host, port);
    if (!result) {
        control_socket_.reset();
        return result.error();
    }
    
    // Read welcome message
    auto welcome_response = read_response();
    if (!welcome_response) {
        disconnect();
        return welcome_response.error();
    }
    
    if (!welcome_response->is_success()) {
        disconnect();
        return Error("Server rejected connection: " + welcome_response->get_message());
    }
    
    return {};
}

void FtpClient::disconnect() {
    logged_in_ = false;
    data_connection_.close();
    
    if (control_socket_) {
        control_socket_->close();
        control_socket_.reset();
    }
}

Result<FtpResponse> FtpClient::login(const std::string& username, const std::string& password) {
    if (!is_connected()) {
        return Error("Not connected to server");
    }
    
    // Send USER command
    auto user_response = send_command("USER", username);
    if (!user_response) {
        return user_response.error();
    }
    
    if (user_response->get_code() == FtpResponseCode::USER_LOGGED_IN) {
        // No password required
        logged_in_ = true;
        return *user_response;
    }
    
    if (user_response->get_code() != FtpResponseCode::USERNAME_OK) {
        return Error("Username rejected: " + user_response->get_message());
    }
    
    // Send PASS command
    auto pass_response = send_command("PASS", password);
    if (!pass_response) {
        return pass_response.error();
    }
    
    if (pass_response->get_code() == FtpResponseCode::USER_LOGGED_IN) {
        logged_in_ = true;
    } else {
        return Error("Login failed: " + pass_response->get_message());
    }
    
    return *pass_response;
}

Result<FtpResponse> FtpClient::login_anonymous() {
    return login("anonymous", "anonymous@example.com");
}

Result<FtpResponse> FtpClient::logout() {
    auto result = quit();
    logged_in_ = false;
    return result;
}

void FtpClient::set_progress_callback(std::function<void(size_t, size_t)> callback) {
    progress_callback_ = callback;
    data_connection_.set_progress_callback(callback);
}

Result<FtpResponse> FtpClient::change_directory(const std::string& path) {
    if (!logged_in_) {
        return Error("Not logged in");
    }
    
    auto response = send_command("CWD", path);
    if (response && response->is_success()) {
        current_directory_ = path;
    }
    
    return response;
}

Result<FtpResponse> FtpClient::change_to_parent_directory() {
    if (!logged_in_) {
        return Error("Not logged in");
    }
    
    return send_command("CDUP");
}

Result<FtpResponse> FtpClient::print_working_directory() {
    if (!logged_in_) {
        return Error("Not logged in");
    }
    
    return send_command("PWD");
}

Result<FtpResponse> FtpClient::make_directory(const std::string& path) {
    if (!logged_in_) {
        return Error("Not logged in");
    }
    
    return send_command("MKD", path);
}

Result<FtpResponse> FtpClient::remove_directory(const std::string& path) {
    if (!logged_in_) {
        return Error("Not logged in");
    }
    
    return send_command("RMD", path);
}

Result<std::string> FtpClient::list_directory(const std::string& path) {
    if (!logged_in_) {
        return Error("Not logged in");
    }
    
    // Setup data connection
    auto setup_result = setup_data_connection();
    if (!setup_result) {
        return setup_result.error();
    }
    
    // Send LIST command
    auto response = send_command("LIST", path);
    if (!response || !response->is_positive_preliminary()) {
        data_connection_.close();
        return Error("LIST command failed: " + (response ? response->get_message() : "Unknown error"));
    }
    
    // Accept/connect data connection
    Result<void> connect_result;
    if (data_connection_.get_mode() == FtpConnectionMode::PASSIVE) {
        connect_result = data_connection_.accept_passive_connection();
    } else {
        connect_result = data_connection_.connect();
    }
    
    if (!connect_result) {
        data_connection_.close();
        return connect_result.error();
    }
    
    // Receive directory listing
    auto listing_result = data_connection_.receive_directory_listing();
    data_connection_.close();
    
    if (!listing_result) {
        return listing_result.error();
    }
    
    // Read final response
    auto final_response = read_response();
    if (!final_response || !final_response->is_success()) {
        return Error("Transfer failed: " + (final_response ? final_response->get_message() : "Unknown error"));
    }
    
    return *listing_result;
}

Result<std::vector<std::string>> FtpClient::name_list(const std::string& path) {
    if (!logged_in_) {
        return Error("Not logged in");
    }
    
    // Setup data connection
    auto setup_result = setup_data_connection();
    if (!setup_result) {
        return setup_result.error();
    }
    
    // Send NLST command
    auto response = send_command("NLST", path);
    if (!response || !response->is_positive_preliminary()) {
        data_connection_.close();
        return Error("NLST command failed: " + (response ? response->get_message() : "Unknown error"));
    }
    
    // Accept/connect data connection
    Result<void> connect_result;
    if (data_connection_.get_mode() == FtpConnectionMode::PASSIVE) {
        connect_result = data_connection_.accept_passive_connection();
    } else {
        connect_result = data_connection_.connect();
    }
    
    if (!connect_result) {
        data_connection_.close();
        return connect_result.error();
    }
    
    // Receive name list
    auto listing_result = data_connection_.receive_directory_listing();
    data_connection_.close();
    
    if (!listing_result) {
        return listing_result.error();
    }
    
    // Read final response
    auto final_response = read_response();
    if (!final_response || !final_response->is_success()) {
        return Error("Transfer failed: " + (final_response ? final_response->get_message() : "Unknown error"));
    }
    
    // Parse the list into individual names
    std::vector<std::string> names;
    std::stringstream ss(*listing_result);
    std::string line;
    
    while (std::getline(ss, line)) {
        line = Utils::trim(line);
        if (!line.empty()) {
            names.push_back(line);
        }
    }
    
    return names;
}

Result<FtpResponse> FtpClient::upload_file(const std::filesystem::path& local_path, 
                                          const std::string& remote_path) {
    if (!logged_in_) {
        return Error("Not logged in");
    }
    
    if (!std::filesystem::exists(local_path)) {
        return Error("Local file does not exist: " + local_path.string());
    }
    
    // Setup transfer mode
    auto mode_result = setup_transfer_mode();
    if (!mode_result) {
        return mode_result.error();
    }
    
    // Setup data connection
    auto setup_result = setup_data_connection();
    if (!setup_result) {
        return setup_result.error();
    }
    
    // Send STOR command
    std::string remote_filename = remote_path.empty() ? local_path.filename().string() : remote_path;
    auto response = send_command("STOR", remote_filename);
    if (!response || !response->is_positive_preliminary()) {
        data_connection_.close();
        return Error("STOR command failed: " + (response ? response->get_message() : "Unknown error"));
    }
    
    // Accept/connect data connection
    Result<void> connect_result;
    if (data_connection_.get_mode() == FtpConnectionMode::PASSIVE) {
        connect_result = data_connection_.accept_passive_connection();
    } else {
        connect_result = data_connection_.connect();
    }
    
    if (!connect_result) {
        data_connection_.close();
        return connect_result.error();
    }
    
    // Send file
    auto transfer_result = data_connection_.send_file(local_path);
    data_connection_.close();
    
    if (!transfer_result) {
        return transfer_result.error();
    }
    
    // Read final response
    auto final_response = read_response();
    if (!final_response || !final_response->is_success()) {
        return Error("Upload failed: " + (final_response ? final_response->get_message() : "Unknown error"));
    }
    
    return *final_response;
}

Result<FtpResponse> FtpClient::download_file(const std::string& remote_path, 
                                            const std::filesystem::path& local_path) {
    if (!logged_in_) {
        return Error("Not logged in");
    }
    
    // Setup transfer mode
    auto mode_result = setup_transfer_mode();
    if (!mode_result) {
        return mode_result.error();
    }
    
    // Setup data connection
    auto setup_result = setup_data_connection();
    if (!setup_result) {
        return setup_result.error();
    }
    
    // Send RETR command
    auto response = send_command("RETR", remote_path);
    if (!response || !response->is_positive_preliminary()) {
        data_connection_.close();
        return Error("RETR command failed: " + (response ? response->get_message() : "Unknown error"));
    }
    
    // Accept/connect data connection
    Result<void> connect_result;
    if (data_connection_.get_mode() == FtpConnectionMode::PASSIVE) {
        connect_result = data_connection_.accept_passive_connection();
    } else {
        connect_result = data_connection_.connect();
    }
    
    if (!connect_result) {
        data_connection_.close();
        return connect_result.error();
    }
    
    // Receive file
    std::filesystem::path target_path = local_path.empty() ? 
        std::filesystem::current_path() / std::filesystem::path(remote_path).filename() : 
        local_path;
    
    auto transfer_result = data_connection_.receive_file(target_path);
    data_connection_.close();
    
    if (!transfer_result) {
        return transfer_result.error();
    }
    
    // Read final response
    auto final_response = read_response();
    if (!final_response || !final_response->is_success()) {
        return Error("Download failed: " + (final_response ? final_response->get_message() : "Unknown error"));
    }
    
    return *final_response;
}

Result<FtpResponse> FtpClient::delete_file(const std::string& remote_path) {
    if (!logged_in_) {
        return Error("Not logged in");
    }
    
    return send_command("DELE", remote_path);
}

Result<FtpResponse> FtpClient::rename_file(const std::string& from_path, const std::string& to_path) {
    if (!logged_in_) {
        return Error("Not logged in");
    }
    
    // Send RNFR command
    auto rnfr_response = send_command("RNFR", from_path);
    if (!rnfr_response || !rnfr_response->is_positive_intermediate()) {
        return Error("RNFR command failed: " + (rnfr_response ? rnfr_response->get_message() : "Unknown error"));
    }
    
    // Send RNTO command
    return send_command("RNTO", to_path);
}

Result<FtpResponse> FtpClient::get_file_size(const std::string& remote_path) {
    if (!logged_in_) {
        return Error("Not logged in");
    }
    
    return send_command("SIZE", remote_path);
}

Result<FtpResponse> FtpClient::get_modification_time(const std::string& remote_path) {
    if (!logged_in_) {
        return Error("Not logged in");
    }
    
    return send_command("MDTM", remote_path);
}

Result<FtpResponse> FtpClient::system_type() {
    return send_command("SYST");
}

Result<FtpResponse> FtpClient::help(const std::string& command) {
    return send_command("HELP", command);
}

Result<FtpResponse> FtpClient::noop() {
    return send_command("NOOP");
}

Result<FtpResponse> FtpClient::quit() {
    auto response = send_command("QUIT");
    disconnect();
    return response;
}

Result<FtpResponse> FtpClient::send_command(const std::string& command, const std::string& args) {
    FtpCommand cmd(command, args);
    return send_command(cmd);
}

Result<FtpResponse> FtpClient::send_command(const FtpCommand& command) {
    if (!control_socket_) {
        return Error("Not connected to server");
    }
    
    std::string command_line = command.to_string() + "\r\n";
    auto send_result = control_socket_->send(Utils::string_to_bytes(command_line));
    if (!send_result) {
        return send_result.error();
    }
    
    return read_response();
}

Result<FtpResponse> FtpClient::read_response() {
    if (!control_socket_) {
        return Error("Not connected to server");
    }
    
    std::string response_line;
    std::vector<uint8_t> buffer(1);
    
    // Read response line by line until we get CRLF
    while (true) {
        auto result = control_socket_->receive(buffer, timeout_);
        if (!result) {
            return result.error();
        }
        
        auto data = *result;
        if (data.empty()) {
            return Error("Connection closed by server");
        }
        
        char ch = static_cast<char>(data[0]);
        if (ch == '\r') {
            continue; // Skip CR
        } else if (ch == '\n') {
            break; // End of line
        } else {
            response_line += ch;
        }
        
        if (response_line.length() > FTP_MAX_RESPONSE_LENGTH) {
            return Error("Response line too long");
        }
    }
    
    auto response = FtpResponse::parse(response_line);
    if (!response) {
        return response.error();
    }
    
    // Check if this is a multiline response
    if (response->is_multiline()) {
        return read_multiline_response();
    }
    
    return *response;
}

Result<FtpResponse> FtpClient::read_multiline_response() {
    std::vector<std::string> lines;
    
    // Read the first line again to get the initial response
    auto first_response = read_response();
    if (!first_response) {
        return first_response.error();
    }
    
    lines.push_back(first_response->to_string());
    
    // Read subsequent lines until we get the final line
    uint16_t expected_code = first_response->get_code_number();
    
    while (true) {
        auto response = read_response();
        if (!response) {
            return response.error();
        }
        
        lines.push_back(response->to_string());
        
        if (response->get_code_number() == expected_code && !response->is_multiline()) {
            break; // Final line
        }
    }
    
    return FtpResponse::parse_multiline(lines);
}

Result<void> FtpClient::setup_data_connection() {
    if (connection_mode_ == FtpConnectionMode::PASSIVE) {
        return setup_passive_mode();
    } else {
        return setup_active_mode();
    }
}

Result<void> FtpClient::setup_transfer_mode() {
    std::string mode_arg = (transfer_mode_ == FtpTransferMode::ASCII) ? "A" : "I";
    auto response = send_command("TYPE", mode_arg);
    
    if (!response || !response->is_success()) {
        return Error("Failed to set transfer mode: " + (response ? response->get_message() : "Unknown error"));
    }
    
    data_connection_.set_transfer_mode(transfer_mode_);
    return {};
}

Result<void> FtpClient::setup_passive_mode() {
    data_connection_.set_mode(FtpConnectionMode::PASSIVE);
    
    // Send PASV command
    auto response = send_command("PASV");
    if (!response || !response->is_success()) {
        return Error("PASV command failed: " + (response ? response->get_message() : "Unknown error"));
    }
    
    // Parse the passive response to get IP and port
    std::string message = response->get_message();
    size_t start = message.find('(');
    size_t end = message.find(')');
    
    if (start == std::string::npos || end == std::string::npos) {
        return Error("Invalid PASV response format");
    }
    
    std::string address_part = message.substr(start + 1, end - start - 1);
    auto address_result = Utils::parse_passive_address(address_part);
    if (!address_result) {
        return address_result.error();
    }
    
    auto [server_ip, server_port] = *address_result;
    
    // Setup passive connection to server
    auto setup_result = data_connection_.setup_passive(server_ip);
    if (!setup_result) {
        return setup_result.error();
    }
    
    return {};
}

Result<void> FtpClient::setup_active_mode() {
    data_connection_.set_mode(FtpConnectionMode::ACTIVE);
    
    std::string local_ip = get_local_ip();
    uint16_t local_port = allocate_data_port();
    
    // Setup active connection
    auto setup_result = data_connection_.setup_active(local_ip, local_port);
    if (!setup_result) {
        return setup_result.error();
    }
    
    // Send PORT command
    std::string port_arg = Utils::format_passive_address(local_ip, local_port);
    auto response = send_command("PORT", port_arg);
    
    if (!response || !response->is_success()) {
        return Error("PORT command failed: " + (response ? response->get_message() : "Unknown error"));
    }
    
    return {};
}

std::string FtpClient::get_local_ip() const {
    return Utils::get_local_ip_address();
}

uint16_t FtpClient::allocate_data_port() const {
    // Allocate a random high port for data connection
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint16_t> dis(10000, 65535);
    
    uint16_t port;
    do {
        port = dis(gen);
    } while (!Utils::is_valid_port(port));
    
    return port;
}

} // namespace NetworkQuests::Ftp