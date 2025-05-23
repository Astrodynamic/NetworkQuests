# Getting Started with NetworkQuests

Welcome to NetworkQuests! This guide will help you get up and running with the comprehensive C++20 network protocol educational library.

## Table of Contents

1. [Quick Start](#quick-start)
2. [Installation](#installation)
3. [First Steps](#first-steps)
4. [Building Your First Application](#building-your-first-application)
5. [Understanding the Architecture](#understanding-the-architecture)
6. [Example Walkthrough](#example-walkthrough)
7. [Next Steps](#next-steps)

## Quick Start

### Prerequisites

Before you begin, ensure you have:

- **C++20 compatible compiler**: GCC 10+, Clang 11+, or MSVC 2019+
- **CMake**: Version 3.24.0 or later
- **Git**: For cloning the repository

### 1-Minute Setup

```bash
# Clone the repository
git clone https://github.com/your-username/NetworkQuests.git
cd NetworkQuests

# Build and install
chmod +x scripts/install.sh
./scripts/install.sh

# Test with a simple example
cd build/examples
./tcp_echo_server
```

That's it! You now have NetworkQuests installed and can run the examples.

## Installation

### Using the Installation Script (Recommended)

The easiest way to install NetworkQuests is using the provided installation script:

```bash
# Basic installation
./scripts/install.sh

# Custom installation
./scripts/install.sh --prefix ~/networkquests --build-type Debug --no-examples

# See all options
./scripts/install.sh --help
```

### Manual Installation

If you prefer manual control over the build process:

```bash
# Create build directory
mkdir build && cd build

# Configure the project
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build the project
cmake --build . --parallel $(nproc)

# Run tests (optional)
ctest --parallel $(nproc)

# Install
sudo cmake --install .
```

### Build Options

NetworkQuests provides several build options:

| Option | Default | Description |
|--------|---------|-------------|
| `ENABLE_EXAMPLES` | ON | Build example applications |
| `ENABLE_TESTING` | ON | Build test suite |
| `ENABLE_BOOST` | ON | Enable Boost.Asio support |
| `ENABLE_OPENSSL` | ON | Enable OpenSSL support |
| `ENABLE_WARNINGS` | ON | Enable compiler warnings |
| `ENABLE_SANITIZERS` | OFF | Enable sanitizers (Debug builds) |
| `BUILD_SHARED_LIBS` | OFF | Build shared libraries |

Example with custom options:

```bash
cmake .. \
    -DCMAKE_BUILD_TYPE=Debug \
    -DENABLE_EXAMPLES=ON \
    -DENABLE_TESTING=OFF \
    -DENABLE_BOOST=OFF
```

## First Steps

### Verify Installation

After installation, verify everything works:

```bash
# Check if NetworkQuests is found by CMake
echo 'find_package(NetworkQuests REQUIRED)
message(STATUS "Found NetworkQuests ${NetworkQuests_VERSION}")' > test_find.cmake

cmake -P test_find.cmake
```

### Explore Examples

NetworkQuests comes with comprehensive examples:

```bash
# Navigate to examples directory
cd /usr/local/bin/examples  # or your custom install prefix

# List available examples
ls -la

# Try the TCP echo example
./tcp_echo_server &
./tcp_echo_client
```

### Available Protocols

NetworkQuests implements 8 network protocols:

- **TCP** - Reliable, connection-oriented transport
- **UDP** - Fast, connectionless transport
- **HTTP** - Web protocol with full server/client
- **DNS** - Domain name resolution
- **FTP** - File transfer with active/passive modes
- **WebSocket** - Real-time bidirectional communication
- **SMTP** - Email sending/receiving
- **SNMP** - Network management

## Building Your First Application

### Simple TCP Client

Create a file called `my_tcp_client.cpp`:

```cpp
#include "networkquests/tcp.hpp"
#include <iostream>
#include <string>

using namespace networkquests;

int main() {
    try {
        // Create and connect TCP client
        tcp::TcpClient client;
        auto result = client.connect(SocketAddress::from_string("127.0.0.1", 8080));
        
        if (!result) {
            std::cerr << "Connection failed: " << result.error() << std::endl;
            return 1;
        }
        
        std::cout << "Connected to server!" << std::endl;
        
        // Send a message
        std::string message = "Hello, NetworkQuests!";
        auto send_result = client.send(message);
        
        if (send_result) {
            std::cout << "Message sent: " << message << std::endl;
        }
        
        // Receive response
        auto response = client.receive();
        if (response) {
            std::cout << "Server response: " << response.value() << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
```

### CMakeLists.txt for Your Project

Create a `CMakeLists.txt` file:

```cmake
cmake_minimum_required(VERSION 3.24)
project(MyNetworkApp)

# Set C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find NetworkQuests
find_package(NetworkQuests REQUIRED)

# Create executable
add_executable(my_tcp_client my_tcp_client.cpp)

# Link NetworkQuests
target_link_libraries(my_tcp_client PRIVATE networkquests)

# Or link specific protocols only
# networkquests_link_protocols(my_tcp_client PROTOCOLS TCP)
```

### Build and Run

```bash
mkdir build && cd build
cmake ..
make
./my_tcp_client
```

## Understanding the Architecture

### Core Components

NetworkQuests is built with a modular architecture:

```
NetworkQuests/
├── Core Utilities          # Common functionality
│   ├── Result<T>           # Error handling
│   ├── Logger              # Logging system
│   └── SocketAddress       # Address abstraction
├── Protocol Libraries      # Individual protocols
│   ├── TCP                 # Reliable transport
│   ├── UDP                 # Unreliable transport
│   ├── HTTP                # Application protocol
│   └── ...                 # Other protocols
└── Examples               # Educational examples
```

### Error Handling

NetworkQuests uses a `Result<T>` type for error handling:

```cpp
#include "networkquests/common.hpp"

// Function returns Result<std::string>
auto result = some_function();

if (result) {
    // Success - get the value
    std::string value = result.value();
    std::cout << "Success: " << value << std::endl;
} else {
    // Error - get the error message
    std::cout << "Error: " << result.error() << std::endl;
}
```

### Logging

Built-in logging system:

```cpp
#include "networkquests/logger.hpp"

// Set log level
Logger::set_level(LogLevel::DEBUG);

// Log messages
LOG_INFO("MyApp", "Application started");
LOG_ERROR("Network", "Connection failed: " + error_msg);
LOG_DEBUG("Protocol", "Received packet: " + packet_data);
```

## Example Walkthrough

Let's walk through a complete HTTP client/server example:

### HTTP Server

```cpp
#include "networkquests/http.hpp"
#include <iostream>

using namespace networkquests;

int main() {
    http::HttpServer server(8080);
    
    // Add a simple route
    server.add_route(http::HttpMethod::GET, "/hello", 
        [](const http::HttpRequest& req) {
            http::HttpResponse response;
            response.set_status(http::HttpStatus::OK);
            response.set_body("Hello, World!");
            response.set_header("Content-Type", "text/plain");
            return response;
        });
    
    // Add a JSON API route
    server.add_route(http::HttpMethod::POST, "/api/users",
        [](const http::HttpRequest& req) {
            // Parse JSON, process user data
            http::HttpResponse response;
            response.set_status(http::HttpStatus::CREATED);
            response.set_body("{\"status\":\"user created\"}");
            response.set_header("Content-Type", "application/json");
            return response;
        });
    
    std::cout << "Starting server on http://localhost:8080" << std::endl;
    server.start();
    
    return 0;
}
```

### HTTP Client

```cpp
#include "networkquests/http.hpp"
#include <iostream>

using namespace networkquests;

int main() {
    http::HttpClient client;
    
    // Simple GET request
    auto response = client.get("http://localhost:8080/hello");
    
    if (response) {
        std::cout << "Status: " << static_cast<int>(response.value().status()) << std::endl;
        std::cout << "Body: " << response.value().body() << std::endl;
    } else {
        std::cerr << "Request failed: " << response.error() << std::endl;
    }
    
    // POST request with JSON
    http::HttpRequest request;
    request.set_method(http::HttpMethod::POST);
    request.set_url("http://localhost:8080/api/users");
    request.set_header("Content-Type", "application/json");
    request.set_body("{\"name\":\"John\",\"email\":\"john@example.com\"}");
    
    auto post_response = client.send(request);
    
    if (post_response) {
        std::cout << "POST response: " << post_response.value().body() << std::endl;
    }
    
    return 0;
}
```

## Next Steps

### Explore More Protocols

- **WebSocket**: Try the real-time chat example
- **DNS**: Build a custom DNS resolver
- **FTP**: Create a file synchronization tool
- **SMTP**: Send emails programmatically
- **SNMP**: Monitor network devices

### Advanced Features

- **Middleware**: Add custom HTTP middleware
- **SSL/TLS**: Enable secure communications
- **Asynchronous I/O**: Use Boost.Asio integration
- **Custom Protocols**: Extend the framework

### Learning Resources

- Read the protocol documentation in `docs/`
- Study the example applications
- Explore the test suite for usage patterns
- Check out the legacy FlatBuffers examples

### Contributing

NetworkQuests is an educational project. Contributions are welcome!

- Report bugs and suggest features
- Add new protocol implementations
- Improve documentation
- Share your projects built with NetworkQuests

### Getting Help

- Read the comprehensive documentation
- Study the examples and tests
- Check the FAQ in the repository
- Open an issue for specific questions

---

**Welcome to the world of network programming with NetworkQuests!** 🌐

Start with the examples, experiment with different protocols, and build amazing networked applications.