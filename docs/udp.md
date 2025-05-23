# UDP Protocol Implementation

## Overview

The UDP (User Datagram Protocol) implementation in NetworkQuests provides a comprehensive, educational framework for working with UDP networking. Unlike TCP, UDP is connectionless and does not guarantee delivery, making it suitable for applications where speed is more important than reliability.

## Key Features

- **UdpSocket**: Low-level UDP socket wrapper with send/receive capabilities
- **UdpClient**: High-level client interface for simple UDP communication
- **UdpServer**: Server implementation for handling incoming UDP datagrams
- **Broadcasting Support**: Built-in support for UDP broadcast messages
- **Cross-platform**: Works on Windows, Linux, and macOS
- **Thread-safe**: Safe for use in multi-threaded applications
- **Modern C++20**: Uses latest C++ features and best practices

## UDP Protocol Theory

### What is UDP?

UDP is a lightweight, connectionless transport protocol that operates at Layer 4 (Transport Layer) of the OSI model. Key characteristics:

- **Connectionless**: No handshake process required
- **Unreliable**: No guarantee of delivery or order
- **Fast**: Minimal overhead compared to TCP
- **Stateless**: Each datagram is independent
- **Broadcast/Multicast Capable**: Can send to multiple recipients

### UDP vs TCP

| Feature | UDP | TCP |
|---------|-----|-----|
| Connection | Connectionless | Connection-oriented |
| Reliability | No guarantees | Reliable delivery |
| Ordering | No ordering | Ordered delivery |
| Overhead | Low | Higher |
| Speed | Fast | Slower |
| Use Cases | Gaming, Streaming, DNS | Web, Email, File Transfer |

### UDP Header Structure

```
 0      7 8     15 16    23 24    31
+--------+--------+--------+--------+
|     Source      |   Destination   |
|      Port       |      Port       |
+--------+--------+--------+--------+
|                 |                 |
|     Length      |    Checksum     |
+--------+--------+--------+--------+
|                                   |
|              DATA                 |
|                                   |
+-----------------------------------+
```

- **Source Port** (16 bits): Sender's port number
- **Destination Port** (16 bits): Receiver's port number  
- **Length** (16 bits): Total length of UDP header + data
- **Checksum** (16 bits): Error detection (optional in IPv4)

## Class Documentation

### UdpSocket

The core UDP socket wrapper providing both client and server functionality.

```cpp
#include "networkquests/udp.hpp"

// Create unbound UDP socket
UdpSocket socket;

// Create and bind to specific port
UdpSocket server_socket(8080);

// Create and bind to specific address
auto addr = SocketAddress::from_ipv4("192.168.1.100", 8080);
UdpSocket bound_socket(addr.value());
```

#### Key Methods

**Data Transfer:**
```cpp
// Send data to specific address
auto target = SocketAddress::from_ipv4("192.168.1.50", 9000);
auto result = socket.send_to("Hello, UDP!", target.value());

// Receive data from any sender
auto datagram = socket.receive_from(1024);
if (datagram) {
    std::cout << "From " << datagram.value().sender.to_string() 
              << ": " << datagram.value().to_string() << std::endl;
}
```

**Socket Options:**
```cpp
// Enable broadcasting
socket.enable_broadcast();

// Set timeouts
socket.set_receive_timeout(std::chrono::seconds(5));
socket.set_send_timeout(std::chrono::seconds(3));

// Non-blocking mode
socket.set_non_blocking(true);
```

### UdpClient

High-level client interface for simplified UDP communication.

```cpp
// Connect to UDP server
UdpClient client("localhost", 8080);

// Send message and get response
auto response = client.send_and_receive_text("Hello Server!", 1024, std::chrono::seconds(5));
if (response) {
    std::cout << "Server replied: " << response.value() << std::endl;
}

// Send without waiting for response
client.send("Fire and forget message");
```

### UdpServer

Server implementation for handling incoming UDP datagrams.

```cpp
// Create server listening on port 8080
UdpServer server(8080);

// Define datagram handler
auto handler = [](UdpDatagram datagram, UdpSocket& socket) {
    std::string message = datagram.to_string();
    std::cout << "Received: " << message << " from " 
              << datagram.sender.to_string() << std::endl;
    
    // Echo back to sender
    socket.send_to(datagram);
};

// Start server (blocking call)
server.start(handler);
```

### UdpDatagram

Represents a UDP datagram with data and sender information.

```cpp
struct UdpDatagram {
    std::vector<uint8_t> data;
    SocketAddress sender;
    
    std::string to_string() const;
    size_t size() const;
    bool empty() const;
};
```

## Usage Examples

### Simple Echo Server

```cpp
#include "networkquests/udp.hpp"
#include <iostream>

int main() {
    try {
        networkquests::socket_utils::NetworkingRAII networking;
        
        UdpServer server(8080);
        std::cout << "UDP Echo Server listening on port 8080\n";
        
        auto handler = [](UdpDatagram datagram, UdpSocket& socket) {
            std::cout << "Echo: " << datagram.to_string() << std::endl;
            socket.send_to(datagram); // Echo back
        };
        
        server.start(handler);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    
    return 0;
}
```

### Simple Client

```cpp
#include "networkquests/udp.hpp"
#include <iostream>

int main() {
    try {
        networkquests::socket_utils::NetworkingRAII networking;
        
        UdpClient client("localhost", 8080);
        
        std::string message = "Hello, UDP Server!";
        auto response = client.send_and_receive_text(message);
        
        if (response) {
            std::cout << "Server response: " << response.value() << std::endl;
        } else {
            std::cerr << "No response from server\n";
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    
    return 0;
}
```

### Broadcasting

```cpp
#include "networkquests/udp.hpp"

int main() {
    try {
        networkquests::socket_utils::NetworkingRAII networking;
        
        // Send broadcast message
        udp_utils::broadcast_message(9999, "Hello, everyone!");
        
        // Listen for broadcasts
        auto messages = udp_utils::listen_for_broadcasts(
            9999, 
            std::chrono::seconds(10), 
            5
        );
        
        if (messages) {
            for (const auto& msg : messages.value()) {
                std::cout << "Broadcast from " << msg.sender.to_string() 
                          << ": " << msg.to_string() << std::endl;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    
    return 0;
}
```

## Utility Functions

The `udp_utils` namespace provides convenient helper functions:

```cpp
namespace udp_utils {
    // Send UDP message and wait for response
    Result<std::string> send_udp_message(
        std::string_view host, Port port, 
        std::string_view message,
        std::optional<std::chrono::milliseconds> timeout = std::nullopt
    );
    
    // Run simple echo server
    void run_echo_server(Port port, std::atomic<bool>& should_stop);
    
    // Broadcast message to local network
    Result<void> broadcast_message(Port port, std::string_view message);
    
    // Listen for broadcast messages
    Result<std::vector<UdpDatagram>> listen_for_broadcasts(
        Port port, std::chrono::milliseconds duration, size_t max_messages = 10
    );
}
```

## Best Practices

### Error Handling

Always check return values and handle errors appropriately:

```cpp
auto result = socket.send_to(data, target);
if (!result) {
    std::cerr << "Send failed: " << result.error().message() << std::endl;
    // Handle error appropriately
}
```

### Timeouts

Set appropriate timeouts for receive operations:

```cpp
// Short timeout for real-time applications
socket.set_receive_timeout(std::chrono::milliseconds(100));

// Longer timeout for less time-sensitive operations
socket.set_receive_timeout(std::chrono::seconds(5));
```

### Buffer Sizes

Choose appropriate buffer sizes based on your application:

```cpp
// Small buffers for control messages
auto datagram = socket.receive_from(64);

// Larger buffers for data transfer
auto datagram = socket.receive_from(8192);

// Consider MTU size (typically 1500 bytes for Ethernet)
auto datagram = socket.receive_from(1472); // 1500 - 20 (IP) - 8 (UDP)
```

### Thread Safety

The UDP classes are thread-safe, but consider using separate sockets for different threads:

```cpp
// Server handling multiple clients
void handle_client_requests() {
    UdpSocket socket(8080);
    
    std::thread([&socket]() {
        while (running) {
            auto datagram = socket.receive_from();
            if (datagram) {
                // Process in thread pool
                thread_pool.submit([d = std::move(datagram.value())]() {
                    process_request(d);
                });
            }
        }
    });
}
```

## Performance Considerations

### UDP Performance Characteristics

- **Latency**: Very low due to minimal protocol overhead
- **Throughput**: High for bulk data transfer
- **CPU Usage**: Lower than TCP due to less processing
- **Memory**: Minimal buffering compared to TCP

### Optimization Tips

1. **Minimize System Calls**: Batch operations when possible
2. **Use Appropriate Buffer Sizes**: Match your data patterns
3. **Consider Kernel Bypass**: For high-performance applications
4. **Profile Your Application**: Measure actual performance

### Benchmarking

```cpp
#include <chrono>

// Measure round-trip time
auto start = std::chrono::high_resolution_clock::now();
auto response = client.send_and_receive_text("ping");
auto end = std::chrono::high_resolution_clock::now();

auto rtt = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
std::cout << "Round-trip time: " << rtt.count() << " µs" << std::endl;
```

## Common Use Cases

### 1. Real-time Gaming

```cpp
// Game state updates
struct GameState {
    float player_x, player_y;
    uint32_t timestamp;
};

UdpSocket game_socket;
GameState state = get_current_state();
game_socket.send_to(std::span<const uint8_t>(
    reinterpret_cast<const uint8_t*>(&state), sizeof(state)
), server_address);
```

### 2. DNS Resolution

```cpp
// Simple DNS query (educational purposes)
UdpClient dns_client("8.8.8.8", 53);
auto response = dns_client.send_and_receive(dns_query_packet);
```

### 3. Media Streaming

```cpp
// Video frame transmission
void send_video_frame(const VideoFrame& frame) {
    const size_t chunk_size = 1400; // Stay under MTU
    
    for (size_t offset = 0; offset < frame.size(); offset += chunk_size) {
        size_t size = std::min(chunk_size, frame.size() - offset);
        stream_socket.send_to(
            std::span<const uint8_t>(frame.data() + offset, size),
            client_address
        );
    }
}
```

### 4. Service Discovery

```cpp
// Broadcast service announcement
void announce_service() {
    ServiceInfo info{"MyService", "1.0", 8080};
    std::string announcement = serialize(info);
    
    udp_utils::broadcast_message(5000, announcement);
}

// Listen for service announcements
auto services = udp_utils::listen_for_broadcasts(5000, std::chrono::seconds(5));
```

## Troubleshooting

### Common Issues

1. **"Address already in use"**: Another process is using the port
2. **"Permission denied"**: Trying to bind to privileged port (<1024) without sudo
3. **Timeouts**: Network issues or server not responding
4. **Broadcasts not working**: Firewall blocking or wrong network interface

### Debugging Tips

```cpp
// Enable debug logging
Logger::instance().set_level(LogLevel::Debug);

// Check local address
auto addr = socket.local_address();
if (addr) {
    std::cout << "Socket bound to: " << addr.value().to_string() << std::endl;
}

// Verify network connectivity
auto result = udp_utils::send_udp_message("google.com", 53, "test");
```

## Advanced Topics

### Custom Protocols

Build application-specific protocols on top of UDP:

```cpp
struct ProtocolHeader {
    uint32_t magic;      // Protocol identifier
    uint16_t version;    // Protocol version
    uint16_t type;       // Message type
    uint32_t sequence;   // Sequence number
    uint32_t length;     // Payload length
};

class CustomProtocol {
    UdpSocket socket_;
    uint32_t next_sequence_ = 1;
    
public:
    void send_reliable_message(const std::vector<uint8_t>& data) {
        // Implement reliability on top of UDP
        // - Sequence numbers
        // - Acknowledgments
        // - Retransmission
    }
};
```

### Integration with Event Loops

```cpp
// Integration with epoll/kqueue/IOCP
class UdpEventHandler {
    UdpSocket socket_;
    
public:
    void on_readable() {
        socket_.set_non_blocking(true);
        
        while (true) {
            auto datagram = socket_.receive_from();
            if (!datagram) {
                if (datagram.error() == std::errc::operation_would_block) {
                    break; // No more data
                }
                // Handle other errors
            }
            
            process_datagram(datagram.value());
        }
    }
};
```

## Conclusion

The NetworkQuests UDP implementation provides a complete, educational framework for working with UDP networking in C++. It combines modern C++20 features with practical networking concepts, making it an excellent tool for learning network programming while building real applications.

The implementation emphasizes:
- **Educational value**: Clear documentation and examples
- **Modern practices**: C++20 features and RAII patterns
- **Real-world applicability**: Production-ready error handling and performance
- **Cross-platform compatibility**: Works on all major platforms

Whether you're building a real-time game, implementing a custom protocol, or just learning about UDP networking, this implementation provides the foundation you need.