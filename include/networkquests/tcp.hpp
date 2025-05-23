#pragma once

#include "common.hpp"
#include "socket.hpp"
#include "logger.hpp"

#include <string>
#include <string_view>
#include <vector>
#include <functional>
#include <memory>
#include <thread>
#include <atomic>
#include <future>

namespace networkquests::tcp {

// Forward declarations
class TcpConnection;
class TcpClient;
class TcpServer;

// Message structure for TCP communication
struct Message {
    std::vector<std::byte> data;
    std::chrono::steady_clock::time_point timestamp;
    
    Message() : timestamp(std::chrono::steady_clock::now()) {}
    explicit Message(std::vector<std::byte> data) 
        : data(std::move(data)), timestamp(std::chrono::steady_clock::now()) {}
    explicit Message(std::string_view text) 
        : timestamp(std::chrono::steady_clock::now()) {
        const auto* bytes = reinterpret_cast<const std::byte*>(text.data());
        data.assign(bytes, bytes + text.size());
    }
    
    [[nodiscard]] std::string to_string() const {
        return std::string(reinterpret_cast<const char*>(data.data()), data.size());
    }
    
    [[nodiscard]] std::size_t size() const noexcept { return data.size(); }
    [[nodiscard]] bool empty() const noexcept { return data.empty(); }
};

// Connection class representing a single TCP connection
class TcpConnection {
public:
    explicit TcpConnection(Socket socket);
    
    // Non-copyable, movable (custom implementation needed due to atomic)
    TcpConnection(const TcpConnection&) = delete;
    TcpConnection& operator=(const TcpConnection&) = delete;
    TcpConnection(TcpConnection&& other) noexcept;
    TcpConnection& operator=(TcpConnection&& other) noexcept;
    
    ~TcpConnection();
    
    // Message operations
    [[nodiscard]] Result<void> send_message(const Message& message);
    [[nodiscard]] Result<Message> receive_message();
    
    // Raw data operations
    [[nodiscard]] Result<std::size_t> send(std::span<const std::byte> data);
    [[nodiscard]] Result<std::size_t> receive(std::span<std::byte> buffer);
    
    // Connection management
    void close();
    [[nodiscard]] bool is_connected() const noexcept;
    
    // Connection information
    [[nodiscard]] Result<SocketAddress> local_address() const;
    [[nodiscard]] Result<SocketAddress> remote_address() const;
    
    // Socket access
    [[nodiscard]] Socket& socket() noexcept { return socket_; }
    [[nodiscard]] const Socket& socket() const noexcept { return socket_; }

private:
    Socket socket_;
    std::atomic<bool> connected_{true};
    
    // Helper methods for message protocol (length-prefixed messages)
    [[nodiscard]] Result<void> send_raw(std::span<const std::byte> data);
    [[nodiscard]] Result<std::vector<std::byte>> receive_raw();
};

// TCP Client class
class TcpClient {
public:
    TcpClient() = default;
    explicit TcpClient(AddressFamily family);
    
    // Non-copyable, movable
    TcpClient(const TcpClient&) = delete;
    TcpClient& operator=(const TcpClient&) = delete;
    TcpClient(TcpClient&&) = default;
    TcpClient& operator=(TcpClient&&) = default;
    
    // Connection management
    [[nodiscard]] Result<TcpConnection> connect(const SocketAddress& server_address);
    [[nodiscard]] Result<TcpConnection> connect(std::string_view host, Port port);
    
    // Convenience methods for simple operations
    [[nodiscard]] Result<std::string> send_request(const SocketAddress& server_address, 
                                                   std::string_view request);
    [[nodiscard]] Result<std::string> send_request(std::string_view host, Port port,
                                                   std::string_view request);
    
    // Configuration
    void set_timeout(const Timeout& timeout) { timeout_ = timeout; }
    [[nodiscard]] const Timeout& timeout() const noexcept { return timeout_; }

private:
    AddressFamily family_ = AddressFamily::IPv4;
    Timeout timeout_ = DEFAULT_TIMEOUT;
};

// TCP Server class
class TcpServer {
public:
    // Connection handler callback type
    using ConnectionHandler = std::function<void(TcpConnection)>;
    
    TcpServer() = default;
    explicit TcpServer(Port port, AddressFamily family = AddressFamily::IPv4);
    
    // Non-copyable, non-movable (due to atomic members and threads)
    TcpServer(const TcpServer&) = delete;
    TcpServer& operator=(const TcpServer&) = delete;
    TcpServer(TcpServer&&) = delete;
    TcpServer& operator=(TcpServer&&) = delete;
    
    ~TcpServer();
    
    // Server lifecycle
    [[nodiscard]] Result<void> bind(const SocketAddress& address);
    [[nodiscard]] Result<void> bind(Port port);
    [[nodiscard]] Result<void> listen(int backlog = 5);
    [[nodiscard]] Result<void> start(ConnectionHandler handler);
    void stop();
    
    // Single connection accept
    [[nodiscard]] Result<TcpConnection> accept_connection();
    
    // Server status
    [[nodiscard]] bool is_running() const noexcept { return running_; }
    [[nodiscard]] Result<SocketAddress> local_address() const;
    
    // Configuration
    void set_reuse_address(bool enable = true) { reuse_address_ = enable; }

private:
    Socket server_socket_;
    std::atomic<bool> running_{false};
    std::unique_ptr<std::thread> accept_thread_;
    ConnectionHandler connection_handler_;
    
    // Configuration
    bool reuse_address_ = true;
    AddressFamily family_ = AddressFamily::IPv4;
    
    void accept_loop();
    void handle_connection(TcpConnection connection);
};

// Utility functions
namespace utils {

[[nodiscard]] Result<void> send_file(TcpConnection& connection, std::string_view file_path);
[[nodiscard]] Result<void> receive_file(TcpConnection& connection, std::string_view file_path);

[[nodiscard]] Result<std::string> download_string(std::string_view host, Port port, 
                                                  std::string_view request);
[[nodiscard]] Result<void> upload_string(std::string_view host, Port port, 
                                         std::string_view data);

} // namespace utils

// Protocol trait
struct TcpProtocol {
    using ClientType = TcpClient;
    using ServerType = TcpServer;
    using ConnectionType = TcpConnection;
    using MessageType = Message;
    
    static constexpr std::string_view protocol_name() { return "TCP"; }
    static constexpr Port default_port() { return 8080; }
};

} // namespace networkquests::tcp