# WebSocket Protocol Implementation Guide

## Table of Contents
1. [Protocol Overview](#protocol-overview)
2. [WebSocket Theory](#websocket-theory)
3. [Implementation Architecture](#implementation-architecture)
4. [API Reference](#api-reference)
5. [Usage Examples](#usage-examples)
6. [Best Practices](#best-practices)
7. [Troubleshooting](#troubleshooting)

## Protocol Overview

WebSocket is a computer communications protocol that provides full-duplex communication channels over a single TCP connection. Defined in RFC 6455, it enables real-time, bidirectional communication between web browsers (or other client applications) and web servers.

### Key Features
- **Full-Duplex Communication**: Both client and server can send data simultaneously
- **Low Latency**: No HTTP overhead after handshake completion
- **Persistent Connection**: Single connection for entire session
- **Frame-Based Protocol**: Efficient message framing with minimal overhead
- **Text and Binary Support**: Handle both text and binary data
- **Built-in Keep-Alive**: Ping/Pong frames for connection monitoring

### Use Cases
- **Real-time Chat Applications**: Instant messaging and group chat
- **Live Updates**: Stock prices, sports scores, news feeds
- **Gaming**: Multiplayer online games requiring low latency
- **Collaboration Tools**: Shared documents, whiteboards
- **IoT Communication**: Device monitoring and control
- **Financial Trading**: Real-time market data

## WebSocket Theory

### Connection Lifecycle

```
Client                    Server
   |                        |
   |--- HTTP Upgrade -----> |  (1) Handshake Request
   |                        |
   |<-- 101 Switching ------|  (2) Handshake Response
   |                        |
   |<====== Data Frames =====>  (3) Bidirectional Communication
   |                        |
   |--- Close Frame ------> |  (4) Close Handshake
   |<-- Close Frame -------|
   |                        |
   |     TCP Close          |  (5) Connection Termination
```

### HTTP Upgrade Handshake

The WebSocket connection begins with an HTTP upgrade request:

**Client Request:**
```http
GET /chat HTTP/1.1
Host: server.example.com
Upgrade: websocket
Connection: Upgrade
Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==
Sec-WebSocket-Version: 13
```

**Server Response:**
```http
HTTP/1.1 101 Switching Protocols
Upgrade: websocket
Connection: Upgrade
Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=
```

### Frame Format

WebSocket frames have a specific binary format:

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-------+-+-------------+-------------------------------+
|F|R|R|R| opcode|M| Payload len |    Extended payload length    |
|I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
|N|V|V|V|       |S|             |   (if payload len==126/127)   |
| |1|2|3|       |K|             |                               |
+-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
|     Extended payload length continued, if payload len == 127  |
+ - - - - - - - - - - - - - - - +-------------------------------+
|                               |Masking-key, if MASK set to 1  |
+-------------------------------+-------------------------------+
| Masking-key (continued)       |          Payload Data         |
+-------------------------------- - - - - - - - - - - - - - - - +
:                     Payload Data continued ...                :
+ - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
|                     Payload Data continued ...                |
+---------------------------------------------------------------+
```

### Frame Types

| Opcode | Frame Type | Description |
|--------|------------|-------------|
| 0x0    | Continuation | Continuation of a fragmented message |
| 0x1    | Text       | UTF-8 text data |
| 0x2    | Binary     | Binary data |
| 0x8    | Close      | Connection close |
| 0x9    | Ping       | Ping frame |
| 0xA    | Pong       | Pong frame |

### Masking

Client-to-server frames must be masked using XOR with a 32-bit random key:
```cpp
for (i = 0; i < payload_length; i++) {
    decoded[i] = encoded[i] ^ mask[i % 4];
}
```

## Implementation Architecture

### Core Classes

```cpp
namespace networkquests::websocket {
    class WebSocketClient;      // Client implementation
    class WebSocketServer;      // Server implementation
    class WebSocketConnection;  // Connection management
    class WebSocketFrame;       // Frame handling
    class WebSocketMessage;     // Message assembly
}
```

### Class Relationships

```
WebSocketClient ─┐
                 ├─► WebSocketConnection ──► WebSocketFrame
WebSocketServer ─┘                      └─► WebSocketMessage
```

### Thread Safety

- **WebSocketConnection**: Thread-safe for concurrent send/receive
- **WebSocketClient**: Not thread-safe (single-threaded usage)
- **WebSocketServer**: Thread-safe server with multi-client support
- **Frame Processing**: Atomic operations for state management

## API Reference

### WebSocketClient

```cpp
class WebSocketClient {
public:
    // Connection
    Result<std::unique_ptr<WebSocketConnection>> connect(std::string_view url);
    Result<std::unique_ptr<WebSocketConnection>> connect(std::string_view host, 
                                                        Port port, 
                                                        std::string_view path = "/");
    
    // Configuration
    void set_timeout(std::chrono::milliseconds timeout);
    void set_headers(const http::HttpHeaders& headers);
    void add_header(std::string_view name, std::string_view value);
    void set_subprotocol(std::string_view protocol);
    void set_extensions(const std::vector<WebSocketExtension>& extensions);
    
    // Handshake information
    const std::string& selected_subprotocol() const;
    const std::vector<WebSocketExtension>& selected_extensions() const;
};
```

### WebSocketServer

```cpp
class WebSocketServer {
public:
    using ConnectionHandler = std::function<void(std::unique_ptr<WebSocketConnection>)>;
    using HandshakeValidator = std::function<bool(const http::HttpRequest&)>;
    
    // Server lifecycle
    Result<void> start();
    void stop();
    bool is_running() const;
    
    // Event handlers
    void set_connection_handler(ConnectionHandler handler);
    void set_handshake_validator(HandshakeValidator validator);
    
    // Configuration
    void set_max_connections(int max_conn);
    void add_supported_subprotocol(std::string_view protocol);
    void add_supported_extension(WebSocketExtension extension);
    
    // Server information
    SocketAddress local_address() const;
    size_t active_connections() const;
};
```

### WebSocketConnection

```cpp
class WebSocketConnection {
public:
    using MessageHandler = std::function<void(const WebSocketMessage&)>;
    using CloseHandler = std::function<void(WebSocketCloseCode, std::string_view)>;
    using ErrorHandler = std::function<void(const std::string&)>;
    
    // Connection state
    WebSocketState state() const;
    bool is_connected() const;
    bool is_client() const;
    
    // Message sending
    Result<void> send(const WebSocketMessage& message);
    Result<void> send_text(std::string_view text);
    Result<void> send_binary(const std::vector<uint8_t>& data);
    Result<void> send_ping(std::string_view data = "");
    Result<void> send_pong(std::string_view data = "");
    Result<void> close(WebSocketCloseCode code = WebSocketCloseCode::Normal, 
                      std::string_view reason = "");
    
    // Message receiving
    Result<WebSocketMessage> receive_message(std::chrono::milliseconds timeout);
    void start_async_receive();
    void stop_async_receive();
    
    // Event handlers
    void set_message_handler(MessageHandler handler);
    void set_close_handler(CloseHandler handler);
    void set_error_handler(ErrorHandler handler);
    
    // Configuration
    void set_max_message_size(size_t size);
    void set_ping_interval(std::chrono::milliseconds interval);
    void enable_auto_pong(bool enable);
    
    // Connection information
    SocketAddress local_address() const;
    SocketAddress remote_address() const;
};
```

### WebSocketMessage

```cpp
class WebSocketMessage {
public:
    // Constructors
    WebSocketMessage(WebSocketFrameType type, std::string_view data);
    WebSocketMessage(WebSocketFrameType type, const std::vector<uint8_t>& data);
    
    // Accessors
    WebSocketFrameType type() const;
    const std::vector<uint8_t>& data() const;
    bool is_complete() const;
    size_t size() const;
    
    // Operations
    std::string as_string() const;
    void clear();
    
    // Factory methods
    static WebSocketMessage text(std::string_view content);
    static WebSocketMessage binary(const std::vector<uint8_t>& content);
    static WebSocketMessage close(WebSocketCloseCode code, std::string_view reason);
    static WebSocketMessage ping(std::string_view data);
    static WebSocketMessage pong(std::string_view data);
};
```

## Usage Examples

### Basic Client

```cpp
#include "networkquests/websocket.hpp"

using namespace networkquests::websocket;

int main() {
    // Initialize networking
    Socket::initialize_platform();
    
    // Create client and connect
    WebSocketClient client;
    auto connection = client.connect("ws://echo.websocket.org").value();
    
    // Send message
    connection->send_text("Hello, WebSocket!");
    
    // Receive echo
    auto message = connection->receive_message(std::chrono::seconds(5)).value();
    std::cout << "Received: " << message.as_string() << std::endl;
    
    // Close connection
    connection->close();
    
    Socket::cleanup_platform();
    return 0;
}
```

### Basic Server

```cpp
#include "networkquests/websocket.hpp"

using namespace networkquests::websocket;

int main() {
    Socket::initialize_platform();
    
    // Create and start server
    WebSocketServer server(8080);
    
    server.set_connection_handler([](std::unique_ptr<WebSocketConnection> conn) {
        std::cout << "New client connected!" << std::endl;
        
        // Echo server: send back received messages
        conn->set_message_handler([conn = conn.get()](const WebSocketMessage& msg) {
            if (msg.type() == WebSocketFrameType::Text) {
                conn->send_text("Echo: " + msg.as_string());
            }
        });
        
        // Handle disconnection
        conn->set_close_handler([](WebSocketCloseCode code, std::string_view reason) {
            std::cout << "Client disconnected: " << static_cast<int>(code) << std::endl;
        });
        
        // Start receiving messages
        conn->start_async_receive();
    });
    
    server.start();
    std::cout << "WebSocket server running on port 8080" << std::endl;
    
    // Keep server running
    std::string input;
    std::getline(std::cin, input);
    
    server.stop();
    Socket::cleanup_platform();
    return 0;
}
```

### Chat Application

```cpp
class ChatServer {
private:
    WebSocketServer server_;
    std::unordered_map<std::string, std::shared_ptr<WebSocketConnection>> clients_;
    std::mutex clients_mutex_;
    
public:
    ChatServer(Port port) : server_(port) {}
    
    void start() {
        server_.set_connection_handler([this](std::unique_ptr<WebSocketConnection> conn) {
            auto client_id = generate_client_id();
            auto shared_conn = std::shared_ptr<WebSocketConnection>(std::move(conn));
            
            // Add to client list
            {
                std::lock_guard<std::mutex> lock(clients_mutex_);
                clients_[client_id] = shared_conn;
            }
            
            // Handle messages
            shared_conn->set_message_handler([this, client_id](const WebSocketMessage& msg) {
                if (msg.type() == WebSocketFrameType::Text) {
                    broadcast_message(client_id + ": " + msg.as_string(), client_id);
                }
            });
            
            // Handle disconnection
            shared_conn->set_close_handler([this, client_id](WebSocketCloseCode, std::string_view) {
                std::lock_guard<std::mutex> lock(clients_mutex_);
                clients_.erase(client_id);
                broadcast_message(client_id + " left the chat", client_id);
            });
            
            shared_conn->start_async_receive();
            broadcast_message(client_id + " joined the chat", client_id);
        });
        
        server_.start();
    }
    
private:
    void broadcast_message(const std::string& message, const std::string& sender = "") {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        
        for (auto& [id, conn] : clients_) {
            if (id != sender && conn && conn->is_connected()) {
                conn->send_text(message);
            }
        }
    }
    
    std::string generate_client_id() {
        static std::atomic<int> counter{0};
        return "user_" + std::to_string(counter.fetch_add(1));
    }
};
```

### Async Message Handling

```cpp
class AsyncWebSocketClient {
private:
    std::unique_ptr<WebSocketConnection> connection_;
    std::queue<WebSocketMessage> message_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    
public:
    void connect(const std::string& url) {
        WebSocketClient client;
        connection_ = client.connect(url).value();
        
        // Set up async message handling
        connection_->set_message_handler([this](const WebSocketMessage& msg) {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            message_queue_.push(msg);
            queue_cv_.notify_one();
        });
        
        connection_->start_async_receive();
    }
    
    void send_message(const std::string& text) {
        if (connection_ && connection_->is_connected()) {
            connection_->send_text(text);
        }
    }
    
    WebSocketMessage wait_for_message() {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        queue_cv_.wait(lock, [this] { return !message_queue_.empty(); });
        
        auto msg = message_queue_.front();
        message_queue_.pop();
        return msg;
    }
};
```

## Best Practices

### Connection Management

1. **Always Check Connection State**
```cpp
if (connection->is_connected()) {
    connection->send_text("Hello");
}
```

2. **Handle Connection Errors**
```cpp
connection->set_error_handler([](const std::string& error) {
    logger.error("WebSocket error: {}", error);
    // Implement reconnection logic
});
```

3. **Implement Graceful Shutdown**
```cpp
// Send close frame before destroying connection
connection->close(WebSocketCloseCode::Normal, "Client shutting down");
```

### Message Handling

1. **Validate Message Types**
```cpp
connection->set_message_handler([](const WebSocketMessage& msg) {
    switch (msg.type()) {
        case WebSocketFrameType::Text:
            handle_text_message(msg.as_string());
            break;
        case WebSocketFrameType::Binary:
            handle_binary_message(msg.data());
            break;
        default:
            // Log unexpected message type
            break;
    }
});
```

2. **Handle Large Messages**
```cpp
// Set reasonable limits
connection->set_max_message_size(1024 * 1024); // 1MB limit
```

3. **Use Async Receive for Real-time Applications**
```cpp
// Start async receive for real-time handling
connection->start_async_receive();

// Use sync receive for request-response patterns
auto response = connection->receive_message(std::chrono::seconds(30));
```

### Server Development

1. **Validate Handshakes**
```cpp
server.set_handshake_validator([](const http::HttpRequest& request) {
    // Check origin, path, authentication, etc.
    auto origin = request.get_header("Origin");
    return origin && is_allowed_origin(origin.value());
});
```

2. **Limit Connections**
```cpp
server.set_max_connections(1000);
```

3. **Use Subprotocols for Different Services**
```cpp
server.add_supported_subprotocol("chat");
server.add_supported_subprotocol("notifications");
```

### Error Handling

1. **Implement Comprehensive Error Handling**
```cpp
auto result = connection->send_text("Hello");
if (!result) {
    logger.error("Send failed: {}", result.error().message);
    // Handle error (reconnect, retry, etc.)
}
```

2. **Handle Network Timeouts**
```cpp
try {
    auto message = connection->receive_message(std::chrono::seconds(30));
    // Process message
} catch (const std::exception& e) {
    logger.warn("Receive timeout: {}", e.what());
}
```

### Performance Optimization

1. **Use Connection Pooling for Clients**
```cpp
class WebSocketPool {
    std::queue<std::unique_ptr<WebSocketConnection>> available_connections_;
    // Implementation details...
};
```

2. **Batch Messages When Possible**
```cpp
// Instead of multiple small sends
connection->send_text("msg1");
connection->send_text("msg2");
connection->send_text("msg3");

// Use larger payloads
std::string batch = "msg1\nmsg2\nmsg3";
connection->send_text(batch);
```

3. **Monitor Connection Health**
```cpp
// Enable automatic ping/pong
connection->enable_auto_pong(true);
connection->set_ping_interval(std::chrono::seconds(30));
```

## Troubleshooting

### Common Issues

#### 1. Connection Handshake Failures

**Problem**: Client cannot establish WebSocket connection
```
Error: Invalid WebSocket handshake response
```

**Solutions**:
- Verify server URL format: `ws://` or `wss://`
- Check server is running and accepting connections
- Validate request headers and server response
- Ensure WebSocket protocol version compatibility

**Debug Steps**:
```cpp
// Enable detailed logging
auto result = client.connect("ws://localhost:8080");
if (!result) {
    std::cout << "Connection failed: " << result.error().message << std::endl;
}
```

#### 2. Frame Parsing Errors

**Problem**: Invalid frame format or corrupted data
```
Error: Insufficient data for WebSocket frame header
```

**Solutions**:
- Check network connectivity and data integrity
- Verify client/server masking compliance
- Ensure proper frame size limits

**Debug Steps**:
```cpp
connection->set_error_handler([](const std::string& error) {
    std::cout << "Frame error: " << error << std::endl;
    // Log raw frame data for analysis
});
```

#### 3. UTF-8 Validation Failures

**Problem**: Text frames contain invalid UTF-8
```
Error: Text frame contains invalid UTF-8
```

**Solutions**:
- Validate text data before sending
- Use binary frames for non-UTF-8 data
- Implement proper encoding conversion

**Example**:
```cpp
bool is_valid_utf8(const std::string& text) {
    // Implement UTF-8 validation
    return websocket_utils::is_valid_utf8(
        std::vector<uint8_t>(text.begin(), text.end())
    );
}

if (is_valid_utf8(message)) {
    connection->send_text(message);
} else {
    connection->send_binary(std::vector<uint8_t>(message.begin(), message.end()));
}
```

#### 4. Connection Drops and Timeouts

**Problem**: Unexpected connection closures
```
Error: Connection lost during operation
```

**Solutions**:
- Implement ping/pong keep-alive mechanism
- Handle network interruptions gracefully
- Add connection retry logic

**Example**:
```cpp
class ReconnectingWebSocket {
private:
    std::string url_;
    std::unique_ptr<WebSocketConnection> connection_;
    std::atomic<bool> should_reconnect_{true};
    
public:
    void connect_with_retry() {
        while (should_reconnect_) {
            try {
                WebSocketClient client;
                connection_ = client.connect(url_).value();
                
                connection_->set_close_handler([this](WebSocketCloseCode code, std::string_view) {
                    if (code != WebSocketCloseCode::Normal) {
                        // Unexpected closure, schedule reconnect
                        std::this_thread::sleep_for(std::chrono::seconds(5));
                        connect_with_retry();
                    }
                });
                
                connection_->start_async_receive();
                break; // Successfully connected
                
            } catch (const std::exception& e) {
                std::cout << "Connection failed, retrying in 5 seconds..." << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(5));
            }
        }
    }
};
```

#### 5. Server Performance Issues

**Problem**: Server becomes unresponsive under load
```
Error: Too many connections or high memory usage
```

**Solutions**:
- Implement connection limits and rate limiting
- Use efficient message queuing
- Monitor and cleanup inactive connections

**Example**:
```cpp
class OptimizedWebSocketServer {
private:
    WebSocketServer server_;
    std::unordered_map<std::string, ConnectionInfo> connections_;
    std::mutex connections_mutex_;
    
    struct ConnectionInfo {
        std::shared_ptr<WebSocketConnection> connection;
        std::chrono::steady_clock::time_point last_activity;
    };
    
public:
    void start_with_monitoring() {
        server_.set_max_connections(1000);
        
        // Cleanup inactive connections periodically
        std::thread cleanup_thread([this]() {
            while (running_) {
                cleanup_inactive_connections();
                std::this_thread::sleep_for(std::chrono::seconds(60));
            }
        });
        
        server_.start();
    }
    
private:
    void cleanup_inactive_connections() {
        auto now = std::chrono::steady_clock::now();
        std::lock_guard<std::mutex> lock(connections_mutex_);
        
        for (auto it = connections_.begin(); it != connections_.end();) {
            auto idle_time = now - it->second.last_activity;
            if (idle_time > std::chrono::minutes(30)) {
                it->second.connection->close(WebSocketCloseCode::GoingAway, "Timeout");
                it = connections_.erase(it);
            } else {
                ++it;
            }
        }
    }
};
```

### Debugging Tools

#### 1. Frame Analysis

```cpp
void debug_frame(const WebSocketFrame& frame) {
    std::cout << "Frame Debug Info:" << std::endl;
    std::cout << "  FIN: " << frame.fin() << std::endl;
    std::cout << "  Opcode: " << static_cast<int>(frame.opcode()) << std::endl;
    std::cout << "  Masked: " << frame.masked() << std::endl;
    std::cout << "  Payload Length: " << frame.payload_length() << std::endl;
    std::cout << "  Payload: " << frame.payload_as_string() << std::endl;
}
```

#### 2. Connection State Monitoring

```cpp
void monitor_connection_state(const WebSocketConnection& conn) {
    std::cout << "Connection State: " << websocket_utils::state_to_string(conn.state()) << std::endl;
    std::cout << "Local Address: " << conn.local_address().to_string() << std::endl;
    std::cout << "Remote Address: " << conn.remote_address().to_string() << std::endl;
    std::cout << "Is Client: " << (conn.is_client() ? "Yes" : "No") << std::endl;
}
```

#### 3. Performance Metrics

```cpp
class WebSocketMetrics {
private:
    std::atomic<uint64_t> messages_sent_{0};
    std::atomic<uint64_t> messages_received_{0};
    std::atomic<uint64_t> bytes_sent_{0};
    std::atomic<uint64_t> bytes_received_{0};
    
public:
    void record_send(size_t bytes) {
        messages_sent_.fetch_add(1);
        bytes_sent_.fetch_add(bytes);
    }
    
    void record_receive(size_t bytes) {
        messages_received_.fetch_add(1);
        bytes_received_.fetch_add(bytes);
    }
    
    void print_stats() {
        std::cout << "WebSocket Metrics:" << std::endl;
        std::cout << "  Messages Sent: " << messages_sent_.load() << std::endl;
        std::cout << "  Messages Received: " << messages_received_.load() << std::endl;
        std::cout << "  Bytes Sent: " << bytes_sent_.load() << std::endl;
        std::cout << "  Bytes Received: " << bytes_received_.load() << std::endl;
    }
};
```

This comprehensive guide provides everything needed to understand and effectively use the NetworkQuests WebSocket implementation. For additional examples and advanced usage patterns, refer to the example applications in the `examples/` directory.