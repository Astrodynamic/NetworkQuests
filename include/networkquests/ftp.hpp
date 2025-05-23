#pragma once

#include "common.hpp"
#include "socket.hpp"
#include "tcp.hpp"
#include "logger.hpp"
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <chrono>
#include <filesystem>
#include <functional>
#include <thread>
#include <mutex>
#include <atomic>
#include <sstream>

namespace NetworkQuests::Ftp {

// Forward declarations
class FtpClient;
class FtpServer;
class FtpSession;
class FtpCommand;
class FtpResponse;
class FtpDataConnection;

// FTP Constants
constexpr uint16_t FTP_CONTROL_PORT = 21;
constexpr uint16_t FTP_DATA_PORT = 20;
constexpr size_t FTP_MAX_COMMAND_LENGTH = 512;
constexpr size_t FTP_MAX_RESPONSE_LENGTH = 512;
constexpr size_t FTP_BUFFER_SIZE = 4096;
constexpr size_t FTP_MAX_PATH_LENGTH = 4096;

// FTP Response Codes (RFC 959)
enum class FtpResponseCode : uint16_t {
    // 1xx - Positive Preliminary Reply
    RESTART_MARKER = 110,               // Restart marker reply
    SERVICE_READY_SOON = 120,           // Service ready in nnn minutes
    DATA_CONNECTION_OPEN = 125,         // Data connection already open; transfer starting
    FILE_STATUS_OK = 150,               // File status okay; about to open data connection

    // 2xx - Positive Completion Reply
    COMMAND_OK = 200,                   // Command okay
    COMMAND_SUPERFLUOUS = 202,          // Command not implemented, superfluous at this site
    SYSTEM_STATUS = 211,                // System status, or system help reply
    DIRECTORY_STATUS = 212,             // Directory status
    FILE_STATUS = 213,                  // File status
    HELP_MESSAGE = 214,                 // Help message
    SYSTEM_TYPE = 215,                  // System type
    SERVICE_READY = 220,                // Service ready for new user
    SERVICE_CLOSING = 221,              // Service closing control connection
    DATA_CONNECTION_OPEN_SUCCESS = 225, // Data connection open; no transfer in progress
    DATA_CONNECTION_CLOSED = 226,       // Closing data connection
    ENTERING_PASSIVE = 227,             // Entering Passive Mode (h1,h2,h3,h4,p1,p2)
    ENTERING_EPSV = 229,                // Entering Extended Passive Mode (|||port|)
    USER_LOGGED_IN = 230,               // User logged in, proceed
    FILE_ACTION_OK = 250,               // Requested file action okay, completed
    PATHNAME_CREATED = 257,             // "PATHNAME" created

    // 3xx - Positive Intermediate Reply
    USERNAME_OK = 331,                  // User name okay, need password
    NEED_ACCOUNT = 332,                 // Need account for login
    FILE_ACTION_PENDING = 350,          // Requested file action pending further information

    // 4xx - Transient Negative Completion Reply
    SERVICE_UNAVAILABLE = 421,          // Service not available, closing control connection
    CANT_OPEN_DATA = 425,               // Can't open data connection
    CONNECTION_CLOSED = 426,            // Connection closed; transfer aborted
    FILE_ACTION_ABORTED = 450,          // Requested file action not taken
    LOCAL_ERROR = 451,                  // Requested action aborted: local error in processing
    INSUFFICIENT_STORAGE = 452,         // Requested action not taken: insufficient storage space

    // 5xx - Permanent Negative Completion Reply
    SYNTAX_ERROR = 500,                 // Syntax error, command unrecognized
    SYNTAX_ERROR_PARAMS = 501,          // Syntax error in parameters or arguments
    COMMAND_NOT_IMPLEMENTED = 502,      // Command not implemented
    BAD_SEQUENCE = 503,                 // Bad sequence of commands
    PARAMETER_NOT_IMPLEMENTED = 504,    // Command not implemented for that parameter
    NOT_LOGGED_IN = 530,                // Not logged in
    NEED_ACCOUNT_STORE = 532,           // Need account for storing files
    FILE_ACTION_NOT_TAKEN = 550,        // Requested action not taken: file unavailable
    PAGE_TYPE_UNKNOWN = 551,            // Requested action aborted: page type unknown
    STORAGE_EXCEEDED = 552,             // Requested file action aborted: storage allocation exceeded
    FILENAME_NOT_ALLOWED = 553          // Requested action not taken: file name not allowed
};

// FTP Transfer Mode
enum class FtpTransferMode {
    ASCII,      // ASCII text mode
    BINARY      // Binary/Image mode
};

// FTP Connection Mode
enum class FtpConnectionMode {
    ACTIVE,     // Server connects to client for data
    PASSIVE     // Client connects to server for data
};

// FTP Data Structure
enum class FtpDataStructure {
    FILE,       // File structure (default)
    RECORD,     // Record structure
    PAGE        // Page structure
};

// FTP User Permissions
struct FtpUserPermissions {
    bool can_read = true;
    bool can_write = false;
    bool can_delete = false;
    bool can_create_dirs = false;
    bool can_list = true;
    bool can_rename = false;
    std::string root_directory = "/";
    std::vector<std::string> allowed_commands;
    
    bool has_command_permission(const std::string& command) const;
};

// FTP User Account
struct FtpUser {
    std::string username;
    std::string password;
    FtpUserPermissions permissions;
    bool is_anonymous = false;
    std::chrono::system_clock::time_point last_login;
    
    FtpUser() = default;
    FtpUser(const std::string& user, const std::string& pass, 
            const FtpUserPermissions& perms = {}, bool anon = false);
};

// FTP Command Structure
class FtpCommand {
public:
    FtpCommand() = default;
    FtpCommand(const std::string& cmd, const std::string& args = "");
    
    // Getters
    const std::string& get_command() const { return command_; }
    const std::string& get_arguments() const { return arguments_; }
    const std::string& get_raw() const { return raw_command_; }
    
    // Setters
    void set_command(const std::string& cmd) { command_ = cmd; }
    void set_arguments(const std::string& args) { arguments_ = args; }
    
    // Parsing
    static Result<FtpCommand> parse(const std::string& line);
    std::string to_string() const;
    
    // Validation
    bool is_valid() const;
    bool requires_data_connection() const;
    bool requires_authentication() const;
    
private:
    std::string command_;
    std::string arguments_;
    std::string raw_command_;
    
    void normalize_command();
};

// FTP Response Structure
class FtpResponse {
public:
    FtpResponse() = default;
    FtpResponse(FtpResponseCode code, const std::string& message = "");
    FtpResponse(uint16_t code, const std::string& message = "");
    
    // Getters
    FtpResponseCode get_code() const { return code_; }
    uint16_t get_code_number() const { return static_cast<uint16_t>(code_); }
    const std::string& get_message() const { return message_; }
    bool is_multiline() const { return multiline_; }
    
    // Setters
    void set_code(FtpResponseCode code) { code_ = code; }
    void set_message(const std::string& message) { message_ = message; }
    void set_multiline(bool multiline) { multiline_ = multiline; }
    
    // Response type checks
    bool is_positive_preliminary() const { return get_code_number() >= 100 && get_code_number() < 200; }
    bool is_positive_completion() const { return get_code_number() >= 200 && get_code_number() < 300; }
    bool is_positive_intermediate() const { return get_code_number() >= 300 && get_code_number() < 400; }
    bool is_transient_negative() const { return get_code_number() >= 400 && get_code_number() < 500; }
    bool is_permanent_negative() const { return get_code_number() >= 500 && get_code_number() < 600; }
    bool is_error() const { return get_code_number() >= 400; }
    bool is_success() const { return get_code_number() >= 200 && get_code_number() < 400; }
    
    // Serialization
    std::string to_string() const;
    std::vector<std::string> to_lines() const;
    static Result<FtpResponse> parse(const std::string& line);
    static Result<FtpResponse> parse_multiline(const std::vector<std::string>& lines);
    
    // Standard responses
    static FtpResponse ok(const std::string& message = "");
    static FtpResponse error(FtpResponseCode code, const std::string& message = "");
    static FtpResponse system_type(const std::string& system_name = "UNIX Type: L8");
    static FtpResponse entering_passive(const std::string& ip, uint16_t port);
    static FtpResponse entering_epsv(uint16_t port);
    static FtpResponse file_status(const std::string& filename, size_t size);
    static FtpResponse directory_listing();
    
private:
    FtpResponseCode code_ = FtpResponseCode::COMMAND_OK;
    std::string message_;
    bool multiline_ = false;
    
    static std::string get_default_message(FtpResponseCode code);
};

// FTP Data Connection Handler
class FtpDataConnection {
public:
    FtpDataConnection() = default;
    ~FtpDataConnection();
    
    // Configuration
    void set_mode(FtpConnectionMode mode) { mode_ = mode; }
    void set_transfer_mode(FtpTransferMode transfer_mode) { transfer_mode_ = transfer_mode; }
    FtpConnectionMode get_mode() const { return mode_; }
    FtpTransferMode get_transfer_mode() const { return transfer_mode_; }
    
    // Active mode (server connects to client)
    Result<void> setup_active(const std::string& client_ip, uint16_t client_port);
    
    // Passive mode (client connects to server)
    Result<uint16_t> setup_passive(const std::string& server_ip = "");
    Result<void> accept_passive_connection();
    
    // Connection management
    Result<void> connect();
    void close();
    bool is_connected() const { return connected_; }
    
    // Data transfer
    Result<void> send_file(const std::filesystem::path& file_path);
    Result<void> receive_file(const std::filesystem::path& file_path);
    Result<void> send_data(const std::vector<uint8_t>& data);
    Result<std::vector<uint8_t>> receive_data();
    Result<void> send_directory_listing(const std::string& listing);
    Result<std::string> receive_directory_listing();
    
    // Transfer progress
    void set_progress_callback(std::function<void(size_t transferred, size_t total)> callback);
    
private:
    FtpConnectionMode mode_ = FtpConnectionMode::PASSIVE;
    FtpTransferMode transfer_mode_ = FtpTransferMode::BINARY;
    std::unique_ptr<TcpSocket> socket_;
    std::unique_ptr<TcpServer> server_;
    std::string client_ip_;
    uint16_t client_port_ = 0;
    std::string server_ip_;
    uint16_t server_port_ = 0;
    bool connected_ = false;
    std::function<void(size_t, size_t)> progress_callback_;
    
    // Helper methods
    Result<void> send_ascii_data(const std::vector<uint8_t>& data);
    Result<std::vector<uint8_t>> receive_ascii_data();
    std::vector<uint8_t> convert_to_ascii(const std::vector<uint8_t>& data);
    std::vector<uint8_t> convert_from_ascii(const std::vector<uint8_t>& data);
};

// FTP Client Session
class FtpClient {
public:
    FtpClient() = default;
    ~FtpClient();
    
    // Connection management
    Result<void> connect(const std::string& host, uint16_t port = FTP_CONTROL_PORT);
    void disconnect();
    bool is_connected() const { return control_socket_ && control_socket_->is_connected(); }
    
    // Authentication
    Result<FtpResponse> login(const std::string& username, const std::string& password = "");
    Result<FtpResponse> login_anonymous();
    Result<FtpResponse> logout();
    
    // Configuration
    void set_timeout(std::chrono::milliseconds timeout) { timeout_ = timeout; }
    void set_transfer_mode(FtpTransferMode mode) { transfer_mode_ = mode; }
    void set_connection_mode(FtpConnectionMode mode) { connection_mode_ = mode; }
    void set_progress_callback(std::function<void(size_t, size_t)> callback);
    
    // Directory operations
    Result<FtpResponse> change_directory(const std::string& path);
    Result<FtpResponse> change_to_parent_directory();
    Result<FtpResponse> print_working_directory();
    Result<FtpResponse> make_directory(const std::string& path);
    Result<FtpResponse> remove_directory(const std::string& path);
    Result<std::string> list_directory(const std::string& path = "");
    Result<std::vector<std::string>> name_list(const std::string& path = "");
    
    // File operations
    Result<FtpResponse> upload_file(const std::filesystem::path& local_path, 
                                   const std::string& remote_path = "");
    Result<FtpResponse> download_file(const std::string& remote_path, 
                                     const std::filesystem::path& local_path = "");
    Result<FtpResponse> delete_file(const std::string& remote_path);
    Result<FtpResponse> rename_file(const std::string& from_path, const std::string& to_path);
    Result<FtpResponse> get_file_size(const std::string& remote_path);
    Result<FtpResponse> get_modification_time(const std::string& remote_path);
    
    // System operations
    Result<FtpResponse> system_type();
    Result<FtpResponse> help(const std::string& command = "");
    Result<FtpResponse> noop();
    Result<FtpResponse> quit();
    
    // Low-level command interface
    Result<FtpResponse> send_command(const std::string& command, const std::string& args = "");
    Result<FtpResponse> send_command(const FtpCommand& command);
    
private:
    std::unique_ptr<TcpSocket> control_socket_;
    FtpDataConnection data_connection_;
    std::chrono::milliseconds timeout_{30000};
    FtpTransferMode transfer_mode_ = FtpTransferMode::BINARY;
    FtpConnectionMode connection_mode_ = FtpConnectionMode::PASSIVE;
    std::string current_directory_;
    bool logged_in_ = false;
    std::function<void(size_t, size_t)> progress_callback_;
    
    // Internal methods
    Result<FtpResponse> read_response();
    Result<FtpResponse> read_multiline_response();
    Result<void> setup_data_connection();
    Result<void> setup_transfer_mode();
    Result<void> setup_passive_mode();
    Result<void> setup_active_mode();
    std::string get_local_ip() const;
    uint16_t allocate_data_port() const;
};

// FTP Server Session (per-client connection)
class FtpSession {
public:
    explicit FtpSession(std::unique_ptr<TcpSocket> socket, 
                       const std::unordered_map<std::string, FtpUser>& users,
                       const std::string& root_directory = "/tmp/ftp");
    ~FtpSession();
    
    // Session management
    void run();
    void close();
    bool is_active() const { return active_; }
    
    // State
    const std::string& get_client_address() const { return client_address_; }
    const std::string& get_current_user() const { return current_user_; }
    bool is_authenticated() const { return authenticated_; }
    
private:
    std::unique_ptr<TcpSocket> control_socket_;
    FtpDataConnection data_connection_;
    const std::unordered_map<std::string, FtpUser>& users_;
    std::string root_directory_;
    std::string client_address_;
    
    // Session state
    bool active_ = true;
    bool authenticated_ = false;
    std::string current_user_;
    std::string pending_username_;
    std::string current_directory_;
    FtpTransferMode transfer_mode_ = FtpTransferMode::BINARY;
    FtpConnectionMode connection_mode_ = FtpConnectionMode::PASSIVE;
    std::string rename_from_path_;
    
    // Command processing
    void process_commands();
    Result<void> handle_command(const FtpCommand& command);
    Result<void> send_response(const FtpResponse& response);
    
    // Authentication commands
    Result<void> handle_user(const std::string& username);
    Result<void> handle_pass(const std::string& password);
    Result<void> handle_quit();
    
    // Transfer mode commands
    Result<void> handle_type(const std::string& type);
    Result<void> handle_mode(const std::string& mode);
    Result<void> handle_stru(const std::string& structure);
    
    // Data connection commands
    Result<void> handle_port(const std::string& args);
    Result<void> handle_pasv();
    Result<void> handle_epsv(const std::string& args = "");
    
    // Directory commands
    Result<void> handle_pwd();
    Result<void> handle_cwd(const std::string& path);
    Result<void> handle_cdup();
    Result<void> handle_mkd(const std::string& path);
    Result<void> handle_rmd(const std::string& path);
    Result<void> handle_list(const std::string& path = "");
    Result<void> handle_nlst(const std::string& path = "");
    
    // File commands
    Result<void> handle_retr(const std::string& filename);
    Result<void> handle_stor(const std::string& filename);
    Result<void> handle_dele(const std::string& filename);
    Result<void> handle_rnfr(const std::string& filename);
    Result<void> handle_rnto(const std::string& filename);
    Result<void> handle_size(const std::string& filename);
    Result<void> handle_mdtm(const std::string& filename);
    
    // System commands
    Result<void> handle_syst();
    Result<void> handle_help(const std::string& command = "");
    Result<void> handle_noop();
    Result<void> handle_stat(const std::string& path = "");
    
    // Helper methods
    std::string resolve_path(const std::string& path) const;
    std::string get_relative_path(const std::string& full_path) const;
    bool is_path_allowed(const std::string& path) const;
    bool has_permission(const std::string& operation) const;
    std::string generate_directory_listing(const std::string& path) const;
    std::string generate_name_list(const std::string& path) const;
    std::string format_file_info(const std::filesystem::directory_entry& entry) const;
    std::string format_modification_time(const std::filesystem::file_time_type& time) const;
    
    // Port parsing
    Result<std::pair<std::string, uint16_t>> parse_port_command(const std::string& args) const;
    std::string format_passive_response(const std::string& ip, uint16_t port) const;
};

// FTP Server
class FtpServer {
public:
    explicit FtpServer(uint16_t port = FTP_CONTROL_PORT);
    ~FtpServer();
    
    // Server control
    Result<void> start();
    void stop();
    bool is_running() const { return running_; }
    
    // Configuration
    void set_root_directory(const std::string& root) { root_directory_ = root; }
    void set_max_connections(size_t max_conn) { max_connections_ = max_conn; }
    void set_session_timeout(std::chrono::seconds timeout) { session_timeout_ = timeout; }
    void set_welcome_message(const std::string& message) { welcome_message_ = message; }
    
    // User management
    void add_user(const FtpUser& user);
    void remove_user(const std::string& username);
    void enable_anonymous_access(bool enable = true, const FtpUserPermissions& perms = {});
    bool authenticate_user(const std::string& username, const std::string& password) const;
    
    // Statistics
    size_t get_active_sessions() const { return active_sessions_.size(); }
    size_t get_total_connections() const { return total_connections_; }
    
private:
    uint16_t port_;
    std::string root_directory_;
    size_t max_connections_;
    std::chrono::seconds session_timeout_;
    std::string welcome_message_;
    
    // Server state
    std::atomic<bool> running_{false};
    std::atomic<bool> should_stop_{false};
    std::unique_ptr<TcpServer> tcp_server_;
    std::thread accept_thread_;
    
    // User management
    std::unordered_map<std::string, FtpUser> users_;
    mutable std::shared_mutex users_mutex_;
    bool anonymous_enabled_ = false;
    FtpUserPermissions anonymous_permissions_;
    
    // Session management
    std::vector<std::unique_ptr<FtpSession>> active_sessions_;
    mutable std::mutex sessions_mutex_;
    std::atomic<size_t> total_connections_{0};
    
    // Server operations
    void accept_connections();
    void cleanup_sessions();
    void session_cleanup_thread();
};

// FTP Utilities
namespace Utils {
    // Path operations
    std::string normalize_path(const std::string& path);
    std::string join_paths(const std::string& base, const std::string& relative);
    bool is_absolute_path(const std::string& path);
    bool is_safe_path(const std::string& path, const std::string& root);
    
    // File operations
    Result<std::vector<uint8_t>> read_file(const std::filesystem::path& path);
    Result<void> write_file(const std::filesystem::path& path, const std::vector<uint8_t>& data);
    Result<size_t> get_file_size(const std::filesystem::path& path);
    Result<std::string> get_file_permissions(const std::filesystem::path& path);
    
    // Network utilities
    std::string format_passive_address(const std::string& ip, uint16_t port);
    Result<std::pair<std::string, uint16_t>> parse_passive_address(const std::string& address);
    std::string get_local_ip_address();
    bool is_valid_port(uint16_t port);
    
    // Protocol utilities
    std::string get_response_text(FtpResponseCode code);
    bool is_ftp_command(const std::string& command);
    std::vector<std::string> get_supported_commands();
    
    // String utilities
    std::string trim(const std::string& str);
    std::string to_upper(const std::string& str);
    std::string to_lower(const std::string& str);
    std::vector<std::string> split(const std::string& str, char delimiter);
    std::string join(const std::vector<std::string>& parts, const std::string& delimiter);
    
    // Time utilities
    std::string format_timestamp(const std::chrono::system_clock::time_point& time);
    std::string format_ftp_time(const std::filesystem::file_time_type& time);
    
    // Validation
    bool is_valid_username(const std::string& username);
    bool is_valid_filename(const std::string& filename);
    bool is_valid_ftp_path(const std::string& path);
    
    // Conversion utilities
    std::vector<uint8_t> string_to_bytes(const std::string& str);
    std::string bytes_to_string(const std::vector<uint8_t>& bytes);
    
    // Security utilities
    std::string sanitize_path(const std::string& path);
    bool is_directory_traversal_attempt(const std::string& path);
}

} // namespace NetworkQuests::Ftp