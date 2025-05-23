#pragma once

#include "networkquests/socket.hpp"
#include "networkquests/common.hpp"
#include "networkquests/logger.hpp"

#include <string>
#include <vector>
#include <chrono>
#include <functional>

namespace networkquests::udp {

/**
 * @brief Represents a UDP datagram with data and sender information
 */
struct UdpDatagram {
    std::vector<uint8_t> data;
    SocketAddress sender;
    
    UdpDatagram() = default;
    UdpDatagram(std::vector<uint8_t> data, const SocketAddress& sender)
        : data(std::move(data)), sender(sender) {}
    
    UdpDatagram(std::string_view text, const SocketAddress& sender)
        : data(text.begin(), text.end()), sender(sender) {}
    
    std::string to_string() const {
        return std::string(data.begin(), data.end());
    }
    
    size_t size() const { return data.size(); }
    bool empty() const { return data.empty(); }
};

/**
 * @brief UDP socket wrapper providing both client and server functionality
 * 
 * Unlike TCP, UDP is connectionless, so the same socket can be used for
 * both sending and receiving data to/from multiple peers.
 */
class UdpSocket {
private:
    std::unique_ptr<Socket> socket_;
    std::optional<SocketAddress> bound_address_;
    mutable std::mutex mutex_;

public:
    /**
     * @brief Create an unbound UDP socket
     */
    UdpSocket();
    
    /**
     * @brief Create and bind UDP socket to specific address
     */
    explicit UdpSocket(const SocketAddress& bind_addr);
    
    /**
     * @brief Create and bind UDP socket to specific port (any interface)
     */
    explicit UdpSocket(Port port);
    
    // Move-only type
    UdpSocket(const UdpSocket&) = delete;
    UdpSocket& operator=(const UdpSocket&) = delete;
    UdpSocket(UdpSocket&&) noexcept;
    UdpSocket& operator=(UdpSocket&&) noexcept;
    
    ~UdpSocket() = default;
    
    /**
     * @brief Bind socket to specific address
     */
    Result<void> bind(const SocketAddress& addr);
    
    /**
     * @brief Bind socket to specific port (any interface)
     */
    Result<void> bind(Port port);
    
    /**
     * @brief Send data to specific address
     */
    Result<size_t> send_to(std::span<const uint8_t> data, const SocketAddress& target);
    
    /**
     * @brief Send text to specific address
     */
    Result<size_t> send_to(std::string_view text, const SocketAddress& target);
    
    /**
     * @brief Send datagram to its original sender (for echo servers)
     */
    Result<size_t> send_to(const UdpDatagram& datagram);
    
    /**
     * @brief Receive data from any sender
     * @param max_size Maximum size of data to receive
     * @param timeout Optional timeout for receive operation
     */
    Result<UdpDatagram> receive_from(
        size_t max_size = 4096,
        std::optional<std::chrono::milliseconds> timeout = std::nullopt
    );
    
    /**
     * @brief Check if socket is bound to an address
     */
    bool is_bound() const;
    
    /**
     * @brief Get local bound address
     */
    std::optional<SocketAddress> local_address() const;
    
    /**
     * @brief Set socket options for broadcasting
     */
    Result<void> enable_broadcast();
    
    /**
     * @brief Set socket to non-blocking mode
     */
    Result<void> set_non_blocking(bool non_blocking = true);
    
    /**
     * @brief Set receive timeout
     */
    Result<void> set_receive_timeout(std::chrono::milliseconds timeout);
    
    /**
     * @brief Set send timeout
     */
    Result<void> set_send_timeout(std::chrono::milliseconds timeout);
    
    /**
     * @brief Get the underlying socket handle
     */
    SocketHandle handle() const;
};

/**
 * @brief UDP Client for sending data to servers
 */
class UdpClient {
private:
    UdpSocket socket_;
    SocketAddress server_address_;

public:
    /**
     * @brief Create UDP client for communicating with server
     */
    explicit UdpClient(const SocketAddress& server_addr);
    
    /**
     * @brief Create UDP client for communicating with server
     */
    UdpClient(std::string_view host, Port port);
    
    // Move-only type
    UdpClient(const UdpClient&) = delete;
    UdpClient& operator=(const UdpClient&) = delete;
    UdpClient(UdpClient&&) noexcept = default;
    UdpClient& operator=(UdpClient&&) noexcept = default;
    
    /**
     * @brief Send data to server
     */
    Result<size_t> send(std::span<const uint8_t> data);
    
    /**
     * @brief Send text to server
     */
    Result<size_t> send(std::string_view text);
    
    /**
     * @brief Receive response from server
     */
    Result<std::vector<uint8_t>> receive(
        size_t max_size = 4096,
        std::optional<std::chrono::milliseconds> timeout = std::nullopt
    );
    
    /**
     * @brief Send data and wait for response
     */
    Result<std::vector<uint8_t>> send_and_receive(
        std::span<const uint8_t> data,
        size_t max_response_size = 4096,
        std::optional<std::chrono::milliseconds> timeout = std::nullopt
    );
    
    /**
     * @brief Send text and wait for response
     */
    Result<std::string> send_and_receive_text(
        std::string_view text,
        size_t max_response_size = 4096,
        std::optional<std::chrono::milliseconds> timeout = std::nullopt
    );
    
    /**
     * @brief Get server address
     */
    const SocketAddress& server_address() const { return server_address_; }
    
    /**
     * @brief Get local address (if bound)
     */
    std::optional<SocketAddress> local_address() const;
};

/**
 * @brief UDP Server for handling incoming datagrams
 */
class UdpServer {
public:
    using DatagramHandler = std::function<void(UdpDatagram, UdpSocket&)>;

private:
    UdpSocket socket_;
    SocketAddress listen_address_;
    std::atomic<bool> running_{false};
    mutable std::mutex mutex_;

public:
    /**
     * @brief Create UDP server listening on specific address
     */
    explicit UdpServer(const SocketAddress& listen_addr);
    
    /**
     * @brief Create UDP server listening on specific port (any interface)
     */
    explicit UdpServer(Port port);
    
    // Move-only type
    UdpServer(const UdpServer&) = delete;
    UdpServer& operator=(const UdpServer&) = delete;
    UdpServer(UdpServer&&) noexcept = default;
    UdpServer& operator=(UdpServer&&) noexcept = default;
    
    /**
     * @brief Start the server and handle incoming datagrams
     * @param handler Function to handle each received datagram
     * @param max_datagram_size Maximum size of received datagrams
     */
    Result<void> start(
        DatagramHandler handler,
        size_t max_datagram_size = 4096
    );
    
    /**
     * @brief Stop the server
     */
    void stop();
    
    /**
     * @brief Check if server is running
     */
    bool is_running() const;
    
    /**
     * @brief Get local listening address
     */
    std::optional<SocketAddress> local_address() const;
    
    /**
     * @brief Enable broadcasting for the server socket
     */
    Result<void> enable_broadcast();
};

namespace udp_utils {

/**
 * @brief Send a single UDP message and wait for response
 */
Result<std::string> send_udp_message(
    std::string_view host,
    Port port,
    std::string_view message,
    std::optional<std::chrono::milliseconds> timeout = std::nullopt
);

/**
 * @brief Create a simple UDP echo server (for testing)
 */
void run_echo_server(Port port, std::atomic<bool>& should_stop);

/**
 * @brief Broadcast a message to the local network
 */
Result<void> broadcast_message(
    Port port,
    std::string_view message
);

/**
 * @brief Listen for broadcast messages
 */
Result<std::vector<UdpDatagram>> listen_for_broadcasts(
    Port port,
    std::chrono::milliseconds duration,
    size_t max_messages = 10
);

} // namespace udp_utils

} // namespace networkquests::udp