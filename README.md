# NetworkQuests

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.wikipedia.org/wiki/C%2B%2B20)
[![CMake](https://img.shields.io/badge/CMake-3.24%2B-brightgreen.svg)](https://cmake.org/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

A comprehensive, educational C++ project exploring network protocols and interactions across all layers of the OSI and TCP/IP models. This project serves as both a practical implementation and a learning resource for understanding fundamental network programming concepts using modern C++20.

## Table of Contents

- [Introduction](#introduction)
- [Features](#features)
- [Project Structure](#project-structure)
- [Prerequisites](#prerequisites)
- [Quick Start](#quick-start)
- [Protocol Implementations](#protocol-implementations)
- [Documentation](#documentation)
- [Testing](#testing)
- [Contributing](#contributing)
- [License](#license)

## Introduction

NetworkQuests is designed to be an educational and research-oriented project that explores network protocols through practical C++ implementations. Each protocol implementation includes:

- **Client and Server applications** demonstrating real-world usage
- **Comprehensive documentation** explaining protocol mechanics and implementation details
- **Unit and integration tests** ensuring correctness and reliability
- **Modern C++20 features** showcasing best practices and contemporary standards

## Features

- 🌐 **Multi-Protocol Support**: TCP, UDP, HTTP, DNS with complete client/server implementations
- 🔧 **Modern C++20**: Leveraging concepts, ranges, coroutines, and other modern features
- 📚 **Educational Focus**: Detailed documentation and guides for each protocol
- 🧪 **Comprehensive Testing**: Unit and integration tests for all components
- 🏗️ **Modular Architecture**: Clean separation between protocols and shared utilities
- 🌍 **HTTP/1.1 Support**: Full RESTful API server with routing, middleware, and static files
- 🔍 **DNS Resolution**: Complete RFC 1035 implementation with caching and zone management
- 🔒 **Security Support**: Optional SSL/TLS support via OpenSSL integration
- ⚡ **Asynchronous I/O**: Boost.Asio support for high-performance applications
- 🖥️ **Cross-Platform**: Support for Linux, Windows, and macOS

## Project Structure

```
NetworkQuests/
├── cmake/                   # CMake modules and scripts
├── docs/                    # Protocol documentation and guides
│   ├── tcp.md              # TCP protocol guide
│   ├── udp.md              # UDP protocol guide
│   ├── http.md             # HTTP protocol guide
│   ├── dns.md              # DNS protocol guide
│   └── ...                 # Other protocol guides
├── include/                 # Header files for shared utilities
│   └── networkquests/      # Main library headers
├── src/                     # Source code, organized by protocol
│   ├── http/               # HTTP client and server
│   ├── tcp/                # TCP client and server
│   ├── udp/                # UDP client and server
│   ├── dns/                # DNS client and server
│   ├── ftp/                # FTP client and server
│   ├── websocket/          # WebSocket client and server
│   └── utils/              # Shared utilities and common code
├── examples/                # Example applications and demos
│   ├── tcp_echo_server.cpp
│   ├── tcp_echo_client.cpp
│   ├── udp_echo_server.cpp
│   ├── udp_echo_client.cpp
│   ├── http_server.cpp
│   ├── http_client.cpp
│   ├── dns_server.cpp
│   ├── dns_client.cpp
│   └── ...
└── tests/                   # Unit tests and test utilities
```

## Prerequisites

### Required
- **C++20 compatible compiler**: GCC 10+, Clang 11+, or MSVC 2019+
- **CMake**: Version 3.24.1 or later
- **Operating System**: Linux, Windows, or macOS

### Optional Dependencies
- **Boost**: For advanced asynchronous networking features
- **OpenSSL**: For secure communication protocols
- **Catch2 or Google Test**: For testing (automatically fetched if not found)

## Quick Start

### 1. Clone the Repository

```bash
git clone https://github.com/your-username/NetworkQuests.git
cd NetworkQuests
```

### 2. Build the Project

```bash
# Create build directory
mkdir build && cd build

# Configure the project
cmake ..

# Build all targets
cmake --build .

# Run tests (optional)
ctest
```

### 3. Run Your First Example

#### TCP Echo Example
```bash
# Start TCP server in one terminal
./examples/tcp_echo_server

# Connect with TCP client in another terminal
./examples/tcp_echo_client
```

#### HTTP Server Example
```bash
# Start HTTP server in one terminal
./examples/http_server

# Test with curl or visit http://localhost:8080 in your browser
curl -X GET http://localhost:8080/
curl -X POST http://localhost:8080/api/users -H "Content-Type: application/json" -d '{"name":"John","email":"john@example.com"}'

# Or use the interactive HTTP client
./examples/http_client
```

#### DNS Server/Client Example
```bash
# Start DNS server in one terminal (uses port 5353 for non-privileged access)
./examples/dns_server

# Test with dig or use the interactive DNS client
dig @127.0.0.1 -p 5353 example.local
./examples/dns_client
```

## Protocol Implementations

### Currently Implemented
- ✅ **TCP** - Transmission Control Protocol with connection management
- ✅ **UDP** - User Datagram Protocol for connectionless communication
- ✅ **HTTP** - HyperText Transfer Protocol with RESTful API support and routing
- ✅ **DNS** - Domain Name System with full RFC 1035 implementation
- 🚧 **FTP** - File Transfer Protocol (in progress)

### Planned Implementations
- 📋 **WebSocket** - Real-time bidirectional communication
- 📋 **SMTP** - Simple Mail Transfer Protocol
- 📋 **SSH** - Secure Shell Protocol
- 📋 **SNMP** - Simple Network Management Protocol

## Documentation

Each protocol implementation includes comprehensive documentation:

- **Protocol Overview**: Purpose, use cases, and technical specifications
- **OSI/TCP-IP Layer Information**: Which layer the protocol operates on
- **Implementation Details**: How the C++ code implements the protocol
- **Usage Examples**: Practical examples with code snippets
- **References**: Links to RFCs and additional resources

All documentation is available in the [`docs/`](docs/) directory.

## Testing

The project includes extensive testing:

```bash
# Run all tests
ctest

# Run specific test categories
ctest -L unit        # Unit tests only
ctest -L integration # Integration tests only

# Run tests with detailed output
ctest --verbose
```

### Test Categories
- **Unit Tests**: Test individual classes and functions
- **Integration Tests**: Test complete client-server interactions
- **Performance Tests**: Measure throughput and latency characteristics

## Build Options

The project provides several build-time options:

```bash
# Basic configuration
cmake -DCMAKE_BUILD_TYPE=Release ..

# Enable/disable optional features
cmake -DENABLE_BOOST=ON -DENABLE_OPENSSL=ON ..

# Disable testing and examples for production builds
cmake -DENABLE_TESTING=OFF -DENABLE_EXAMPLES=OFF ..
```

Available options:
- `ENABLE_BOOST`: Enable Boost.Asio for advanced networking (default: ON)
- `ENABLE_OPENSSL`: Enable OpenSSL for secure communications (default: ON)
- `ENABLE_TESTING`: Enable building tests (default: ON)
- `ENABLE_EXAMPLES`: Enable building examples (default: ON)

## Contributing

We welcome contributions! Please see our [Contributing Guidelines](CONTRIBUTING.md) for details on:

- Code style and formatting requirements
- How to add new protocol implementations
- Testing requirements
- Documentation standards

## License

This project is licensed under the [MIT License](LICENSE) - see the LICENSE file for details.

## Acknowledgments

- Inspired by the original [L4NetworkQuests](https://github.com/Astrodynamic/L4NetworkQuests) project
- Built with modern C++20 standards and best practices
- Educational content informed by RFC specifications and networking literature

---

**Happy Network Programming! 🌐**