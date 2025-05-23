# TCP (Transmission Control Protocol) Implementation

## Table of Contents

- [Overview](#overview)
- [Protocol Characteristics](#protocol-characteristics)
- [OSI/TCP-IP Layer](#ositcp-ip-layer)
- [Implementation Details](#implementation-details)
- [Usage Examples](#usage-examples)
- [API Reference](#api-reference)
- [Best Practices](#best-practices)
- [Performance Considerations](#performance-considerations)
- [References](#references)

## Overview

TCP (Transmission Control Protocol) is a connection-oriented, reliable transport layer protocol that provides ordered, error-checked delivery of data between applications running on hosts communicating over a network. This implementation provides a modern C++20 wrapper around TCP socket operations with built-in error handling, logging, and convenience features.

### Key Features

- **Reliability**: Guaranteed delivery of data in the correct order
- **Connection-oriented**: Establishes a connection before data transfer
- **Flow Control**: Manages data transmission rate between sender and receiver
- **Error Detection and Correction**: Ensures data integrity
- **Congestion Control**: Adapts to network conditions

## Protocol Characteristics

### Connection Management

TCP uses a three-way handshake to establish connections:

1. **SYN**: Client sends synchronization packet to server
2. **SYN-ACK**: Server responds with synchronization-acknowledgment
3. **ACK**: Client acknowledges, connection established

Connection termination uses a four-way handshake with FIN and ACK packets.

### Data Transmission

- **Segment-based**: Data is transmitted in segments with headers
- **Sequence Numbers**: Each byte has a sequence number for ordering
- **Acknowledgments**: Receiver confirms receipt of data
- **Retransmission**: Lost packets are automatically retransmitted

### Header Structure

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|          Source Port          |       Destination Port        |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                        Sequence Number                        |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                    Acknowledgment Number                      |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  Data |           |U|A|P|R|S|F|                               |
| Offset| Reserved  |R|C|S|S|Y|I|            Window             |
|       |           |G|K|H|T|N|N|                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|           Checksum            |         Urgent Pointer        |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                    Options                    |    Padding    |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                             data                              |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

## OSI/TCP-IP Layer

TCP operates at:
- **OSI Model**: Layer 4 (Transport Layer)
- **TCP/IP Model**: Layer 3 (Transport Layer)

### Role in the Network Stack

```
┌─────────────────────────────────────┐
│         Application Layer           │ ← HTTP, FTP, SMTP, etc.
├─────────────────────────────────────┤
│         Transport Layer             │ ← TCP (This Implementation)
├─────────────────────────────────────┤
│         Network Layer               │ ← IP
├─────────────────────────────────────┤
│         Data Link Layer             │ ← Ethernet, WiFi
├─────────────────────────────────────┤
│         Physical Layer              │ ← Cables, Radio Waves
└─────────────────────────────────────┘
```

## Implementation Details

### Message Protocol

Our TCP implementation uses a length-prefixed message protocol:

1. **Length Prefix**: 4-byte big-endian integer indicating message size
2. **Message Data**: The actual message content
3. **Maximum Size**: 10MB per message for safety

```cpp
// Message format:
// [4 bytes: length in network byte order][variable: message data]
```

### Class Architecture

```cpp
namespace networkquests::tcp {
    class TcpConnection;  // Represents a single TCP connection
    class TcpClient;      // Client-side TCP operations
    class TcpServer;      // Server-side TCP operations
    struct Message;       // Message container with timestamp
}
```

### Key Classes

#### TcpConnection
- Represents an established TCP connection
- Provides message-based and raw data communication
- Handles connection lifecycle and error states

#### TcpClient
- Initiates connections to TCP servers
- Supports hostname resolution
- Provides convenience methods for request-response patterns

#### TcpServer
- Accepts incoming TCP connections
- Multi-threaded connection handling
- Configurable connection callbacks

## Usage Examples

### Basic Echo Server

```cpp
#include "networkquests/tcp.hpp"

using namespace networkquests::tcp;

void handle_connection(TcpConnection connection) {
    while (connection.is_connected()) {
        auto message = connection.receive_message();
        if (message) {
            connection.send_message(message.value());
        }
    }
}

int main() {
    TcpServer server(8080);
    server.listen();
    server.start(handle_connection);
    
    // Server runs until stopped
    std::this_thread::sleep_for(std::chrono::seconds(60));
    return 0;
}
```

### Simple Client

```cpp
#include "networkquests/tcp.hpp"

using namespace networkquests::tcp;

int main() {
    TcpClient client;
    auto connection = client.connect("localhost", 8080);
    
    if (connection) {
        Message msg("Hello, Server!");
        connection.value().send_message(msg);
        
        auto response = connection.value().receive_message();
        if (response) {
            std::cout << "Server replied: " << response.value().to_string() << std::endl;
        }
    }
    
    return 0;
}
```

### File Transfer Server

```cpp
#include "networkquests/tcp.hpp"

void handle_file_request(TcpConnection connection) {
    auto request = connection.receive_message();
    if (request) {
        std::string filename = request.value().to_string();
        tcp::utils::send_file(connection, filename);
    }
}

int main() {
    TcpServer server(8080);
    server.listen();
    server.start(handle_file_request);
    
    std::cout << "File server running on port 8080\n";
    std::cin.get(); // Wait for user input
    return 0;
}
```

### Async Client (C++20 Coroutines)

```cpp
#include "networkquests/tcp.hpp"
#include <coroutine>

std::future<std::string> async_request(std::string host, Port port, std::string request) {
    TcpClient client;
    auto result = client.send_request(host, port, request);
    co_return result.value_or("Error");
}

int main() {
    auto future = async_request("httpbin.org", 80, "GET / HTTP/1.1\r\nHost: httpbin.org\r\n\r\n");
    std::cout << "Response: " << future.get() << std::endl;
    return 0;
}
```

## API Reference

### TcpConnection

```cpp
class TcpConnection {
public:
    // Message operations
    Result<void> send_message(const Message& message);
    Result<Message> receive_message();
    
    // Raw data operations
    Result<std::size_t> send(std::span<const std::byte> data);
    Result<std::size_t> receive(std::span<std::byte> buffer);
    
    // Connection management
    void close();
    bool is_connected() const noexcept;
    
    // Address information
    Result<SocketAddress> local_address() const;
    Result<SocketAddress> remote_address() const;
};
```

### TcpClient

```cpp
class TcpClient {
public:
    // Connection methods
    Result<TcpConnection> connect(const SocketAddress& server_address);
    Result<TcpConnection> connect(std::string_view host, Port port);
    
    // Convenience methods
    Result<std::string> send_request(const SocketAddress& server_address, 
                                   std::string_view request);
    Result<std::string> send_request(std::string_view host, Port port,
                                   std::string_view request);
    
    // Configuration
    void set_timeout(const Timeout& timeout);
};
```

### TcpServer

```cpp
class TcpServer {
public:
    using ConnectionHandler = std::function<void(TcpConnection)>;
    
    // Lifecycle
    Result<void> bind(const SocketAddress& address);
    Result<void> listen(int backlog = 5);
    Result<void> start(ConnectionHandler handler);
    void stop();
    
    // Status
    bool is_running() const noexcept;
    Result<SocketAddress> local_address() const;
};
```

## Best Practices

### Error Handling

Always check return values using the `Result<T>` type:

```cpp
auto connection = client.connect("localhost", 8080);
if (!connection) {
    std::cerr << "Connection failed: " << connection.error().message() << std::endl;
    return 1;
}

// Use the connection
auto& conn = connection.value();
```

### Resource Management

TCP connections are automatically closed when `TcpConnection` objects are destroyed (RAII):

```cpp
{
    auto connection = client.connect("localhost", 8080);
    // Use connection...
} // Connection automatically closed here
```

### Thread Safety

- `TcpConnection` objects are not thread-safe
- `TcpServer` handles multiple connections in separate threads
- Use proper synchronization when sharing connections between threads

### Performance Tips

1. **Reuse Connections**: Keep connections open for multiple operations
2. **Batching**: Send multiple small messages together when possible
3. **Buffering**: Use appropriate buffer sizes for your use case
4. **Async Operations**: Consider using async I/O for high-throughput applications

## Performance Considerations

### Throughput

- TCP's sliding window protocol provides good throughput for bulk transfers
- Window size affects performance; larger windows generally improve throughput
- Network latency affects performance more than bandwidth for small messages

### Latency

- TCP's reliability mechanisms add latency compared to UDP
- Nagle's algorithm may delay small packets (can be disabled with TCP_NODELAY)
- Connection establishment overhead (3-way handshake)

### Memory Usage

- TCP maintains connection state and buffers
- Our implementation uses length-prefixed messages with maximum 10MB size
- Consider message size limits for your application

### Benchmarks

Typical performance characteristics on modern hardware:

| Operation | Throughput | Latency |
|-----------|------------|---------|
| Local Echo | ~1GB/s | <1ms |
| LAN Transfer | ~100MB/s | 1-5ms |
| Internet Transfer | Variable | 10-200ms |

## References

### RFCs
- [RFC 793](https://tools.ietf.org/html/rfc793) - Transmission Control Protocol
- [RFC 1122](https://tools.ietf.org/html/rfc1122) - Requirements for Internet Hosts
- [RFC 2018](https://tools.ietf.org/html/rfc2018) - TCP Selective Acknowledgment Options
- [RFC 3168](https://tools.ietf.org/html/rfc3168) - Explicit Congestion Notification
- [RFC 7323](https://tools.ietf.org/html/rfc7323) - TCP Extensions for High Performance

### Additional Resources
- [TCP/IP Illustrated, Volume 1](https://www.amazon.com/TCP-Illustrated-Protocols-Addison-Wesley-Professional/dp/0321336313) by W. Richard Stevens
- [Computer Networks](https://www.amazon.com/Computer-Networks-Tanenbaum-International-Economy/dp/9332518742) by Andrew S. Tanenbaum
- [Berkeley Sockets API Documentation](https://man7.org/linux/man-pages/man7/socket.7.html)

### Example Applications
- Web servers (HTTP over TCP)
- Email servers (SMTP over TCP)
- File transfer (FTP over TCP)
- Remote login (SSH over TCP)
- Database connections