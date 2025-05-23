#pragma once

#include "networkquests/tcp.hpp"
#include "networkquests/http.hpp"
#include "networkquests/common.hpp"
#include "networkquests/logger.hpp"

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <chrono>
#include <random>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>

namespace networkquests::websocket {

// Forward declarations
class WebSocketClient;
class WebSocketServer;
class WebSocketConnection;
class WebSocketFrame;
class WebSocketMessage;

// WebSocket operation codes (RFC 6455 Section 5.2)
enum class WebSocketOpcode : uint8_t {
    Continuation = 0x0,
    Text = 0x1,
    Binary = 0x2,
    // 0x3-0x7 reserved for future non-control frames
    Close = 0x8,
    Ping = 0x9,
    Pong = 0xA
    // 0xB-0xF reserved for future control frames
};

// WebSocket close codes (RFC 6455 Section 7.4)
enum class WebSocketCloseCode : uint16_t {
    Normal = 1000,
    GoingAway = 1001,
    ProtocolError = 1002,
    UnsupportedData = 1003,
    NoStatusReceived = 1005,
    AbnormalClosure = 1006,
    InvalidFramePayloadData = 1007,
    PolicyViolation = 1008,
    MessageTooBig = 1009,
    MandatoryExtension = 1010,
    InternalError = 1011,
    ServiceRestart = 1012,
    TryAgainLater = 1013,
    BadGateway = 1014,
    TlsHandshake = 1015
};

// WebSocket connection state
enum class WebSocketState {
    Connecting,
    Open,
    Closing,
    Closed
};

// WebSocket frame types
enum class WebSocketFrameType {
    Text,
    Binary,
    Close,
    Ping,
    Pong,
    Continuation
};

// WebSocket extensions
enum class WebSocketExtension {
    None,
    PerMessageDeflate
};

/**
 * @brief Represents a WebSocket frame according to RFC 6455
 */
class WebSocketFrame {
public:
    WebSocketFrame() = default;
    WebSocketFrame(WebSocketOpcode opcode, std::vector<uint8_t> payload, bool fin = true);
    WebSocketFrame(WebSocketOpcode opcode, std::string_view payload, bool fin = true);
    
    // Accessors
    bool fin() const { return fin_; }
    bool rsv1() const { return rsv1_; }
    bool rsv2() const { return rsv2_; }
    bool rsv3() const { return rsv3_; }
    WebSocketOpcode opcode() const { return opcode_; }
    bool masked() const { return masked_; }
    uint64_t payload_length() const { return payload_length_; }
    uint32_t masking_key() const { return masking_key_; }
    const std::vector<uint8_t>& payload() const { return payload_; }
    
    // Mutators
    void set_fin(bool fin) { fin_ = fin; }
    void set_rsv1(bool rsv1) { rsv1_ = rsv1; }
    void set_rsv2(bool rsv2) { rsv2_ = rsv2; }
    void set_rsv3(bool rsv3) { rsv3_ = rsv3; }
    void set_opcode(WebSocketOpcode opcode) { opcode_ = opcode; }
    void set_masked(bool masked) { masked_ = masked; }
    void set_masking_key(uint32_t key) { masking_key_ = key; }
    void set_payload(const std::vector<uint8_t>& payload);
    void set_payload(std::string_view payload);
    
    // Frame operations
    bool is_control_frame() const;
    bool is_data_frame() const;
    WebSocketFrameType frame_type() const;
    
    // Serialization
    std::vector<uint8_t> serialize() const;
    static Result<WebSocketFrame> deserialize(const std::vector<uint8_t>& data);
    static Result<WebSocketFrame> deserialize(const uint8_t* data, size_t length);
    
    // Utility
    std::string payload_as_string() const;
    void generate_masking_key();
    void apply_mask();
    void remove_mask();
    
private:
    bool fin_ = true;
    bool rsv1_ = false;
    bool rsv2_ = false;
    bool rsv3_ = false;
    WebSocketOpcode opcode_ = WebSocketOpcode::Text;
    bool masked_ = false;
    uint64_t payload_length_ = 0;
    uint32_t masking_key_ = 0;
    std::vector<uint8_t> payload_;
};

/**
 * @brief Represents a complete WebSocket message (potentially fragmented)
 */
class WebSocketMessage {
public:
    WebSocketMessage() = default;
    explicit WebSocketMessage(WebSocketFrameType type);
    WebSocketMessage(WebSocketFrameType type, std::string_view data);
    WebSocketMessage(WebSocketFrameType type, const std::vector<uint8_t>& data);
    
    // Accessors
    WebSocketFrameType type() const { return type_; }
    const std::vector<uint8_t>& data() const { return data_; }
    bool is_complete() const { return complete_; }
    size_t size() const { return data_.size(); }
    
    // Operations
    void append_frame(const WebSocketFrame& frame);
    std::string as_string() const;
    void clear();
    
    // Creation
    static WebSocketMessage text(std::string_view content);
    static WebSocketMessage binary(const std::vector<uint8_t>& content);
    static WebSocketMessage close(WebSocketCloseCode code = WebSocketCloseCode::Normal, 
                                 std::string_view reason = "");
    static WebSocketMessage ping(std::string_view data = "");
    static WebSocketMessage pong(std::string_view data = "");
    
private:
    WebSocketFrameType type_ = WebSocketFrameType::Text;
    std::vector<uint8_t> data_;
    bool complete_ = false;
};

/**
 * @brief Represents a WebSocket connection
 */
class WebSocketConnection {
public:
    using MessageHandler = std::function<void(const WebSocketMessage&)>;
    using CloseHandler = std::function<void(WebSocketCloseCode, std::string_view)>;
    using ErrorHandler = std::function<void(const std::string&)>;
    
    explicit WebSocketConnection(tcp::TcpConnection tcp_conn);
    
    // Move-only type
    WebSocketConnection(const WebSocketConnection&) = delete;
    WebSocketConnection& operator=(const WebSocketConnection&) = delete;
    WebSocketConnection(WebSocketConnection&&) noexcept = default;
    WebSocketConnection& operator=(WebSocketConnection&&) noexcept = default;
    
    // Connection state
    WebSocketState state() const { return state_; }
    bool is_connected() const { return state_ == WebSocketState::Open; }
    bool is_client() const { return is_client_; }
    
    // Message sending
    Result<void> send(const WebSocketMessage& message);
    Result<void> send_text(std::string_view text);
    Result<void> send_binary(const std::vector<uint8_t>& data);
    Result<void> send_ping(std::string_view data = "");
    Result<void> send_pong(std::string_view data = "");
    Result<void> close(WebSocketCloseCode code = WebSocketCloseCode::Normal, 
                      std::string_view reason = "");
    
    // Message receiving
    Result<WebSocketMessage> receive_message(std::chrono::milliseconds timeout = std::chrono::seconds(30));
    void start_async_receive();
    void stop_async_receive();
    
    // Event handlers
    void set_message_handler(MessageHandler handler) { message_handler_ = std::move(handler); }
    void set_close_handler(CloseHandler handler) { close_handler_ = std::move(handler); }
    void set_error_handler(ErrorHandler handler) { error_handler_ = std::move(handler); }
    
    // Settings
    void set_max_message_size(size_t size) { max_message_size_ = size; }
    void set_ping_interval(std::chrono::milliseconds interval) { ping_interval_ = interval; }
    void enable_auto_pong(bool enable) { auto_pong_ = enable; }
    
    // Connection info
    SocketAddress local_address() const;
    SocketAddress remote_address() const;
    
private:
    tcp::TcpConnection tcp_conn_;
    WebSocketState state_ = WebSocketState::Connecting;
    bool is_client_ = false;
    
    // Event handlers
    MessageHandler message_handler_;
    CloseHandler close_handler_;
    ErrorHandler error_handler_;
    
    // Message reassembly
    std::unordered_map<WebSocketFrameType, WebSocketMessage> partial_messages_;
    
    // Settings
    size_t max_message_size_ = 1024 * 1024; // 1MB default
    std::chrono::milliseconds ping_interval_{30000}; // 30 seconds
    bool auto_pong_ = true;
    
    // Async receive
    std::atomic<bool> receiving_{false};
    std::thread receive_thread_;
    
    // Helper methods
    Result<WebSocketFrame> receive_frame();
    Result<void> send_frame(const WebSocketFrame& frame);
    void process_frame(const WebSocketFrame& frame);
    void handle_control_frame(const WebSocketFrame& frame);
    void handle_data_frame(const WebSocketFrame& frame);
    void async_receive_loop();
    
    friend class WebSocketClient;
    friend class WebSocketServer;
};

/**
 * @brief WebSocket client implementation
 */
class WebSocketClient {
public:
    WebSocketClient();
    explicit WebSocketClient(const http::HttpHeaders& headers);
    
    // Move-only type
    WebSocketClient(const WebSocketClient&) = delete;
    WebSocketClient& operator=(const WebSocketClient&) = delete;
    WebSocketClient(WebSocketClient&&) noexcept = default;
    WebSocketClient& operator=(WebSocketClient&&) noexcept = default;
    
    // Connection
    Result<std::unique_ptr<WebSocketConnection>> connect(std::string_view url);
    Result<std::unique_ptr<WebSocketConnection>> connect(std::string_view host, Port port, 
                                                        std::string_view path = "/");
    
    // Settings
    void set_timeout(std::chrono::milliseconds timeout) { timeout_ = timeout; }
    void set_headers(const http::HttpHeaders& headers) { headers_ = headers; }
    void add_header(std::string_view name, std::string_view value);
    void set_subprotocol(std::string_view protocol) { subprotocol_ = protocol; }
    void set_extensions(const std::vector<WebSocketExtension>& extensions) { extensions_ = extensions; }
    
    // Connection info
    const std::string& selected_subprotocol() const { return selected_subprotocol_; }
    const std::vector<WebSocketExtension>& selected_extensions() const { return selected_extensions_; }
    
private:
    http::HttpHeaders headers_;
    std::chrono::milliseconds timeout_{30000};
    std::string subprotocol_;
    std::vector<WebSocketExtension> extensions_;
    
    // Handshake result
    std::string selected_subprotocol_;
    std::vector<WebSocketExtension> selected_extensions_;
    
    // Handshake methods
    Result<http::HttpRequest> create_handshake_request(std::string_view host, Port port, 
                                                      std::string_view path);
    Result<void> validate_handshake_response(const http::HttpResponse& response, 
                                            std::string_view websocket_key);
    std::string generate_websocket_key();
    std::string calculate_websocket_accept(std::string_view key);
    
    struct UrlParts {
        std::string scheme;
        std::string host;
        Port port;
        std::string path;
    };
    
    Result<UrlParts> parse_url(std::string_view url) const;
};

/**
 * @brief WebSocket server implementation
 */
class WebSocketServer {
public:
    using ConnectionHandler = std::function<void(std::unique_ptr<WebSocketConnection>)>;
    using HandshakeValidator = std::function<bool(const http::HttpRequest&)>;
    
    explicit WebSocketServer(Port port);
    WebSocketServer(const SocketAddress& bind_addr);
    
    // Move-only type
    WebSocketServer(const WebSocketServer&) = delete;
    WebSocketServer& operator=(const WebSocketServer&) = delete;
    WebSocketServer(WebSocketServer&&) noexcept = default;
    WebSocketServer& operator=(WebSocketServer&&) noexcept = default;
    
    // Server control
    Result<void> start();
    void stop();
    bool is_running() const { return running_; }
    
    // Event handlers
    void set_connection_handler(ConnectionHandler handler) { connection_handler_ = std::move(handler); }
    void set_handshake_validator(HandshakeValidator validator) { handshake_validator_ = std::move(validator); }
    
    // Settings
    void set_max_connections(int max_conn) { max_connections_ = max_conn; }
    void add_supported_subprotocol(std::string_view protocol);
    void add_supported_extension(WebSocketExtension extension);
    
    // Server info
    SocketAddress local_address() const;
    size_t active_connections() const { return active_connections_; }
    
private:
    SocketAddress bind_addr_;
    std::unique_ptr<tcp::TcpServer> tcp_server_;
    
    // Event handlers
    ConnectionHandler connection_handler_;
    HandshakeValidator handshake_validator_;
    
    // Settings
    int max_connections_ = 100;
    std::vector<std::string> supported_subprotocols_;
    std::vector<WebSocketExtension> supported_extensions_;
    
    // State
    std::atomic<bool> running_{false};
    std::atomic<size_t> active_connections_{0};
    
    // Handshake handling
    void handle_client(tcp::TcpConnection connection);
    Result<http::HttpResponse> process_handshake(const http::HttpRequest& request);
    bool validate_websocket_request(const http::HttpRequest& request);
    std::string select_subprotocol(const std::string& requested_protocols);
    std::vector<WebSocketExtension> select_extensions(const std::string& requested_extensions);
    std::string generate_websocket_accept(std::string_view key);
};

// Utility functions
namespace websocket_utils {

/**
 * @brief WebSocket protocol constants
 */
namespace constants {
    constexpr char WEBSOCKET_MAGIC_STRING[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    constexpr char WEBSOCKET_VERSION[] = "13";
    constexpr size_t MAX_CONTROL_FRAME_SIZE = 125;
    constexpr size_t MASKING_KEY_SIZE = 4;
}

/**
 * @brief Convert opcode to string
 */
std::string_view opcode_to_string(WebSocketOpcode opcode);

/**
 * @brief Convert close code to string
 */
std::string_view close_code_to_string(WebSocketCloseCode code);

/**
 * @brief Convert state to string
 */
std::string_view state_to_string(WebSocketState state);

/**
 * @brief Validate UTF-8 encoding for text frames
 */
bool is_valid_utf8(const std::vector<uint8_t>& data);

/**
 * @brief Generate random masking key
 */
uint32_t generate_masking_key();

/**
 * @brief Apply XOR masking to payload
 */
void apply_masking(std::vector<uint8_t>& payload, uint32_t masking_key);

/**
 * @brief Calculate SHA-1 hash (for WebSocket handshake)
 */
std::string sha1_hash(std::string_view input);

/**
 * @brief Base64 encode
 */
std::string base64_encode(const std::vector<uint8_t>& input);
std::string base64_encode(std::string_view input);

/**
 * @brief Base64 decode
 */
Result<std::vector<uint8_t>> base64_decode(std::string_view input);

/**
 * @brief Parse extension list from header
 */
std::vector<std::string> parse_extension_list(std::string_view extensions);

/**
 * @brief Parse subprotocol list from header
 */
std::vector<std::string> parse_subprotocol_list(std::string_view protocols);

/**
 * @brief Build extension header value
 */
std::string build_extension_header(const std::vector<WebSocketExtension>& extensions);

/**
 * @brief Build subprotocol header value
 */
std::string build_subprotocol_header(const std::vector<std::string>& protocols);

/**
 * @brief Validate WebSocket frame
 */
Result<void> validate_frame(const WebSocketFrame& frame);

/**
 * @brief Create close frame with code and reason
 */
WebSocketFrame create_close_frame(WebSocketCloseCode code, std::string_view reason = "");

/**
 * @brief Parse close frame payload
 */
struct CloseInfo {
    WebSocketCloseCode code;
    std::string reason;
};

Result<CloseInfo> parse_close_frame(const WebSocketFrame& frame);

/**
 * @brief Simple WebSocket client for quick connections
 */
Result<std::string> simple_websocket_request(std::string_view url, std::string_view message,
                                           std::chrono::milliseconds timeout = std::chrono::seconds(30));

} // namespace websocket_utils

} // namespace networkquests::websocket