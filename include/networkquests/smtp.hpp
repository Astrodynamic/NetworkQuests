#pragma once

#include "networkquests/tcp.hpp"
#include "networkquests/common.hpp"
#include "networkquests/logger.hpp"

#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <functional>
#include <memory>
#include <atomic>
#include <thread>
#include <regex>

namespace networkquests::smtp {

// Forward declarations
class SmtpClient;
class SmtpServer;
class SmtpConnection;
class SmtpMessage;
class EmailAddress;

// SMTP commands (RFC 5321)
enum class SmtpCommand {
    EHLO,           // Extended HELLO
    HELO,           // HELLO
    MAIL,           // MAIL FROM
    RCPT,           // RCPT TO
    DATA,           // DATA
    RSET,           // RESET
    NOOP,           // No Operation
    QUIT,           // QUIT
    VRFY,           // Verify
    EXPN,           // Expand
    HELP,           // Help
    AUTH,           // Authentication (extension)
    STARTTLS        // Start TLS (extension)
};

// SMTP response codes (RFC 5321)
enum class SmtpResponseCode {
    // 2xx Success
    SystemStatus = 211,
    HelpMessage = 214,
    ServiceReady = 220,
    ServiceClosing = 221,
    AuthenticationSuccessful = 235,
    OK = 250,
    UserNotLocal = 251,
    CannotVerify = 252,
    
    // 3xx Intermediate
    StartMailInput = 354,
    
    // 4xx Temporary Failure
    ServiceNotAvailable = 421,
    PasswordTransition = 432,
    MailboxBusy = 450,
    LocalError = 451,
    InsufficientStorage = 452,
    TemporaryAuthFailure = 454,
    
    // 5xx Permanent Failure
    SyntaxError = 500,
    ParameterError = 501,
    CommandNotImplemented = 502,
    BadSequence = 503,
    ParameterNotImplemented = 504,
    MailboxUnavailable = 550,
    UserNotLocal2 = 551,
    ExceededStorage = 552,
    InvalidMailbox = 553,
    TransactionFailed = 554,
    ParametersNotRecognized = 555
};

// SMTP authentication methods
enum class SmtpAuthMethod {
    NONE,
    PLAIN,
    LOGIN,
    CRAM_MD5,
    DIGEST_MD5,
    OAUTH2
};

// SMTP extensions
enum class SmtpExtension {
    SIZE,           // Message size declaration
    PIPELINING,     // Command pipelining
    ENHANCEDSTATUSCODES, // Enhanced status codes
    STARTTLS,       // Start TLS
    AUTH,           // Authentication
    DELIVERYSTATUS, // Delivery status notifications
    BINARYMIME,     // Binary MIME
    CHUNKING        // Chunking
};

// Email priority
enum class EmailPriority {
    Low = 1,
    Normal = 3,
    High = 5
};

/**
 * @brief Represents an email address with name and address
 */
class EmailAddress {
public:
    EmailAddress() = default;
    EmailAddress(std::string_view address);
    EmailAddress(std::string_view name, std::string_view address);
    
    // Accessors
    const std::string& name() const { return name_; }
    const std::string& address() const { return address_; }
    
    // Mutators
    void set_name(std::string_view name) { name_ = name; }
    void set_address(std::string_view address) { address_ = address; }
    
    // Validation
    bool is_valid() const;
    
    // Formatting
    std::string to_string() const;
    std::string to_address_only() const;
    
    // Parsing
    static Result<EmailAddress> from_string(std::string_view str);
    
    // Comparison
    bool operator==(const EmailAddress& other) const;
    bool operator!=(const EmailAddress& other) const;
    
private:
    std::string name_;
    std::string address_;
};

/**
 * @brief Represents an SMTP command with arguments
 */
class SmtpCommand_ {
public:
    SmtpCommand_() = default;
    SmtpCommand_(SmtpCommand command);
    SmtpCommand_(SmtpCommand command, std::string_view argument);
    SmtpCommand_(SmtpCommand command, const std::vector<std::string>& arguments);
    
    // Accessors
    SmtpCommand command() const { return command_; }
    const std::vector<std::string>& arguments() const { return arguments_; }
    
    // Mutators
    void set_command(SmtpCommand command) { command_ = command; }
    void add_argument(std::string_view argument);
    void set_arguments(const std::vector<std::string>& arguments);
    
    // Serialization
    std::string to_string() const;
    static Result<SmtpCommand_> from_string(std::string_view data);
    
private:
    SmtpCommand command_ = SmtpCommand::NOOP;
    std::vector<std::string> arguments_;
};

/**
 * @brief Represents an SMTP response with code and message
 */
class SmtpResponse {
public:
    SmtpResponse() = default;
    SmtpResponse(SmtpResponseCode code);
    SmtpResponse(SmtpResponseCode code, std::string_view message);
    SmtpResponse(SmtpResponseCode code, const std::vector<std::string>& messages);
    
    // Accessors
    SmtpResponseCode code() const { return code_; }
    const std::vector<std::string>& messages() const { return messages_; }
    bool is_success() const;
    bool is_intermediate() const;
    bool is_temporary_failure() const;
    bool is_permanent_failure() const;
    
    // Mutators
    void set_code(SmtpResponseCode code) { code_ = code; }
    void add_message(std::string_view message);
    void set_messages(const std::vector<std::string>& messages);
    
    // Serialization
    std::string to_string() const;
    static Result<SmtpResponse> from_string(std::string_view data);
    
private:
    SmtpResponseCode code_ = SmtpResponseCode::OK;
    std::vector<std::string> messages_;
};

/**
 * @brief Represents an email message
 */
class SmtpMessage {
public:
    SmtpMessage() = default;
    
    // Basic message properties
    void set_from(const EmailAddress& from) { from_ = from; }
    void add_to(const EmailAddress& to) { to_.push_back(to); }
    void add_cc(const EmailAddress& cc) { cc_.push_back(cc); }
    void add_bcc(const EmailAddress& bcc) { bcc_.push_back(bcc); }
    
    void set_subject(std::string_view subject) { subject_ = subject; }
    void set_body(std::string_view body) { body_ = body; }
    void set_priority(EmailPriority priority) { priority_ = priority; }
    
    // Headers
    void set_header(std::string_view name, std::string_view value);
    std::optional<std::string> get_header(std::string_view name) const;
    void remove_header(std::string_view name);
    
    // Attachments
    struct Attachment {
        std::string filename;
        std::string content_type;
        std::vector<uint8_t> data;
        bool inline_attachment = false;
    };
    
    void add_attachment(const Attachment& attachment);
    void add_attachment_from_file(std::string_view filename, std::string_view content_type = "");
    
    // Accessors
    const EmailAddress& from() const { return from_; }
    const std::vector<EmailAddress>& to() const { return to_; }
    const std::vector<EmailAddress>& cc() const { return cc_; }
    const std::vector<EmailAddress>& bcc() const { return bcc_; }
    const std::string& subject() const { return subject_; }
    const std::string& body() const { return body_; }
    EmailPriority priority() const { return priority_; }
    const std::vector<Attachment>& attachments() const { return attachments_; }
    
    // Message generation
    std::string to_mime_string() const;
    std::string get_message_id() const;
    
    // Validation
    bool is_valid() const;
    
    // Utility
    void clear();
    size_t estimated_size() const;
    
private:
    EmailAddress from_;
    std::vector<EmailAddress> to_;
    std::vector<EmailAddress> cc_;
    std::vector<EmailAddress> bcc_;
    std::string subject_;
    std::string body_;
    EmailPriority priority_ = EmailPriority::Normal;
    std::unordered_map<std::string, std::string> headers_;
    std::vector<Attachment> attachments_;
    
    std::string generate_boundary() const;
    std::string encode_header(std::string_view header) const;
};

/**
 * @brief SMTP connection handler for both client and server
 */
class SmtpConnection {
public:
    explicit SmtpConnection(tcp::TcpConnection tcp_conn);
    
    // Move-only type
    SmtpConnection(const SmtpConnection&) = delete;
    SmtpConnection& operator=(const SmtpConnection&) = delete;
    SmtpConnection(SmtpConnection&&) noexcept = default;
    SmtpConnection& operator=(SmtpConnection&&) noexcept = default;
    
    // Connection info
    SocketAddress local_address() const;
    SocketAddress remote_address() const;
    bool is_connected() const;
    
    // SMTP protocol operations
    Result<void> send_command(const SmtpCommand_& command);
    Result<SmtpResponse> receive_response(std::chrono::milliseconds timeout = std::chrono::seconds(30));
    Result<void> send_response(const SmtpResponse& response);
    Result<SmtpCommand_> receive_command(std::chrono::milliseconds timeout = std::chrono::seconds(30));
    
    // Data transfer
    Result<void> send_data(std::string_view data);
    Result<std::string> receive_data_block(std::chrono::milliseconds timeout = std::chrono::seconds(30));
    
    // Connection management
    void close();
    
    // State
    void set_authenticated(bool auth) { authenticated_ = auth; }
    bool is_authenticated() const { return authenticated_; }
    void set_secure(bool secure) { secure_ = secure; }
    bool is_secure() const { return secure_; }
    
private:
    tcp::TcpConnection tcp_conn_;
    bool authenticated_ = false;
    bool secure_ = false;
};

/**
 * @brief SMTP client for sending emails
 */
class SmtpClient {
public:
    SmtpClient();
    explicit SmtpClient(std::chrono::milliseconds timeout);
    
    // Move-only type
    SmtpClient(const SmtpClient&) = delete;
    SmtpClient& operator=(const SmtpClient&) = delete;
    SmtpClient(SmtpClient&&) noexcept = default;
    SmtpClient& operator=(SmtpClient&&) noexcept = default;
    
    // Connection
    Result<void> connect(std::string_view hostname, Port port = 25);
    Result<void> connect_secure(std::string_view hostname, Port port = 465);
    void disconnect();
    bool is_connected() const;
    
    // Authentication
    Result<void> authenticate(std::string_view username, std::string_view password, 
                             SmtpAuthMethod method = SmtpAuthMethod::PLAIN);
    Result<void> start_tls();
    
    // Mail operations
    Result<void> send_message(const SmtpMessage& message);
    Result<void> send_raw_message(const EmailAddress& from, 
                                 const std::vector<EmailAddress>& to,
                                 std::string_view message_data);
    
    // Server capabilities
    Result<std::vector<SmtpExtension>> get_extensions();
    bool supports_extension(SmtpExtension extension) const;
    
    // Settings
    void set_timeout(std::chrono::milliseconds timeout) { timeout_ = timeout; }
    void set_hostname(std::string_view hostname) { client_hostname_ = hostname; }
    void set_max_message_size(size_t size) { max_message_size_ = size; }
    
private:
    std::unique_ptr<SmtpConnection> connection_;
    std::chrono::milliseconds timeout_;
    std::string client_hostname_;
    std::vector<SmtpExtension> server_extensions_;
    size_t max_message_size_ = 0;
    
    Result<void> perform_handshake();
    Result<void> send_ehlo();
    Result<void> send_mail_transaction(const SmtpMessage& message);
    std::string encode_auth_plain(std::string_view username, std::string_view password) const;
    std::string encode_auth_login(std::string_view data) const;
};

/**
 * @brief SMTP server for receiving emails
 */
class SmtpServer {
public:
    using MessageHandler = std::function<void(const SmtpMessage&, const SocketAddress&)>;
    using AuthValidator = std::function<bool(std::string_view username, std::string_view password)>;
    using RecipientValidator = std::function<bool(const EmailAddress&)>;
    
    explicit SmtpServer(Port port = 25);
    SmtpServer(const SocketAddress& bind_addr);
    
    // Move-only type
    SmtpServer(const SmtpServer&) = delete;
    SmtpServer& operator=(const SmtpServer&) = delete;
    SmtpServer(SmtpServer&&) noexcept = default;
    SmtpServer& operator=(SmtpServer&&) noexcept = default;
    
    // Server lifecycle
    Result<void> start();
    void stop();
    bool is_running() const;
    
    // Event handlers
    void set_message_handler(MessageHandler handler) { message_handler_ = handler; }
    void set_auth_validator(AuthValidator validator) { auth_validator_ = validator; }
    void set_recipient_validator(RecipientValidator validator) { recipient_validator_ = validator; }
    
    // Configuration
    void set_hostname(std::string_view hostname) { hostname_ = hostname; }
    void set_max_connections(int max_conn) { max_connections_ = max_conn; }
    void set_max_message_size(size_t size) { max_message_size_ = size; }
    void set_require_auth(bool require) { require_auth_ = require; }
    void add_supported_extension(SmtpExtension extension);
    
    // Server information
    SocketAddress local_address() const;
    size_t active_connections() const { return active_connections_.load(); }
    
private:
    SocketAddress bind_addr_;
    std::string hostname_;
    std::unique_ptr<tcp::TcpServer> tcp_server_;
    std::atomic<bool> running_{false};
    std::atomic<size_t> active_connections_{0};
    int max_connections_ = 100;
    size_t max_message_size_ = 10 * 1024 * 1024; // 10MB
    bool require_auth_ = false;
    std::vector<SmtpExtension> supported_extensions_;
    
    // Event handlers
    MessageHandler message_handler_;
    AuthValidator auth_validator_;
    RecipientValidator recipient_validator_;
    
    // Session state
    struct SessionState {
        bool authenticated = false;
        bool secure = false;
        EmailAddress mail_from;
        std::vector<EmailAddress> rcpt_to;
        std::string message_buffer;
        std::chrono::steady_clock::time_point last_activity;
    };
    
    void handle_client(tcp::TcpConnection connection);
    void process_session(SmtpConnection& connection);
    
    SmtpResponse handle_command(const SmtpCommand_& command, SessionState& state, SmtpConnection& connection);
    SmtpResponse handle_ehlo(const SmtpCommand_& command, SessionState& state);
    SmtpResponse handle_helo(const SmtpCommand_& command, SessionState& state);
    SmtpResponse handle_mail(const SmtpCommand_& command, SessionState& state);
    SmtpResponse handle_rcpt(const SmtpCommand_& command, SessionState& state);
    SmtpResponse handle_data(const SmtpCommand_& command, SessionState& state, SmtpConnection& connection);
    SmtpResponse handle_auth(const SmtpCommand_& command, SessionState& state, SmtpConnection& connection);
    SmtpResponse handle_rset(const SmtpCommand_& command, SessionState& state);
    SmtpResponse handle_noop(const SmtpCommand_& command, SessionState& state);
    SmtpResponse handle_quit(const SmtpCommand_& command, SessionState& state);
    SmtpResponse handle_vrfy(const SmtpCommand_& command, SessionState& state);
    SmtpResponse handle_expn(const SmtpCommand_& command, SessionState& state);
    SmtpResponse handle_help(const SmtpCommand_& command, SessionState& state);
    
    void reset_session_state(SessionState& state);
    bool is_valid_email_address(std::string_view address) const;
};

// Utility functions
namespace smtp_utils {

/**
 * @brief Convert SMTP command to string
 */
std::string_view command_to_string(SmtpCommand command);

/**
 * @brief Convert string to SMTP command
 */
Result<SmtpCommand> string_to_command(std::string_view command);

/**
 * @brief Convert SMTP response code to string
 */
std::string response_code_to_string(SmtpResponseCode code);

/**
 * @brief Get response code description
 */
std::string_view response_description(SmtpResponseCode code);

/**
 * @brief Convert authentication method to string
 */
std::string_view auth_method_to_string(SmtpAuthMethod method);

/**
 * @brief Convert string to authentication method
 */
Result<SmtpAuthMethod> string_to_auth_method(std::string_view method);

/**
 * @brief Convert extension to string
 */
std::string_view extension_to_string(SmtpExtension extension);

/**
 * @brief Convert string to extension
 */
Result<SmtpExtension> string_to_extension(std::string_view extension);

/**
 * @brief Validate email address format
 */
bool is_valid_email_address(std::string_view address);

/**
 * @brief Extract domain from email address
 */
std::string extract_domain(std::string_view address);

/**
 * @brief Extract local part from email address
 */
std::string extract_local_part(std::string_view address);

/**
 * @brief Base64 encode string
 */
std::string base64_encode(std::string_view input);

/**
 * @brief Base64 decode string
 */
Result<std::string> base64_decode(std::string_view input);

/**
 * @brief Encode text for MIME header
 */
std::string encode_mime_header(std::string_view text, std::string_view charset = "UTF-8");

/**
 * @brief Decode MIME header text
 */
Result<std::string> decode_mime_header(std::string_view encoded);

/**
 * @brief Format current time for email headers
 */
std::string format_email_date();

/**
 * @brief Generate unique message ID
 */
std::string generate_message_id(std::string_view domain);

/**
 * @brief Parse MIME content type
 */
struct MimeType {
    std::string type;
    std::string subtype;
    std::unordered_map<std::string, std::string> parameters;
};

Result<MimeType> parse_mime_type(std::string_view content_type);

/**
 * @brief Get MIME type for file extension
 */
std::string get_mime_type_for_file(std::string_view filename);

/**
 * @brief Simple email sending function
 */
Result<void> send_simple_email(std::string_view smtp_server, Port port,
                              std::string_view from, std::string_view to,
                              std::string_view subject, std::string_view body,
                              std::string_view username = "", std::string_view password = "");

} // namespace smtp_utils

} // namespace networkquests::smtp