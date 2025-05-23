# NetworkQuests Architecture

This document provides a comprehensive overview of the NetworkQuests architecture, design principles, and implementation details.

## Table of Contents

1. [Overview](#overview)
2. [Design Principles](#design-principles)
3. [Project Structure](#project-structure)
4. [Core Components](#core-components)
5. [Protocol Implementations](#protocol-implementations)
6. [Build System](#build-system)
7. [Error Handling](#error-handling)
8. [Memory Management](#memory-management)
9. [Threading Model](#threading-model)
10. [Extensibility](#extensibility)

## Overview

NetworkQuests is designed as a comprehensive, educational C++20 network protocol library. The architecture emphasizes:

- **Educational Value**: Clear, well-documented implementations that facilitate learning
- **Modularity**: Independent protocol implementations with shared utilities
- **Modern C++**: Extensive use of C++20 features and best practices
- **Cross-Platform**: Support for Windows, Linux, and macOS
- **Production Ready**: Code suitable for real-world applications

## Design Principles

### 1. Separation of Concerns

Each protocol is implemented as an independent module with minimal dependencies:

```
Core Utilities ← Protocol Implementations ← Examples/Tests
```

### 2. RAII and Resource Management

All resources (sockets, memory, threads) are managed using RAII principles:

```cpp
class TcpConnection {
private:
    std::unique_ptr<Socket> socket_;  // Automatic cleanup
    std::thread worker_thread_;       // Proper thread management
};
```

### 3. Error Handling

Consistent error handling using `Result<T>` type eliminates exceptions in network code:

```cpp
Result<std::string> receive_data() {
    if (error_condition) {
        return make_error("Network timeout");
    }
    return make_success(data);
}
```

### 4. Type Safety

Strong typing prevents common networking errors:

```cpp
enum class Port : uint16_t {};           // Port cannot be confused with other integers
class SocketAddress;                     // Address abstraction
enum class HttpStatus : int;             // HTTP status codes
```

### 5. Modern C++20 Features

Extensive use of modern C++ features:

- Concepts for template constraints
- Ranges for data processing
- Coroutines for asynchronous operations (where supported)
- Modules (future enhancement)

## Project Structure

```
NetworkQuests/
├── include/networkquests/       # Public headers
│   ├── common.hpp              # Core utilities and types
│   ├── logger.hpp              # Logging system
│   ├── socket.hpp              # Socket abstraction
│   ├── tcp.hpp                 # TCP protocol
│   ├── udp.hpp                 # UDP protocol
│   ├── http.hpp                # HTTP protocol
│   ├── dns.hpp                 # DNS protocol
│   ├── ftp.hpp                 # FTP protocol
│   ├── websocket.hpp           # WebSocket protocol
│   ├── smtp.hpp                # SMTP protocol
│   └── snmp.hpp                # SNMP protocol
├── src/                        # Implementation files
│   ├── utils/                  # Core utilities
│   │   ├── common.cpp          # Result<T>, SocketAddress
│   │   ├── logger.cpp          # Thread-safe logging
│   │   └── socket.cpp          # Cross-platform sockets
│   ├── tcp/                    # TCP implementation
│   ├── udp/                    # UDP implementation
│   ├── http/                   # HTTP implementation
│   ├── dns/                    # DNS implementation
│   ├── ftp/                    # FTP implementation
│   ├── websocket/              # WebSocket implementation
│   ├── smtp/                   # SMTP implementation
│   └── snmp/                   # SNMP implementation
├── examples/                   # Example applications
│   ├── legacy/                 # Original FlatBuffers project
│   └── [protocol]_[type].cpp   # Protocol examples
├── docs/                       # Documentation
├── tests/                      # Test suites
├── cmake/                      # CMake modules
└── scripts/                    # Build and utility scripts
```

## Core Components

### 1. Result<T> Type

Custom error handling type that replaces exceptions in network code:

```cpp
template<typename T>
class Result {
public:
    // Success constructor
    Result(T&& value);
    
    // Error constructor
    Result(std::string error);
    
    // Check if successful
    bool has_value() const;
    operator bool() const;
    
    // Access value (only if successful)
    const T& value() const;
    T& value();
    
    // Access error (only if failed)
    const std::string& error() const;
    
private:
    std::variant<T, std::string> data_;
};
```

### 2. Socket Abstraction

Cross-platform socket wrapper:

```cpp
class Socket {
public:
    // Factory methods
    static Result<Socket> create_tcp();
    static Result<Socket> create_udp();
    
    // Core operations
    Result<void> bind(const SocketAddress& addr);
    Result<void> listen(int backlog = SOMAXCONN);
    Result<Socket> accept();
    Result<void> connect(const SocketAddress& addr);
    
    // Data transfer
    Result<size_t> send(std::span<const uint8_t> data);
    Result<std::vector<uint8_t>> receive(size_t max_size);
    
    // Configuration
    Result<void> set_reuse_address(bool enable);
    Result<void> set_timeout(std::chrono::milliseconds timeout);
    
private:
    socket_t handle_;  // Platform-specific socket handle
    SocketType type_;
};
```

### 3. Address Management

Type-safe address handling:

```cpp
class SocketAddress {
public:
    // Factory methods
    static SocketAddress from_string(std::string_view host, Port port);
    static SocketAddress any_ipv4(Port port);
    static SocketAddress any_ipv6(Port port);
    
    // Properties
    std::string ip_string() const;
    Port port() const;
    bool is_ipv4() const;
    bool is_ipv6() const;
    
    // Conversion
    std::string to_string() const;
    
private:
    std::variant<sockaddr_in, sockaddr_in6> addr_;
};
```

### 4. Logging System

Thread-safe, configurable logging:

```cpp
class Logger {
public:
    // Configuration
    static void set_level(LogLevel level);
    static void set_output_file(const std::string& filename);
    static void enable_console_output(bool enable);
    
    // Logging methods
    static void log(LogLevel level, std::string_view component, std::string_view message);
    
private:
    static std::mutex mutex_;
    static LogLevel current_level_;
    static std::ofstream output_file_;
    static bool console_enabled_;
};

// Convenience macros
#define LOG_DEBUG(component, message) Logger::log(LogLevel::DEBUG, component, message)
#define LOG_INFO(component, message)  Logger::log(LogLevel::INFO, component, message)
#define LOG_WARN(component, message)  Logger::log(LogLevel::WARN, component, message)
#define LOG_ERROR(component, message) Logger::log(LogLevel::ERROR, component, message)
```

## Protocol Implementations

### Protocol Architecture Pattern

Each protocol follows a consistent architecture:

```cpp
namespace networkquests::[protocol] {
    // Core protocol classes
    class [Protocol]Client {
        // Client-side functionality
        Result<void> connect(const SocketAddress& addr);
        Result<Response> send_request(const Request& req);
    };
    
    class [Protocol]Server {
        // Server-side functionality
        Result<void> start();
        void stop();
        void add_handler(Handler handler);
    };
    
    // Protocol-specific types
    class [Protocol]Message { /* ... */ };
    enum class [Protocol]Status { /* ... */ };
    
    // Utility functions
    namespace [protocol]_utils {
        // Protocol-specific utilities
    }
}
```

### Example: HTTP Implementation

```cpp
namespace networkquests::http {
    class HttpServer {
    public:
        explicit HttpServer(Port port);
        
        Result<void> start();
        void stop();
        
        void add_route(HttpMethod method, std::string_view path, RouteHandler handler);
        void add_middleware(Middleware middleware);
        void set_static_directory(std::string_view path);
        
    private:
        tcp::TcpServer tcp_server_;
        std::unordered_map<std::string, RouteHandler> routes_;
        std::vector<Middleware> middlewares_;
        std::thread server_thread_;
    };
    
    class HttpClient {
    public:
        Result<HttpResponse> get(std::string_view url);
        Result<HttpResponse> post(std::string_view url, std::string_view body);
        Result<HttpResponse> send(const HttpRequest& request);
        
    private:
        tcp::TcpClient tcp_client_;
        std::chrono::milliseconds timeout_;
    };
}
```

## Build System

### CMake Architecture

The build system uses modern CMake practices:

```cmake
# Main project
project(NetworkQuests VERSION 1.0.0)

# Core library
add_library(networkquests_core ${CORE_SOURCES})
target_include_directories(networkquests_core PUBLIC $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>)

# Protocol libraries
add_library(networkquests_tcp ${TCP_SOURCES})
target_link_libraries(networkquests_tcp PUBLIC networkquests_core)

# Interface library for users
add_library(networkquests INTERFACE)
target_link_libraries(networkquests INTERFACE networkquests_core networkquests_tcp ...)

# Export for installation
install(EXPORT NetworkQuestsTargets ...)
```

### Dependency Management

Dependencies are handled through find_package and FetchContent:

```cmake
# Required dependencies
find_package(Threads REQUIRED)

# Optional dependencies
if(ENABLE_BOOST)
    find_package(Boost COMPONENTS system)
endif()

# Testing framework
if(ENABLE_TESTING)
    find_package(Catch2 QUIET)
    if(NOT Catch2_FOUND)
        FetchContent_Declare(Catch2 ...)
        FetchContent_MakeAvailable(Catch2)
    endif()
endif()
```

## Error Handling

### Error Categories

NetworkQuests defines several error categories:

1. **System Errors**: OS-level socket errors
2. **Protocol Errors**: Protocol-specific errors (HTTP 404, DNS NXDOMAIN)
3. **Configuration Errors**: Invalid parameters or configuration
4. **Network Errors**: Connection failures, timeouts

### Error Propagation

Errors are propagated using the `Result<T>` type:

```cpp
Result<HttpResponse> fetch_url(std::string_view url) {
    // Each operation can fail and return an error
    auto socket_result = create_socket();
    if (!socket_result) {
        return make_error("Failed to create socket: " + socket_result.error());
    }
    
    auto connect_result = socket_result.value().connect(parse_url(url));
    if (!connect_result) {
        return make_error("Connection failed: " + connect_result.error());
    }
    
    // ... continue with successful operations
    return make_success(response);
}
```

### Error Context

Errors include context information:

```cpp
class NetworkError {
    std::string message_;
    std::string context_;
    std::optional<int> error_code_;
    std::chrono::system_clock::time_point timestamp_;
};
```

## Memory Management

### RAII Principles

All resources are managed through RAII:

```cpp
class TcpConnection {
public:
    TcpConnection(Socket&& socket) : socket_(std::move(socket)) {}
    
    ~TcpConnection() {
        // Socket automatically closed by destructor
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }
    }
    
    // Non-copyable, movable
    TcpConnection(const TcpConnection&) = delete;
    TcpConnection& operator=(const TcpConnection&) = delete;
    TcpConnection(TcpConnection&&) = default;
    TcpConnection& operator=(TcpConnection&&) = default;
    
private:
    Socket socket_;                    // Automatic cleanup
    std::thread worker_thread_;        // Proper thread management
    std::unique_ptr<Buffer> buffer_;   // Memory management
};
```

### Smart Pointers Usage

Strategic use of smart pointers:

```cpp
// Unique ownership
std::unique_ptr<HttpRequest> parse_request(std::string_view data);

// Shared ownership for thread-safe data
std::shared_ptr<const RouteTable> route_table_;

// Weak references to break cycles
std::weak_ptr<Server> parent_server_;
```

### Memory Pools

For high-performance scenarios, memory pools are used:

```cpp
class BufferPool {
public:
    std::unique_ptr<Buffer> acquire(size_t size);
    void release(std::unique_ptr<Buffer> buffer);
    
private:
    std::vector<std::unique_ptr<Buffer>> available_buffers_;
    std::mutex mutex_;
};
```

## Threading Model

### Thread Safety Levels

1. **Thread-Safe**: Can be safely used from multiple threads
2. **Thread-Compatible**: Safe with external synchronization
3. **Thread-Hostile**: Not safe for concurrent use

### Server Threading

Multi-threaded servers use a thread pool pattern:

```cpp
class HttpServer {
private:
    void server_loop() {
        while (running_) {
            auto connection = tcp_server_.accept();
            if (connection) {
                // Handle in thread pool
                thread_pool_.enqueue([this, conn = std::move(connection.value())]() {
                    handle_connection(std::move(conn));
                });
            }
        }
    }
    
    ThreadPool thread_pool_;
    std::atomic<bool> running_;
};
```

### Synchronization

Minimal synchronization for performance:

```cpp
// Lock-free when possible
std::atomic<size_t> connection_count_;

// Fine-grained locking
mutable std::shared_mutex route_table_mutex_;

// RAII lock guards
std::shared_lock<std::shared_mutex> lock(route_table_mutex_);
```

## Extensibility

### Adding New Protocols

To add a new protocol:

1. Create header file in `include/networkquests/`
2. Implement classes in `src/protocol_name/`
3. Add CMake configuration
4. Create examples and tests
5. Add documentation

### Plugin Architecture

Future enhancement for plugin-based protocols:

```cpp
class ProtocolPlugin {
public:
    virtual ~ProtocolPlugin() = default;
    virtual std::string_view name() const = 0;
    virtual Version version() const = 0;
    virtual Result<void> initialize(const Config& config) = 0;
    virtual std::unique_ptr<Client> create_client() = 0;
    virtual std::unique_ptr<Server> create_server() = 0;
};

class ProtocolRegistry {
public:
    void register_plugin(std::unique_ptr<ProtocolPlugin> plugin);
    std::unique_ptr<Client> create_client(std::string_view protocol);
    std::unique_ptr<Server> create_server(std::string_view protocol);
};
```

### Custom Middleware

HTTP middleware system for extensibility:

```cpp
using Middleware = std::function<HttpResponse(const HttpRequest&, NextFunction)>;

class HttpServer {
public:
    void add_middleware(Middleware middleware) {
        middlewares_.push_back(std::move(middleware));
    }
    
private:
    std::vector<Middleware> middlewares_;
    
    HttpResponse process_request(const HttpRequest& request) {
        return execute_middleware_chain(request, middlewares_.begin());
    }
};
```

---

This architecture provides a solid foundation for network protocol implementation while maintaining educational value, performance, and extensibility. The modular design allows users to include only the protocols they need while providing a consistent API across all implementations.