#include "networkquests/ftp.hpp"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <regex>
#include <sstream>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
#else
    #include <ifaddrs.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <sys/socket.h>
    #include <unistd.h>
#endif

namespace NetworkQuests::Ftp::Utils {

// Path operations
std::string normalize_path(const std::string& path) {
    if (path.empty()) {
        return "/";
    }
    
    std::string normalized = path;
    
    // Convert backslashes to forward slashes
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    
    // Remove duplicate slashes
    auto new_end = std::unique(normalized.begin(), normalized.end(),
                              [](char a, char b) { return a == '/' && b == '/'; });
    normalized.erase(new_end, normalized.end());
    
    // Ensure it starts with /
    if (normalized.front() != '/') {
        normalized = "/" + normalized;
    }
    
    // Remove trailing slash unless it's root
    if (normalized.length() > 1 && normalized.back() == '/') {
        normalized.pop_back();
    }
    
    return normalized;
}

std::string join_paths(const std::string& base, const std::string& relative) {
    if (relative.empty()) {
        return normalize_path(base);
    }
    
    if (is_absolute_path(relative)) {
        return normalize_path(relative);
    }
    
    std::string result = base;
    if (!result.empty() && result.back() != '/') {
        result += "/";
    }
    result += relative;
    
    return normalize_path(result);
}

bool is_absolute_path(const std::string& path) {
    return !path.empty() && path.front() == '/';
}

bool is_safe_path(const std::string& path, const std::string& root) {
    std::string normalized_path = normalize_path(path);
    std::string normalized_root = normalize_path(root);
    
    // Check if the path tries to escape the root directory
    return normalized_path.substr(0, normalized_root.length()) == normalized_root;
}

// File operations
Result<std::vector<uint8_t>> read_file(const std::filesystem::path& path) {
    try {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
            return Error("Cannot open file: " + path.string());
        }
        
        file.seekg(0, std::ios::end);
        size_t size = file.tellg();
        file.seekg(0, std::ios::beg);
        
        std::vector<uint8_t> data(size);
        file.read(reinterpret_cast<char*>(data.data()), size);
        
        if (!file) {
            return Error("Error reading file: " + path.string());
        }
        
        return data;
    } catch (const std::exception& e) {
        return Error("File read error: " + std::string(e.what()));
    }
}

Result<void> write_file(const std::filesystem::path& path, const std::vector<uint8_t>& data) {
    try {
        // Create directories if they don't exist
        std::filesystem::create_directories(path.parent_path());
        
        std::ofstream file(path, std::ios::binary);
        if (!file.is_open()) {
            return Error("Cannot create file: " + path.string());
        }
        
        file.write(reinterpret_cast<const char*>(data.data()), data.size());
        
        if (!file) {
            return Error("Error writing file: " + path.string());
        }
        
        return {};
    } catch (const std::exception& e) {
        return Error("File write error: " + std::string(e.what()));
    }
}

Result<size_t> get_file_size(const std::filesystem::path& path) {
    try {
        if (!std::filesystem::exists(path)) {
            return Error("File does not exist: " + path.string());
        }
        
        if (!std::filesystem::is_regular_file(path)) {
            return Error("Not a regular file: " + path.string());
        }
        
        return std::filesystem::file_size(path);
    } catch (const std::exception& e) {
        return Error("Error getting file size: " + std::string(e.what()));
    }
}

Result<std::string> get_file_permissions(const std::filesystem::path& path) {
    try {
        if (!std::filesystem::exists(path)) {
            return Error("File does not exist: " + path.string());
        }
        
        auto perms = std::filesystem::status(path).permissions();
        std::string result = std::filesystem::is_directory(path) ? "d" : "-";
        
        // Owner permissions
        result += (perms & std::filesystem::perms::owner_read) != std::filesystem::perms::none ? "r" : "-";
        result += (perms & std::filesystem::perms::owner_write) != std::filesystem::perms::none ? "w" : "-";
        result += (perms & std::filesystem::perms::owner_exec) != std::filesystem::perms::none ? "x" : "-";
        
        // Group permissions
        result += (perms & std::filesystem::perms::group_read) != std::filesystem::perms::none ? "r" : "-";
        result += (perms & std::filesystem::perms::group_write) != std::filesystem::perms::none ? "w" : "-";
        result += (perms & std::filesystem::perms::group_exec) != std::filesystem::perms::none ? "x" : "-";
        
        // Others permissions
        result += (perms & std::filesystem::perms::others_read) != std::filesystem::perms::none ? "r" : "-";
        result += (perms & std::filesystem::perms::others_write) != std::filesystem::perms::none ? "w" : "-";
        result += (perms & std::filesystem::perms::others_exec) != std::filesystem::perms::none ? "x" : "-";
        
        return result;
    } catch (const std::exception& e) {
        return Error("Error getting file permissions: " + std::string(e.what()));
    }
}

// Network utilities
std::string format_passive_address(const std::string& ip, uint16_t port) {
    std::vector<std::string> ip_parts = split(ip, '.');
    if (ip_parts.size() != 4) {
        return "";
    }
    
    uint8_t port_high = (port >> 8) & 0xFF;
    uint8_t port_low = port & 0xFF;
    
    return ip_parts[0] + "," + ip_parts[1] + "," + ip_parts[2] + "," + ip_parts[3] + "," +
           std::to_string(port_high) + "," + std::to_string(port_low);
}

Result<std::pair<std::string, uint16_t>> parse_passive_address(const std::string& address) {
    auto parts = split(address, ',');
    if (parts.size() != 6) {
        return Error("Invalid passive address format");
    }
    
    try {
        std::string ip = parts[0] + "." + parts[1] + "." + parts[2] + "." + parts[3];
        uint16_t port = (std::stoi(parts[4]) << 8) | std::stoi(parts[5]);
        
        return std::make_pair(ip, port);
    } catch (const std::exception& e) {
        return Error("Error parsing passive address: " + std::string(e.what()));
    }
}

std::string get_local_ip_address() {
#ifdef _WIN32
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        struct addrinfo hints, *info;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        
        if (getaddrinfo(hostname, nullptr, &hints, &info) == 0) {
            struct sockaddr_in* addr = (struct sockaddr_in*)info->ai_addr;
            std::string ip = inet_ntoa(addr->sin_addr);
            freeaddrinfo(info);
            return ip;
        }
    }
    return "127.0.0.1";
#else
    struct ifaddrs *ifaddrs_ptr, *ifa;
    
    if (getifaddrs(&ifaddrs_ptr) == -1) {
        return "127.0.0.1";
    }
    
    for (ifa = ifaddrs_ptr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == nullptr || ifa->ifa_addr->sa_family != AF_INET) {
            continue;
        }
        
        // Skip loopback interface
        if (strcmp(ifa->ifa_name, "lo") == 0) {
            continue;
        }
        
        struct sockaddr_in* addr = (struct sockaddr_in*)ifa->ifa_addr;
        std::string ip = inet_ntoa(addr->sin_addr);
        
        freeifaddrs(ifaddrs_ptr);
        return ip;
    }
    
    freeifaddrs(ifaddrs_ptr);
    return "127.0.0.1";
#endif
}

bool is_valid_port(uint16_t port) {
    return port > 0 && port != FTP_CONTROL_PORT; // Don't use FTP control port for data
}

// Protocol utilities
std::string get_response_text(FtpResponseCode code) {
    switch (code) {
        case FtpResponseCode::RESTART_MARKER: return "Restart marker reply";
        case FtpResponseCode::SERVICE_READY_SOON: return "Service ready in few minutes";
        case FtpResponseCode::DATA_CONNECTION_OPEN: return "Data connection already open; transfer starting";
        case FtpResponseCode::FILE_STATUS_OK: return "File status okay; about to open data connection";
        
        case FtpResponseCode::COMMAND_OK: return "Command okay";
        case FtpResponseCode::COMMAND_SUPERFLUOUS: return "Command not implemented, superfluous at this site";
        case FtpResponseCode::SYSTEM_STATUS: return "System status, or system help reply";
        case FtpResponseCode::DIRECTORY_STATUS: return "Directory status";
        case FtpResponseCode::FILE_STATUS: return "File status";
        case FtpResponseCode::HELP_MESSAGE: return "Help message";
        case FtpResponseCode::SYSTEM_TYPE: return "System type";
        case FtpResponseCode::SERVICE_READY: return "Service ready for new user";
        case FtpResponseCode::SERVICE_CLOSING: return "Service closing control connection";
        case FtpResponseCode::DATA_CONNECTION_OPEN_SUCCESS: return "Data connection open; no transfer in progress";
        case FtpResponseCode::DATA_CONNECTION_CLOSED: return "Closing data connection";
        case FtpResponseCode::ENTERING_PASSIVE: return "Entering Passive Mode";
        case FtpResponseCode::ENTERING_EPSV: return "Entering Extended Passive Mode";
        case FtpResponseCode::USER_LOGGED_IN: return "User logged in, proceed";
        case FtpResponseCode::FILE_ACTION_OK: return "Requested file action okay, completed";
        case FtpResponseCode::PATHNAME_CREATED: return "Pathname created";
        
        case FtpResponseCode::USERNAME_OK: return "User name okay, need password";
        case FtpResponseCode::NEED_ACCOUNT: return "Need account for login";
        case FtpResponseCode::FILE_ACTION_PENDING: return "Requested file action pending further information";
        
        case FtpResponseCode::SERVICE_UNAVAILABLE: return "Service not available, closing control connection";
        case FtpResponseCode::CANT_OPEN_DATA: return "Can't open data connection";
        case FtpResponseCode::CONNECTION_CLOSED: return "Connection closed; transfer aborted";
        case FtpResponseCode::FILE_ACTION_ABORTED: return "Requested file action not taken";
        case FtpResponseCode::LOCAL_ERROR: return "Requested action aborted: local error in processing";
        case FtpResponseCode::INSUFFICIENT_STORAGE: return "Requested action not taken: insufficient storage space";
        
        case FtpResponseCode::SYNTAX_ERROR: return "Syntax error, command unrecognized";
        case FtpResponseCode::SYNTAX_ERROR_PARAMS: return "Syntax error in parameters or arguments";
        case FtpResponseCode::COMMAND_NOT_IMPLEMENTED: return "Command not implemented";
        case FtpResponseCode::BAD_SEQUENCE: return "Bad sequence of commands";
        case FtpResponseCode::PARAMETER_NOT_IMPLEMENTED: return "Command not implemented for that parameter";
        case FtpResponseCode::NOT_LOGGED_IN: return "Not logged in";
        case FtpResponseCode::NEED_ACCOUNT_STORE: return "Need account for storing files";
        case FtpResponseCode::FILE_ACTION_NOT_TAKEN: return "Requested action not taken: file unavailable";
        case FtpResponseCode::PAGE_TYPE_UNKNOWN: return "Requested action aborted: page type unknown";
        case FtpResponseCode::STORAGE_EXCEEDED: return "Requested file action aborted: storage allocation exceeded";
        case FtpResponseCode::FILENAME_NOT_ALLOWED: return "Requested action not taken: file name not allowed";
        
        default: return "Unknown response code";
    }
}

bool is_ftp_command(const std::string& command) {
    static const std::vector<std::string> commands = {
        "USER", "PASS", "ACCT", "CWD", "CDUP", "SMNT", "QUIT", "REIN",
        "PORT", "PASV", "EPSV", "TYPE", "STRU", "MODE",
        "RETR", "STOR", "STOU", "APPE", "ALLO", "REST", "RNFR", "RNTO",
        "ABOR", "DELE", "RMD", "MKD", "PWD", "LIST", "NLST", "SITE",
        "SYST", "STAT", "HELP", "NOOP", "SIZE", "MDTM"
    };
    
    std::string upper_cmd = to_upper(command);
    return std::find(commands.begin(), commands.end(), upper_cmd) != commands.end();
}

std::vector<std::string> get_supported_commands() {
    return {
        "USER", "PASS", "CWD", "CDUP", "QUIT",
        "PORT", "PASV", "EPSV", "TYPE",
        "RETR", "STOR", "DELE", "RNFR", "RNTO",
        "RMD", "MKD", "PWD", "LIST", "NLST",
        "SYST", "HELP", "NOOP", "SIZE", "MDTM"
    };
}

// String utilities
std::string trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }
    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

std::string to_upper(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::toupper);
    return result;
}

std::string to_lower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

std::vector<std::string> split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    
    while (std::getline(ss, token, delimiter)) {
        tokens.push_back(token);
    }
    
    return tokens;
}

std::string join(const std::vector<std::string>& parts, const std::string& delimiter) {
    if (parts.empty()) {
        return "";
    }
    
    std::string result = parts[0];
    for (size_t i = 1; i < parts.size(); ++i) {
        result += delimiter + parts[i];
    }
    
    return result;
}

// Time utilities
std::string format_timestamp(const std::chrono::system_clock::time_point& time) {
    auto time_t = std::chrono::system_clock::to_time_t(time);
    auto tm = *std::localtime(&time_t);
    
    std::stringstream ss;
    ss << std::put_time(&tm, "%Y%m%d%H%M%S");
    return ss.str();
}

std::string format_ftp_time(const std::filesystem::file_time_type& time) {
    // Convert file_time_type to system_clock::time_point (this is a simplification)
    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        time - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
    
    auto time_t = std::chrono::system_clock::to_time_t(sctp);
    auto tm = *std::localtime(&time_t);
    
    std::stringstream ss;
    ss << std::put_time(&tm, "%b %d %H:%M");
    return ss.str();
}

// Validation
bool is_valid_username(const std::string& username) {
    if (username.empty() || username.length() > 32) {
        return false;
    }
    
    // Allow alphanumeric characters, underscore, and dash
    return std::all_of(username.begin(), username.end(), [](char c) {
        return std::isalnum(c) || c == '_' || c == '-';
    });
}

bool is_valid_filename(const std::string& filename) {
    if (filename.empty() || filename.length() > 255) {
        return false;
    }
    
    // Check for invalid characters
    const std::string invalid_chars = "<>:\"|?*\0";
    return filename.find_first_of(invalid_chars) == std::string::npos &&
           filename != "." && filename != "..";
}

bool is_valid_ftp_path(const std::string& path) {
    if (path.empty() || path.length() > FTP_MAX_PATH_LENGTH) {
        return false;
    }
    
    // Check for directory traversal attempts
    return !is_directory_traversal_attempt(path);
}

// Conversion utilities
std::vector<uint8_t> string_to_bytes(const std::string& str) {
    return std::vector<uint8_t>(str.begin(), str.end());
}

std::string bytes_to_string(const std::vector<uint8_t>& bytes) {
    return std::string(bytes.begin(), bytes.end());
}

// Security utilities
std::string sanitize_path(const std::string& path) {
    std::string sanitized = path;
    
    // Remove null bytes
    sanitized.erase(std::remove(sanitized.begin(), sanitized.end(), '\0'), sanitized.end());
    
    // Replace potentially dangerous sequences
    size_t pos = 0;
    while ((pos = sanitized.find("../", pos)) != std::string::npos) {
        sanitized.replace(pos, 3, "");
    }
    
    // Remove trailing dots and spaces (Windows issue)
    while (!sanitized.empty() && (sanitized.back() == '.' || sanitized.back() == ' ')) {
        sanitized.pop_back();
    }
    
    return sanitized;
}

bool is_directory_traversal_attempt(const std::string& path) {
    return path.find("../") != std::string::npos ||
           path.find("..\\") != std::string::npos ||
           path.find("/..") != std::string::npos ||
           path.find("\\..") != std::string::npos ||
           path == ".." ||
           path.find('\0') != std::string::npos;
}

} // namespace NetworkQuests::Ftp::Utils