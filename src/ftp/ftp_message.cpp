#include "networkquests/ftp.hpp"
#include <algorithm>
#include <sstream>

namespace NetworkQuests::Ftp {

// FtpUserPermissions implementation
bool FtpUserPermissions::has_command_permission(const std::string& command) const {
    if (allowed_commands.empty()) {
        return true; // No restrictions if list is empty
    }
    
    std::string upper_cmd = Utils::to_upper(command);
    return std::find(allowed_commands.begin(), allowed_commands.end(), upper_cmd) != allowed_commands.end();
}

// FtpUser implementation
FtpUser::FtpUser(const std::string& user, const std::string& pass, 
                 const FtpUserPermissions& perms, bool anon)
    : username(user), password(pass), permissions(perms), is_anonymous(anon) {
    last_login = std::chrono::system_clock::now();
}

// FtpCommand implementation
FtpCommand::FtpCommand(const std::string& cmd, const std::string& args)
    : command_(cmd), arguments_(args) {
    normalize_command();
    raw_command_ = command_ + (arguments_.empty() ? "" : " " + arguments_);
}

Result<FtpCommand> FtpCommand::parse(const std::string& line) {
    std::string trimmed = Utils::trim(line);
    if (trimmed.empty()) {
        return Error("Empty command line");
    }
    
    if (trimmed.length() > FTP_MAX_COMMAND_LENGTH) {
        return Error("Command line too long");
    }
    
    // Find the first space to separate command from arguments
    size_t space_pos = trimmed.find(' ');
    
    std::string command;
    std::string arguments;
    
    if (space_pos == std::string::npos) {
        command = trimmed;
    } else {
        command = trimmed.substr(0, space_pos);
        arguments = Utils::trim(trimmed.substr(space_pos + 1));
    }
    
    if (command.empty()) {
        return Error("Missing command");
    }
    
    FtpCommand cmd(command, arguments);
    cmd.raw_command_ = trimmed;
    
    if (!cmd.is_valid()) {
        return Error("Invalid FTP command: " + command);
    }
    
    return cmd;
}

std::string FtpCommand::to_string() const {
    return raw_command_;
}

bool FtpCommand::is_valid() const {
    return Utils::is_ftp_command(command_);
}

bool FtpCommand::requires_data_connection() const {
    static const std::vector<std::string> data_commands = {
        "RETR", "STOR", "STOU", "APPE", "LIST", "NLST"
    };
    
    std::string upper_cmd = Utils::to_upper(command_);
    return std::find(data_commands.begin(), data_commands.end(), upper_cmd) != data_commands.end();
}

bool FtpCommand::requires_authentication() const {
    static const std::vector<std::string> unauth_commands = {
        "USER", "PASS", "QUIT", "HELP", "NOOP", "SYST"
    };
    
    std::string upper_cmd = Utils::to_upper(command_);
    return std::find(unauth_commands.begin(), unauth_commands.end(), upper_cmd) == unauth_commands.end();
}

void FtpCommand::normalize_command() {
    command_ = Utils::to_upper(Utils::trim(command_));
}

// FtpResponse implementation
FtpResponse::FtpResponse(FtpResponseCode code, const std::string& message)
    : code_(code), message_(message.empty() ? get_default_message(code) : message) {
}

FtpResponse::FtpResponse(uint16_t code, const std::string& message)
    : code_(static_cast<FtpResponseCode>(code))
    , message_(message.empty() ? get_default_message(code_) : message) {
}

std::string FtpResponse::to_string() const {
    if (multiline_) {
        return std::to_string(get_code_number()) + "-" + message_;
    } else {
        return std::to_string(get_code_number()) + " " + message_;
    }
}

std::vector<std::string> FtpResponse::to_lines() const {
    std::vector<std::string> lines;
    
    if (multiline_) {
        // Split message into lines
        std::stringstream ss(message_);
        std::string line;
        bool first_line = true;
        
        while (std::getline(ss, line)) {
            if (first_line) {
                lines.push_back(std::to_string(get_code_number()) + "-" + line);
                first_line = false;
            } else {
                lines.push_back(" " + line);
            }
        }
        
        // Add the final line with the code
        lines.push_back(std::to_string(get_code_number()) + " End");
    } else {
        lines.push_back(to_string());
    }
    
    return lines;
}

Result<FtpResponse> FtpResponse::parse(const std::string& line) {
    std::string trimmed = Utils::trim(line);
    if (trimmed.length() < 4) {
        return Error("Invalid response format: too short");
    }
    
    // Parse response code
    std::string code_str = trimmed.substr(0, 3);
    if (!std::all_of(code_str.begin(), code_str.end(), ::isdigit)) {
        return Error("Invalid response code format");
    }
    
    uint16_t code = std::stoi(code_str);
    
    // Check separator
    char separator = trimmed[3];
    if (separator != ' ' && separator != '-') {
        return Error("Invalid response separator");
    }
    
    std::string message = trimmed.length() > 4 ? trimmed.substr(4) : "";
    
    FtpResponse response(code, message);
    response.set_multiline(separator == '-');
    
    return response;
}

Result<FtpResponse> FtpResponse::parse_multiline(const std::vector<std::string>& lines) {
    if (lines.empty()) {
        return Error("Empty response lines");
    }
    
    // Parse the first line
    auto first_response = parse(lines[0]);
    if (!first_response) {
        return first_response.error();
    }
    
    if (!first_response->is_multiline()) {
        return *first_response;
    }
    
    // Collect all message lines
    std::string full_message = first_response->get_message();
    
    // Process middle lines
    for (size_t i = 1; i < lines.size() - 1; ++i) {
        std::string line = Utils::trim(lines[i]);
        if (line.length() > 0 && line[0] == ' ') {
            full_message += "\n" + line.substr(1);
        } else {
            full_message += "\n" + line;
        }
    }
    
    // Parse the last line to get the final code
    if (lines.size() > 1) {
        auto last_response = parse(lines.back());
        if (!last_response || last_response->get_code() != first_response->get_code()) {
            return Error("Multiline response code mismatch");
        }
    }
    
    FtpResponse response(first_response->get_code(), full_message);
    response.set_multiline(false); // Final response is not multiline
    
    return response;
}

FtpResponse FtpResponse::ok(const std::string& message) {
    return FtpResponse(FtpResponseCode::COMMAND_OK, message);
}

FtpResponse FtpResponse::error(FtpResponseCode code, const std::string& message) {
    return FtpResponse(code, message);
}

FtpResponse FtpResponse::system_type(const std::string& system_name) {
    return FtpResponse(FtpResponseCode::SYSTEM_TYPE, system_name);
}

FtpResponse FtpResponse::entering_passive(const std::string& ip, uint16_t port) {
    std::string address = Utils::format_passive_address(ip, port);
    return FtpResponse(FtpResponseCode::ENTERING_PASSIVE, 
                      "Entering Passive Mode (" + address + ")");
}

FtpResponse FtpResponse::entering_epsv(uint16_t port) {
    return FtpResponse(FtpResponseCode::ENTERING_EPSV,
                      "Entering Extended Passive Mode (|||" + std::to_string(port) + "|)");
}

FtpResponse FtpResponse::file_status(const std::string& filename, size_t size) {
    return FtpResponse(FtpResponseCode::FILE_STATUS,
                      filename + " (" + std::to_string(size) + " bytes)");
}

FtpResponse FtpResponse::directory_listing() {
    return FtpResponse(FtpResponseCode::FILE_STATUS_OK,
                      "Here comes the directory listing");
}

std::string FtpResponse::get_default_message(FtpResponseCode code) {
    return Utils::get_response_text(code);
}

} // namespace NetworkQuests::Ftp