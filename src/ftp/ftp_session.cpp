#include "networkquests/ftp.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>

namespace NetworkQuests::Ftp {

// FtpSession implementation
FtpSession::FtpSession(std::unique_ptr<TcpSocket> socket, 
                       const std::unordered_map<std::string, FtpUser>& users,
                       const std::string& root_directory)
    : control_socket_(std::move(socket)), users_(users), root_directory_(root_directory) {
    
    if (control_socket_) {
        client_address_ = control_socket_->get_peer_address();
    }
    
    current_directory_ = "/";
}

FtpSession::~FtpSession() {
    close();
}

void FtpSession::run() {
    if (!control_socket_) {
        return;
    }
    
    // Send welcome message
    FtpResponse welcome(FtpResponseCode::SERVICE_READY, "NetworkQuests FTP Server ready");
    send_response(welcome);
    
    // Process commands
    process_commands();
}

void FtpSession::close() {
    active_ = false;
    data_connection_.close();
    
    if (control_socket_) {
        control_socket_->close();
        control_socket_.reset();
    }
}

void FtpSession::process_commands() {
    std::string command_line;
    std::vector<uint8_t> buffer(1);
    
    while (active_ && control_socket_) {
        try {
            command_line.clear();
            
            // Read command line character by character until CRLF
            while (true) {
                auto result = control_socket_->receive(buffer);
                if (!result) {
                    // Client disconnected
                    return;
                }
                
                auto data = *result;
                if (data.empty()) {
                    // Connection closed
                    return;
                }
                
                char ch = static_cast<char>(data[0]);
                if (ch == '\r') {
                    continue; // Skip CR
                } else if (ch == '\n') {
                    break; // End of command
                } else {
                    command_line += ch;
                }
                
                if (command_line.length() > FTP_MAX_COMMAND_LENGTH) {
                    FtpResponse error(FtpResponseCode::SYNTAX_ERROR, "Command line too long");
                    send_response(error);
                    command_line.clear();
                    break;
                }
            }
            
            if (command_line.empty()) {
                continue;
            }
            
            // Parse and handle command
            auto command_result = FtpCommand::parse(command_line);
            if (!command_result) {
                FtpResponse error(FtpResponseCode::SYNTAX_ERROR, command_result.error().message());
                send_response(error);
                continue;
            }
            
            auto command = *command_result;
            auto handle_result = handle_command(command);
            if (!handle_result) {
                FtpResponse error(FtpResponseCode::LOCAL_ERROR, handle_result.error().message());
                send_response(error);
            }
            
        } catch (const std::exception& e) {
            FtpResponse error(FtpResponseCode::LOCAL_ERROR, "Internal server error");
            send_response(error);
            break;
        }
    }
}

Result<void> FtpSession::handle_command(const FtpCommand& command) {
    std::string cmd = command.get_command();
    std::string args = command.get_arguments();
    
    // Check authentication requirement
    if (command.requires_authentication() && !authenticated_) {
        FtpResponse error(FtpResponseCode::NOT_LOGGED_IN, "Please login with USER and PASS");
        return send_response(error);
    }
    
    // Check command permissions
    if (authenticated_ && !has_permission(cmd)) {
        FtpResponse error(FtpResponseCode::COMMAND_NOT_IMPLEMENTED, "Command not allowed");
        return send_response(error);
    }
    
    // Handle specific commands
    if (cmd == "USER") {
        return handle_user(args);
    } else if (cmd == "PASS") {
        return handle_pass(args);
    } else if (cmd == "QUIT") {
        return handle_quit();
    } else if (cmd == "TYPE") {
        return handle_type(args);
    } else if (cmd == "MODE") {
        return handle_mode(args);
    } else if (cmd == "STRU") {
        return handle_stru(args);
    } else if (cmd == "PORT") {
        return handle_port(args);
    } else if (cmd == "PASV") {
        return handle_pasv();
    } else if (cmd == "EPSV") {
        return handle_epsv(args);
    } else if (cmd == "PWD") {
        return handle_pwd();
    } else if (cmd == "CWD") {
        return handle_cwd(args);
    } else if (cmd == "CDUP") {
        return handle_cdup();
    } else if (cmd == "MKD") {
        return handle_mkd(args);
    } else if (cmd == "RMD") {
        return handle_rmd(args);
    } else if (cmd == "LIST") {
        return handle_list(args);
    } else if (cmd == "NLST") {
        return handle_nlst(args);
    } else if (cmd == "RETR") {
        return handle_retr(args);
    } else if (cmd == "STOR") {
        return handle_stor(args);
    } else if (cmd == "DELE") {
        return handle_dele(args);
    } else if (cmd == "RNFR") {
        return handle_rnfr(args);
    } else if (cmd == "RNTO") {
        return handle_rnto(args);
    } else if (cmd == "SIZE") {
        return handle_size(args);
    } else if (cmd == "MDTM") {
        return handle_mdtm(args);
    } else if (cmd == "SYST") {
        return handle_syst();
    } else if (cmd == "HELP") {
        return handle_help(args);
    } else if (cmd == "NOOP") {
        return handle_noop();
    } else if (cmd == "STAT") {
        return handle_stat(args);
    } else {
        FtpResponse error(FtpResponseCode::COMMAND_NOT_IMPLEMENTED, "Command not implemented");
        return send_response(error);
    }
}

Result<void> FtpSession::send_response(const FtpResponse& response) {
    if (!control_socket_) {
        return Error("Control socket not available");
    }
    
    std::string response_str = response.to_string() + "\r\n";
    auto send_result = control_socket_->send(Utils::string_to_bytes(response_str));
    
    return send_result;
}

// Authentication commands
Result<void> FtpSession::handle_user(const std::string& username) {
    if (username.empty()) {
        FtpResponse error(FtpResponseCode::SYNTAX_ERROR_PARAMS, "Username required");
        return send_response(error);
    }
    
    if (!Utils::is_valid_username(username)) {
        FtpResponse error(FtpResponseCode::SYNTAX_ERROR_PARAMS, "Invalid username");
        return send_response(error);
    }
    
    pending_username_ = username;
    authenticated_ = false;
    current_user_.clear();
    
    FtpResponse response(FtpResponseCode::USERNAME_OK, "User name okay, need password");
    return send_response(response);
}

Result<void> FtpSession::handle_pass(const std::string& password) {
    if (pending_username_.empty()) {
        FtpResponse error(FtpResponseCode::BAD_SEQUENCE, "USER command must precede PASS");
        return send_response(error);
    }
    
    auto user_it = users_.find(pending_username_);
    if (user_it == users_.end()) {
        FtpResponse error(FtpResponseCode::NOT_LOGGED_IN, "Login incorrect");
        return send_response(error);
    }
    
    const FtpUser& user = user_it->second;
    
    // Check password (simplified - in production, use proper hashing)
    if (user.password != password && !user.is_anonymous) {
        FtpResponse error(FtpResponseCode::NOT_LOGGED_IN, "Login incorrect");
        return send_response(error);
    }
    
    // Login successful
    authenticated_ = true;
    current_user_ = pending_username_;
    current_directory_ = user.permissions.root_directory;
    pending_username_.clear();
    
    FtpResponse response(FtpResponseCode::USER_LOGGED_IN, "User logged in, proceed");
    return send_response(response);
}

Result<void> FtpSession::handle_quit() {
    FtpResponse response(FtpResponseCode::SERVICE_CLOSING, "Goodbye");
    send_response(response);
    active_ = false;
    return {};
}

// Transfer mode commands
Result<void> FtpSession::handle_type(const std::string& type) {
    if (type.empty()) {
        FtpResponse error(FtpResponseCode::SYNTAX_ERROR_PARAMS, "TYPE requires argument");
        return send_response(error);
    }
    
    std::string upper_type = Utils::to_upper(type);
    if (upper_type == "A" || upper_type == "ASCII") {
        transfer_mode_ = FtpTransferMode::ASCII;
        data_connection_.set_transfer_mode(transfer_mode_);
    } else if (upper_type == "I" || upper_type == "IMAGE" || upper_type == "BINARY") {
        transfer_mode_ = FtpTransferMode::BINARY;
        data_connection_.set_transfer_mode(transfer_mode_);
    } else {
        FtpResponse error(FtpResponseCode::PARAMETER_NOT_IMPLEMENTED, "Unsupported TYPE");
        return send_response(error);
    }
    
    FtpResponse response(FtpResponseCode::COMMAND_OK, "TYPE set to " + upper_type);
    return send_response(response);
}

Result<void> FtpSession::handle_mode(const std::string& mode) {
    std::string upper_mode = Utils::to_upper(mode);
    if (upper_mode != "S" && upper_mode != "STREAM") {
        FtpResponse error(FtpResponseCode::PARAMETER_NOT_IMPLEMENTED, "Only STREAM mode supported");
        return send_response(error);
    }
    
    FtpResponse response(FtpResponseCode::COMMAND_OK, "MODE set to STREAM");
    return send_response(response);
}

Result<void> FtpSession::handle_stru(const std::string& structure) {
    std::string upper_stru = Utils::to_upper(structure);
    if (upper_stru != "F" && upper_stru != "FILE") {
        FtpResponse error(FtpResponseCode::PARAMETER_NOT_IMPLEMENTED, "Only FILE structure supported");
        return send_response(error);
    }
    
    FtpResponse response(FtpResponseCode::COMMAND_OK, "STRU set to FILE");
    return send_response(response);
}

// Data connection commands
Result<void> FtpSession::handle_port(const std::string& args) {
    if (args.empty()) {
        FtpResponse error(FtpResponseCode::SYNTAX_ERROR_PARAMS, "PORT requires arguments");
        return send_response(error);
    }
    
    auto address_result = parse_port_command(args);
    if (!address_result) {
        FtpResponse error(FtpResponseCode::SYNTAX_ERROR_PARAMS, "Invalid PORT format");
        return send_response(error);
    }
    
    auto [client_ip, client_port] = *address_result;
    
    auto setup_result = data_connection_.setup_active(client_ip, client_port);
    if (!setup_result) {
        FtpResponse error(FtpResponseCode::CANT_OPEN_DATA, setup_result.error().message());
        return send_response(error);
    }
    
    connection_mode_ = FtpConnectionMode::ACTIVE;
    
    FtpResponse response(FtpResponseCode::COMMAND_OK, "PORT command successful");
    return send_response(response);
}

Result<void> FtpSession::handle_pasv() {
    auto port_result = data_connection_.setup_passive();
    if (!port_result) {
        FtpResponse error(FtpResponseCode::CANT_OPEN_DATA, port_result.error().message());
        return send_response(error);
    }
    
    uint16_t data_port = *port_result;
    std::string server_ip = Utils::get_local_ip_address();
    
    connection_mode_ = FtpConnectionMode::PASSIVE;
    
    FtpResponse response = FtpResponse::entering_passive(server_ip, data_port);
    return send_response(response);
}

Result<void> FtpSession::handle_epsv(const std::string& args) {
    // Simplified EPSV - only support IPv4
    if (!args.empty() && args != "1") {
        FtpResponse error(FtpResponseCode::PARAMETER_NOT_IMPLEMENTED, "Only IPv4 supported");
        return send_response(error);
    }
    
    auto port_result = data_connection_.setup_passive();
    if (!port_result) {
        FtpResponse error(FtpResponseCode::CANT_OPEN_DATA, port_result.error().message());
        return send_response(error);
    }
    
    uint16_t data_port = *port_result;
    connection_mode_ = FtpConnectionMode::PASSIVE;
    
    FtpResponse response = FtpResponse::entering_epsv(data_port);
    return send_response(response);
}

// Directory commands
Result<void> FtpSession::handle_pwd() {
    FtpResponse response(FtpResponseCode::PATHNAME_CREATED, "\"" + current_directory_ + "\"");
    return send_response(response);
}

Result<void> FtpSession::handle_cwd(const std::string& path) {
    if (path.empty()) {
        FtpResponse error(FtpResponseCode::SYNTAX_ERROR_PARAMS, "CWD requires path argument");
        return send_response(error);
    }
    
    std::string target_path = resolve_path(path);
    if (!is_path_allowed(target_path)) {
        FtpResponse error(FtpResponseCode::FILE_ACTION_NOT_TAKEN, "Access denied");
        return send_response(error);
    }
    
    if (!std::filesystem::exists(target_path) || !std::filesystem::is_directory(target_path)) {
        FtpResponse error(FtpResponseCode::FILE_ACTION_NOT_TAKEN, "Directory does not exist");
        return send_response(error);
    }
    
    current_directory_ = get_relative_path(target_path);
    
    FtpResponse response(FtpResponseCode::FILE_ACTION_OK, "Directory changed to " + current_directory_);
    return send_response(response);
}

Result<void> FtpSession::handle_cdup() {
    if (current_directory_ == "/") {
        FtpResponse response(FtpResponseCode::FILE_ACTION_OK, "Already in root directory");
        return send_response(response);
    }
    
    size_t last_slash = current_directory_.find_last_of('/');
    if (last_slash != std::string::npos && last_slash > 0) {
        current_directory_ = current_directory_.substr(0, last_slash);
    } else {
        current_directory_ = "/";
    }
    
    FtpResponse response(FtpResponseCode::FILE_ACTION_OK, "Directory changed to " + current_directory_);
    return send_response(response);
}

Result<void> FtpSession::handle_mkd(const std::string& path) {
    if (path.empty()) {
        FtpResponse error(FtpResponseCode::SYNTAX_ERROR_PARAMS, "MKD requires path argument");
        return send_response(error);
    }
    
    if (!has_permission("create_dirs")) {
        FtpResponse error(FtpResponseCode::FILE_ACTION_NOT_TAKEN, "Permission denied");
        return send_response(error);
    }
    
    std::string target_path = resolve_path(path);
    if (!is_path_allowed(target_path)) {
        FtpResponse error(FtpResponseCode::FILE_ACTION_NOT_TAKEN, "Access denied");
        return send_response(error);
    }
    
    try {
        if (std::filesystem::create_directories(target_path)) {
            FtpResponse response(FtpResponseCode::PATHNAME_CREATED, "\"" + get_relative_path(target_path) + "\" directory created");
            return send_response(response);
        } else {
            FtpResponse error(FtpResponseCode::FILE_ACTION_NOT_TAKEN, "Directory already exists");
            return send_response(error);
        }
    } catch (const std::exception& e) {
        FtpResponse error(FtpResponseCode::FILE_ACTION_NOT_TAKEN, "Failed to create directory");
        return send_response(error);
    }
}

Result<void> FtpSession::handle_rmd(const std::string& path) {
    if (path.empty()) {
        FtpResponse error(FtpResponseCode::SYNTAX_ERROR_PARAMS, "RMD requires path argument");
        return send_response(error);
    }
    
    if (!has_permission("delete")) {
        FtpResponse error(FtpResponseCode::FILE_ACTION_NOT_TAKEN, "Permission denied");
        return send_response(error);
    }
    
    std::string target_path = resolve_path(path);
    if (!is_path_allowed(target_path)) {
        FtpResponse error(FtpResponseCode::FILE_ACTION_NOT_TAKEN, "Access denied");
        return send_response(error);
    }
    
    try {
        if (std::filesystem::remove(target_path)) {
            FtpResponse response(FtpResponseCode::FILE_ACTION_OK, "Directory removed");
            return send_response(response);
        } else {
            FtpResponse error(FtpResponseCode::FILE_ACTION_NOT_TAKEN, "Directory does not exist or not empty");
            return send_response(error);
        }
    } catch (const std::exception& e) {
        FtpResponse error(FtpResponseCode::FILE_ACTION_NOT_TAKEN, "Failed to remove directory");
        return send_response(error);
    }
}

Result<void> FtpSession::handle_list(const std::string& path) {
    if (!has_permission("list")) {
        FtpResponse error(FtpResponseCode::FILE_ACTION_NOT_TAKEN, "Permission denied");
        return send_response(error);
    }
    
    std::string target_path = path.empty() ? resolve_path(".") : resolve_path(path);
    if (!is_path_allowed(target_path)) {
        FtpResponse error(FtpResponseCode::FILE_ACTION_NOT_TAKEN, "Access denied");
        return send_response(error);
    }
    
    // Check if data connection is ready
    if (connection_mode_ == FtpConnectionMode::PASSIVE && !data_connection_.is_connected()) {
        FtpResponse error(FtpResponseCode::CANT_OPEN_DATA, "No data connection");
        return send_response(error);
    }
    
    // Send preliminary response
    FtpResponse prelim = FtpResponse::directory_listing();
    auto send_result = send_response(prelim);
    if (!send_result) {
        return send_result.error();
    }
    
    // Accept connection if needed
    if (connection_mode_ == FtpConnectionMode::PASSIVE) {
        auto accept_result = data_connection_.accept_passive_connection();
        if (!accept_result) {
            FtpResponse error(FtpResponseCode::CANT_OPEN_DATA, accept_result.error().message());
            return send_response(error);
        }
    } else {
        auto connect_result = data_connection_.connect();
        if (!connect_result) {
            FtpResponse error(FtpResponseCode::CANT_OPEN_DATA, connect_result.error().message());
            return send_response(error);
        }
    }
    
    // Generate and send directory listing
    std::string listing = generate_directory_listing(target_path);
    auto transfer_result = data_connection_.send_directory_listing(listing);
    data_connection_.close();
    
    if (!transfer_result) {
        FtpResponse error(FtpResponseCode::CONNECTION_CLOSED, transfer_result.error().message());
        return send_response(error);
    }
    
    FtpResponse success(FtpResponseCode::DATA_CONNECTION_CLOSED, "Transfer complete");
    return send_response(success);
}

Result<void> FtpSession::handle_nlst(const std::string& path) {
    if (!has_permission("list")) {
        FtpResponse error(FtpResponseCode::FILE_ACTION_NOT_TAKEN, "Permission denied");
        return send_response(error);
    }
    
    std::string target_path = path.empty() ? resolve_path(".") : resolve_path(path);
    if (!is_path_allowed(target_path)) {
        FtpResponse error(FtpResponseCode::FILE_ACTION_NOT_TAKEN, "Access denied");
        return send_response(error);
    }
    
    // Check if data connection is ready
    if (connection_mode_ == FtpConnectionMode::PASSIVE && !data_connection_.is_connected()) {
        FtpResponse error(FtpResponseCode::CANT_OPEN_DATA, "No data connection");
        return send_response(error);
    }
    
    // Send preliminary response
    FtpResponse prelim(FtpResponseCode::FILE_STATUS_OK, "Opening data connection for name list");
    auto send_result = send_response(prelim);
    if (!send_result) {
        return send_result.error();
    }
    
    // Accept connection if needed
    if (connection_mode_ == FtpConnectionMode::PASSIVE) {
        auto accept_result = data_connection_.accept_passive_connection();
        if (!accept_result) {
            FtpResponse error(FtpResponseCode::CANT_OPEN_DATA, accept_result.error().message());
            return send_response(error);
        }
    } else {
        auto connect_result = data_connection_.connect();
        if (!connect_result) {
            FtpResponse error(FtpResponseCode::CANT_OPEN_DATA, connect_result.error().message());
            return send_response(error);
        }
    }
    
    // Generate and send name list
    std::string listing = generate_name_list(target_path);
    auto transfer_result = data_connection_.send_directory_listing(listing);
    data_connection_.close();
    
    if (!transfer_result) {
        FtpResponse error(FtpResponseCode::CONNECTION_CLOSED, transfer_result.error().message());
        return send_response(error);
    }
    
    FtpResponse success(FtpResponseCode::DATA_CONNECTION_CLOSED, "Transfer complete");
    return send_response(success);
}

// File commands
Result<void> FtpSession::handle_retr(const std::string& filename) {
    if (filename.empty()) {
        FtpResponse error(FtpResponseCode::SYNTAX_ERROR_PARAMS, "RETR requires filename");
        return send_response(error);
    }
    
    if (!has_permission("read")) {
        FtpResponse error(FtpResponseCode::FILE_ACTION_NOT_TAKEN, "Permission denied");
        return send_response(error);
    }
    
    std::string file_path = resolve_path(filename);
    if (!is_path_allowed(file_path)) {
        FtpResponse error(FtpResponseCode::FILE_ACTION_NOT_TAKEN, "Access denied");
        return send_response(error);
    }
    
    if (!std::filesystem::exists(file_path) || !std::filesystem::is_regular_file(file_path)) {
        FtpResponse error(FtpResponseCode::FILE_ACTION_NOT_TAKEN, "File not found");
        return send_response(error);
    }
    
    // Send preliminary response
    auto size_result = Utils::get_file_size(file_path);
    std::string file_info = filename;
    if (size_result) {
        file_info += " (" + std::to_string(*size_result) + " bytes)";
    }
    
    FtpResponse prelim(FtpResponseCode::FILE_STATUS_OK, "Opening data connection for " + file_info);
    auto send_result = send_response(prelim);
    if (!send_result) {
        return send_result.error();
    }
    
    // Accept connection if needed
    if (connection_mode_ == FtpConnectionMode::PASSIVE) {
        auto accept_result = data_connection_.accept_passive_connection();
        if (!accept_result) {
            FtpResponse error(FtpResponseCode::CANT_OPEN_DATA, accept_result.error().message());
            return send_response(error);
        }
    } else {
        auto connect_result = data_connection_.connect();
        if (!connect_result) {
            FtpResponse error(FtpResponseCode::CANT_OPEN_DATA, connect_result.error().message());
            return send_response(error);
        }
    }
    
    // Send file
    auto transfer_result = data_connection_.send_file(file_path);
    data_connection_.close();
    
    if (!transfer_result) {
        FtpResponse error(FtpResponseCode::CONNECTION_CLOSED, transfer_result.error().message());
        return send_response(error);
    }
    
    FtpResponse success(FtpResponseCode::DATA_CONNECTION_CLOSED, "Transfer complete");
    return send_response(success);
}

Result<void> FtpSession::handle_stor(const std::string& filename) {
    if (filename.empty()) {
        FtpResponse error(FtpResponseCode::SYNTAX_ERROR_PARAMS, "STOR requires filename");
        return send_response(error);
    }
    
    if (!has_permission("write")) {
        FtpResponse error(FtpResponseCode::FILE_ACTION_NOT_TAKEN, "Permission denied");
        return send_response(error);
    }
    
    if (!Utils::is_valid_filename(filename)) {
        FtpResponse error(FtpResponseCode::SYNTAX_ERROR_PARAMS, "Invalid filename");
        return send_response(error);
    }
    
    std::string file_path = resolve_path(filename);
    if (!is_path_allowed(file_path)) {
        FtpResponse error(FtpResponseCode::FILE_ACTION_NOT_TAKEN, "Access denied");
        return send_response(error);
    }
    
    // Send preliminary response
    FtpResponse prelim(FtpResponseCode::FILE_STATUS_OK, "Opening data connection for " + filename);
    auto send_result = send_response(prelim);
    if (!send_result) {
        return send_result.error();
    }
    
    // Accept connection if needed
    if (connection_mode_ == FtpConnectionMode::PASSIVE) {
        auto accept_result = data_connection_.accept_passive_connection();
        if (!accept_result) {
            FtpResponse error(FtpResponseCode::CANT_OPEN_DATA, accept_result.error().message());
            return send_response(error);
        }
    } else {
        auto connect_result = data_connection_.connect();
        if (!connect_result) {
            FtpResponse error(FtpResponseCode::CANT_OPEN_DATA, connect_result.error().message());
            return send_response(error);
        }
    }
    
    // Receive file
    auto transfer_result = data_connection_.receive_file(file_path);
    data_connection_.close();
    
    if (!transfer_result) {
        FtpResponse error(FtpResponseCode::CONNECTION_CLOSED, transfer_result.error().message());
        return send_response(error);
    }
    
    FtpResponse success(FtpResponseCode::DATA_CONNECTION_CLOSED, "Transfer complete");
    return send_response(success);
}

Result<void> FtpSession::handle_dele(const std::string& filename) {
    if (filename.empty()) {
        FtpResponse error(FtpResponseCode::SYNTAX_ERROR_PARAMS, "DELE requires filename");
        return send_response(error);
    }
    
    if (!has_permission("delete")) {
        FtpResponse error(FtpResponseCode::FILE_ACTION_NOT_TAKEN, "Permission denied");
        return send_response(error);
    }
    
    std::string file_path = resolve_path(filename);
    if (!is_path_allowed(file_path)) {
        FtpResponse error(FtpResponseCode::FILE_ACTION_NOT_TAKEN, "Access denied");
        return send_response(error);
    }
    
    try {
        if (std::filesystem::remove(file_path)) {
            FtpResponse response(FtpResponseCode::FILE_ACTION_OK, "File deleted");
            return send_response(response);
        } else {
            FtpResponse error(FtpResponseCode::FILE_ACTION_NOT_TAKEN, "File not found");
            return send_response(error);
        }
    } catch (const std::exception& e) {
        FtpResponse error(FtpResponseCode::FILE_ACTION_NOT_TAKEN, "Failed to delete file");
        return send_response(error);
    }
}

Result<void> FtpSession::handle_rnfr(const std::string& filename) {
    if (filename.empty()) {
        FtpResponse error(FtpResponseCode::SYNTAX_ERROR_PARAMS, "RNFR requires filename");
        return send_response(error);
    }
    
    if (!has_permission("rename")) {
        FtpResponse error(FtpResponseCode::FILE_ACTION_NOT_TAKEN, "Permission denied");
        return send_response(error);
    }
    
    std::string file_path = resolve_path(filename);
    if (!is_path_allowed(file_path)) {
        FtpResponse error(FtpResponseCode::FILE_ACTION_NOT_TAKEN, "Access denied");
        return send_response(error);
    }
    
    if (!std::filesystem::exists(file_path)) {
        FtpResponse error(FtpResponseCode::FILE_ACTION_NOT_TAKEN, "File not found");
        return send_response(error);
    }
    
    rename_from_path_ = file_path;
    
    FtpResponse response(FtpResponseCode::FILE_ACTION_PENDING, "File exists, ready for destination name");
    return send_response(response);
}

Result<void> FtpSession::handle_rnto(const std::string& filename) {
    if (filename.empty()) {
        FtpResponse error(FtpResponseCode::SYNTAX_ERROR_PARAMS, "RNTO requires filename");
        return send_response(error);
    }
    
    if (rename_from_path_.empty()) {
        FtpResponse error(FtpResponseCode::BAD_SEQUENCE, "RNFR command must precede RNTO");
        return send_response(error);
    }
    
    if (!Utils::is_valid_filename(filename)) {
        FtpResponse error(FtpResponseCode::SYNTAX_ERROR_PARAMS, "Invalid filename");
        return send_response(error);
    }
    
    std::string to_path = resolve_path(filename);
    if (!is_path_allowed(to_path)) {
        FtpResponse error(FtpResponseCode::FILE_ACTION_NOT_TAKEN, "Access denied");
        return send_response(error);
    }
    
    try {
        std::filesystem::rename(rename_from_path_, to_path);
        rename_from_path_.clear();
        
        FtpResponse response(FtpResponseCode::FILE_ACTION_OK, "File renamed");
        return send_response(response);
    } catch (const std::exception& e) {
        rename_from_path_.clear();
        FtpResponse error(FtpResponseCode::FILE_ACTION_NOT_TAKEN, "Failed to rename file");
        return send_response(error);
    }
}

Result<void> FtpSession::handle_size(const std::string& filename) {
    if (filename.empty()) {
        FtpResponse error(FtpResponseCode::SYNTAX_ERROR_PARAMS, "SIZE requires filename");
        return send_response(error);
    }
    
    std::string file_path = resolve_path(filename);
    if (!is_path_allowed(file_path)) {
        FtpResponse error(FtpResponseCode::FILE_ACTION_NOT_TAKEN, "Access denied");
        return send_response(error);
    }
    
    auto size_result = Utils::get_file_size(file_path);
    if (!size_result) {
        FtpResponse error(FtpResponseCode::FILE_ACTION_NOT_TAKEN, size_result.error().message());
        return send_response(error);
    }
    
    FtpResponse response(FtpResponseCode::FILE_STATUS, std::to_string(*size_result));
    return send_response(response);
}

Result<void> FtpSession::handle_mdtm(const std::string& filename) {
    if (filename.empty()) {
        FtpResponse error(FtpResponseCode::SYNTAX_ERROR_PARAMS, "MDTM requires filename");
        return send_response(error);
    }
    
    std::string file_path = resolve_path(filename);
    if (!is_path_allowed(file_path)) {
        FtpResponse error(FtpResponseCode::FILE_ACTION_NOT_TAKEN, "Access denied");
        return send_response(error);
    }
    
    try {
        if (!std::filesystem::exists(file_path)) {
            FtpResponse error(FtpResponseCode::FILE_ACTION_NOT_TAKEN, "File not found");
            return send_response(error);
        }
        
        auto ftime = std::filesystem::last_write_time(file_path);
        std::string time_str = Utils::format_ftp_time(ftime);
        
        FtpResponse response(FtpResponseCode::FILE_STATUS, time_str);
        return send_response(response);
    } catch (const std::exception& e) {
        FtpResponse error(FtpResponseCode::FILE_ACTION_NOT_TAKEN, "Failed to get file time");
        return send_response(error);
    }
}

// System commands
Result<void> FtpSession::handle_syst() {
    FtpResponse response = FtpResponse::system_type();
    return send_response(response);
}

Result<void> FtpSession::handle_help(const std::string& command) {
    if (command.empty()) {
        auto commands = Utils::get_supported_commands();
        std::string help_text = "The following commands are supported:\n";
        for (const auto& cmd : commands) {
            help_text += " " + cmd;
        }
        
        FtpResponse response(FtpResponseCode::HELP_MESSAGE, help_text);
        return send_response(response);
    } else {
        std::string upper_cmd = Utils::to_upper(command);
        if (Utils::is_ftp_command(upper_cmd)) {
            FtpResponse response(FtpResponseCode::HELP_MESSAGE, "Command " + upper_cmd + " is supported");
            return send_response(response);
        } else {
            FtpResponse error(FtpResponseCode::COMMAND_NOT_IMPLEMENTED, "Command " + upper_cmd + " not implemented");
            return send_response(error);
        }
    }
}

Result<void> FtpSession::handle_noop() {
    FtpResponse response(FtpResponseCode::COMMAND_OK, "NOOP command successful");
    return send_response(response);
}

Result<void> FtpSession::handle_stat(const std::string& path) {
    if (path.empty()) {
        // Return server status
        std::string status = "NetworkQuests FTP Server\n";
        status += "Connected to: " + client_address_ + "\n";
        status += "TYPE: " + (transfer_mode_ == FtpTransferMode::ASCII ? "ASCII" : "BINARY") + "\n";
        status += "STRUcture: FILE\n";
        status += "MODE: Stream\n";
        
        FtpResponse response(FtpResponseCode::SYSTEM_STATUS, status);
        return send_response(response);
    } else {
        // Return file/directory status (simplified)
        std::string target_path = resolve_path(path);
        if (!is_path_allowed(target_path)) {
            FtpResponse error(FtpResponseCode::FILE_ACTION_NOT_TAKEN, "Access denied");
            return send_response(error);
        }
        
        if (std::filesystem::exists(target_path)) {
            std::string info = std::filesystem::is_directory(target_path) ? "Directory" : "File";
            info += ": " + path;
            
            FtpResponse response(FtpResponseCode::FILE_STATUS, info);
            return send_response(response);
        } else {
            FtpResponse error(FtpResponseCode::FILE_ACTION_NOT_TAKEN, "File or directory not found");
            return send_response(error);
        }
    }
}

// Helper methods
std::string FtpSession::resolve_path(const std::string& path) const {
    std::string full_path;
    
    if (Utils::is_absolute_path(path)) {
        full_path = Utils::join_paths(root_directory_, path);
    } else {
        std::string current_full_path = Utils::join_paths(root_directory_, current_directory_);
        full_path = Utils::join_paths(current_full_path, path);
    }
    
    return Utils::normalize_path(full_path);
}

std::string FtpSession::get_relative_path(const std::string& full_path) const {
    std::string normalized_root = Utils::normalize_path(root_directory_);
    std::string normalized_path = Utils::normalize_path(full_path);
    
    if (normalized_path.substr(0, normalized_root.length()) == normalized_root) {
        std::string relative = normalized_path.substr(normalized_root.length());
        return relative.empty() ? "/" : relative;
    }
    
    return "/"; // Fallback to root if outside allowed area
}

bool FtpSession::is_path_allowed(const std::string& path) const {
    return Utils::is_safe_path(path, root_directory_) &&
           !Utils::is_directory_traversal_attempt(path);
}

bool FtpSession::has_permission(const std::string& operation) const {
    if (!authenticated_) {
        return false;
    }
    
    auto user_it = users_.find(current_user_);
    if (user_it == users_.end()) {
        return false;
    }
    
    const FtpUserPermissions& perms = user_it->second.permissions;
    
    if (operation == "read" || operation == "RETR") {
        return perms.can_read;
    } else if (operation == "write" || operation == "STOR") {
        return perms.can_write;
    } else if (operation == "delete" || operation == "DELE") {
        return perms.can_delete;
    } else if (operation == "create_dirs" || operation == "MKD") {
        return perms.can_create_dirs;
    } else if (operation == "list" || operation == "LIST" || operation == "NLST") {
        return perms.can_list;
    } else if (operation == "rename" || operation == "RNFR" || operation == "RNTO") {
        return perms.can_rename;
    }
    
    return perms.has_command_permission(operation);
}

std::string FtpSession::generate_directory_listing(const std::string& path) const {
    std::stringstream listing;
    
    try {
        for (const auto& entry : std::filesystem::directory_iterator(path)) {
            listing << format_file_info(entry) << "\r\n";
        }
    } catch (const std::exception& e) {
        // Return empty listing on error
    }
    
    return listing.str();
}

std::string FtpSession::generate_name_list(const std::string& path) const {
    std::stringstream listing;
    
    try {
        for (const auto& entry : std::filesystem::directory_iterator(path)) {
            listing << entry.path().filename().string() << "\r\n";
        }
    } catch (const std::exception& e) {
        // Return empty listing on error
    }
    
    return listing.str();
}

std::string FtpSession::format_file_info(const std::filesystem::directory_entry& entry) const {
    auto perms_result = Utils::get_file_permissions(entry.path());
    std::string perms = perms_result ? *perms_result : "----------";
    
    std::string size_str;
    if (entry.is_regular_file()) {
        auto size_result = Utils::get_file_size(entry.path());
        size_str = size_result ? std::to_string(*size_result) : "0";
    } else {
        size_str = "0";
    }
    
    std::string time_str;
    try {
        auto ftime = entry.last_write_time();
        time_str = Utils::format_ftp_time(ftime);
    } catch (...) {
        time_str = "Jan  1 00:00";
    }
    
    // Format: permissions links owner group size month day time name
    return perms + "   1 owner group " + 
           std::string(8 - size_str.length(), ' ') + size_str + " " +
           time_str + " " + entry.path().filename().string();
}

std::string FtpSession::format_modification_time(const std::filesystem::file_time_type& time) const {
    return Utils::format_ftp_time(time);
}

Result<std::pair<std::string, uint16_t>> FtpSession::parse_port_command(const std::string& args) const {
    return Utils::parse_passive_address(args);
}

std::string FtpSession::format_passive_response(const std::string& ip, uint16_t port) const {
    return Utils::format_passive_address(ip, port);
}

} // namespace NetworkQuests::Ftp