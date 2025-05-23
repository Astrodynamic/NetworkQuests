# NetworkQuests

**A Comprehensive C++20 Network Protocol Educational Library**

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![CMake](https://img.shields.io/badge/CMake-3.24%2B-green.svg)](https://cmake.org/)
[![License](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Cross-Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux%20%7C%20macOS-lightgrey.svg)](#)

NetworkQuests is a modern, educational C++20 library implementing comprehensive network protocol stack with production-ready code and extensive documentation. Perfect for learning network programming concepts while building real-world applications.

## 🚀 Quick Start

```bash
# Clone the repository
git clone https://github.com/your-username/NetworkQuests.git
cd NetworkQuests

# Clean up any old modules (first time setup)
chmod +x scripts/cleanup.sh
./scripts/cleanup.sh

# Install with one command
chmod +x scripts/install.sh
./scripts/install.sh

# Try your first example
cd build/examples
./tcp_echo_server
```

## ✨ Features

### 🌐 Implemented Protocols

| Protocol | Description | Status | RFC/Standard |
|----------|-------------|--------|--------------|
| **TCP** | Transmission Control Protocol | ✅ Complete | RFC 793 |
| **UDP** | User Datagram Protocol | ✅ Complete | RFC 768 |
| **HTTP** | HyperText Transfer Protocol | ✅ Complete | RFC 2616/7230+ |
| **DNS** | Domain Name System | ✅ Complete | RFC 1035 |
| **FTP** | File Transfer Protocol | ✅ Complete | RFC 959 |
| **WebSocket** | Full-duplex Communication | ✅ Complete | RFC 6455 |
| **SMTP** | Simple Mail Transfer Protocol | ✅ Complete | RFC 5321 |
| **SNMP** | Simple Network Management Protocol | ✅ Complete | RFC 1157 |

### 🛠️ Core Features

- **Modern C++20**: Concepts, ranges, coroutines, and latest standard features
- **Cross-Platform**: Windows, Linux, and macOS support
- **Thread-Safe**: Production-ready multi-threaded implementations
- **Educational**: Comprehensive documentation and examples for learning
- **Production-Ready**: Real-world applicable code with proper error handling
- **Modular Design**: Use only the protocols you need
- **Zero-Dependencies Core**: Minimal external dependencies
- **Extensive Examples**: 16+ example applications covering all protocols

## 📊 Project Statistics

- **8 Network Protocols** implemented
- **35,000+ Lines of Code** (C++20)
- **16 Example Applications** with full documentation
- **8 Protocol Guides** with theory and implementation details
- **Comprehensive Test Suite** with unit and integration tests
- **Cross-Platform Support** (Windows/Linux/macOS)

## 🏗️ Project Structure

NetworkQuests follows a clean, modular architecture designed for both education and production use:

```
NetworkQuests/
├── 📁 include/networkquests/    # Public API headers
├── 📁 src/                      # Implementation files  
├── 📁 examples/                 # Example applications
│   └── 📁 legacy/               # Original FlatBuffers project
├── 📁 docs/                     # Comprehensive documentation
├──  tests/                    # Test suites
├── 📁 cmake/                    # CMake configuration
└── 📁 scripts/                  # Build and utility scripts
```

For detailed information about the project organization, see [📄 Project Structure Guide](docs/PROJECT_STRUCTURE.md).

## 📚 Quick Examples

### TCP Echo Server

```cpp
#include "networkquests/tcp.hpp"
#include <iostream>

using namespace networkquests;

int main() {
    tcp::TcpServer server(8080);
    
    server.set_connection_handler([](tcp::TcpConnection& conn) {
        while (auto data = conn.receive()) {
            std::cout << "Received: " << data.value() << std::endl;
            conn.send("Echo: " + data.value());
        }
    });
    
    std::cout << "Starting TCP server on port 8080..." << std::endl;
    server.start();
    
    return 0;
}
```

### HTTP Web Server

```cpp
#include "networkquests/http.hpp"
#include <iostream>

using namespace networkquests;

int main() {
    http::HttpServer server(8080);
    
    // JSON API endpoint
    server.add_route(http::HttpMethod::GET, "/api/users", 
        [](const http::HttpRequest& req) {
            http::HttpResponse response;
            response.set_status(http::HttpStatus::OK);
            response.set_body("[{\"id\":1,\"name\":\"John\"}]");
            response.set_header("Content-Type", "application/json");
            return response;
        });
    
    // Static file serving
    server.set_static_directory("./public");
    
    std::cout << "HTTP server running on http://localhost:8080" << std::endl;
    server.start();
    
    return 0;
}
```

### WebSocket Chat Server

```cpp
#include "networkquests/websocket.hpp"
#include <iostream>
#include <set>

using namespace networkquests;

int main() {
    websocket::WebSocketServer server(8080);
    std::set<websocket::WebSocketConnection*> clients;
    
    server.set_connection_handler([&](websocket::WebSocketConnection& conn) {
        clients.insert(&conn);
        
        conn.set_message_handler([&](const std::string& message) {
            // Broadcast to all clients
            for (auto* client : clients) {
                if (client != &conn) {
                    client->send(message);
                }
            }
        });
        
        conn.set_close_handler([&]() {
            clients.erase(&conn);
        });
    });
    
    std::cout << "WebSocket chat server on ws://localhost:8080" << std::endl;
    server.start();
    
    return 0;
}
```

## 🛠️ Installation

### Prerequisites

- **C++20 compatible compiler**: GCC 10+, Clang 11+, or MSVC 2019+
- **CMake**: Version 3.24.0 or later
- **Optional**: Boost (for enhanced features), OpenSSL (for secure protocols)

### Using Installation Script (Recommended)

```bash
# Clone the repository
git clone https://github.com/your-username/NetworkQuests.git
cd NetworkQuests

# Clean up old modules (first time)
chmod +x scripts/cleanup.sh
./scripts/cleanup.sh

# Run installation script
chmod +x scripts/install.sh
./scripts/install.sh

# Custom installation
./scripts/install.sh --prefix ~/networkquests --build-type Debug
```

### Manual Installation

```bash
# Create build directory
mkdir build && cd build

# Configure with CMake
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build the project
cmake --build . --parallel $(nproc)

# Run tests
ctest --parallel $(nproc)

# Install
sudo cmake --install .
```

### Using in Your Project

```cmake
# CMakeLists.txt
find_package(NetworkQuests REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE networkquests)

# Or link specific protocols only
# networkquests_link_protocols(my_app PROTOCOLS TCP HTTP)
```

## 📖 Documentation

### Getting Started
- [📖 Getting Started Guide](docs/GETTING_STARTED.md) - Complete setup and first steps
- [🏗️ Architecture Overview](docs/ARCHITECTURE.md) - System design and implementation details
- [📁 Project Structure](docs/PROJECT_STRUCTURE.md) - Detailed project organization

### Protocol Documentation
- [🌐 TCP Protocol Guide](docs/tcp.md) - Reliable transport layer
- [📡 UDP Protocol Guide](docs/udp.md) - Fast, connectionless communication
- [🌍 HTTP Protocol Guide](docs/http.md) - Web servers and clients
- [🔍 DNS Protocol Guide](docs/dns.md) - Domain name resolution
- [📁 FTP Protocol Guide](docs/ftp.md) - File transfer with active/passive modes
- [💬 WebSocket Protocol Guide](docs/websocket.md) - Real-time communication
- [📧 SMTP Protocol Guide](docs/smtp.md) - Email sending and receiving
- [📈 SNMP Protocol Guide](docs/snmp.md) - Network management

## 🔧 Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `ENABLE_EXAMPLES` | ON | Build example applications |
| `ENABLE_TESTING` | ON | Build test suite |
| `ENABLE_BOOST` | ON | Enable Boost.Asio support |
| `ENABLE_OPENSSL` | ON | Enable OpenSSL support |
| `ENABLE_WARNINGS` | ON | Enable compiler warnings |
| `ENABLE_SANITIZERS` | OFF | Enable sanitizers (Debug builds) |
| `BUILD_SHARED_LIBS` | OFF | Build shared libraries |

Example:
```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug -DENABLE_BOOST=OFF -DENABLE_EXAMPLES=ON
```

## 🧪 Examples and Tests

### Running Examples

```bash
# TCP Examples
./build/examples/tcp_echo_server &
./build/examples/tcp_echo_client

# HTTP Examples
./build/examples/http_web_server &
curl http://localhost:8080/api/users

# WebSocket Examples
./build/examples/websocket_chat_server &
./build/examples/websocket_chat_client

# Email Examples
./build/examples/smtp_mail_client

# Network Monitoring
./build/examples/snmp_monitor
```

### Running Tests

```bash
# Run all tests
cd build && ctest --parallel $(nproc)

# Run specific protocol tests
ctest -R tcp
ctest -R http
ctest -R websocket
```

## 🏛️ Architecture Highlights

### Modern C++20 Features
- **Concepts**: Type-safe template constraints
- **Ranges**: Elegant data processing
- **Coroutines**: Asynchronous operations (where supported)
- **Strong typing**: Prevent common network programming errors

### Error Handling
```cpp
// No exceptions - use Result<T> type
auto result = tcp_client.connect("example.com", 80);
if (result) {
    // Success
    auto response = tcp_client.send("GET / HTTP/1.1\r\n\r\n");
} else {
    // Handle error
    std::cerr << "Connection failed: " << result.error() << std::endl;
}
```

### Thread Safety
- Thread-safe logging system
- Multi-threaded servers with connection pooling
- Lock-free data structures where possible
- RAII resource management

### Cross-Platform Support
- Unified socket abstraction
- Platform-specific optimizations
- Consistent API across all platforms

## 🏆 Educational Value

NetworkQuests is designed as an educational resource:

### For Students
- **Clear implementations** of network protocols
- **Comprehensive documentation** with theory and practice
- **Progressive examples** from basic to advanced
- **Real-world applicable** code patterns

### For Educators
- **Modular structure** for teaching specific protocols
- **Well-documented code** suitable for classroom analysis
- **Practical exercises** with working examples
- **Industry-standard practices** demonstrated throughout

### For Professionals
- **Production-ready implementations** for real projects
- **Performance-optimized** code with benchmarks
- **Extensible architecture** for custom protocols
- **Comprehensive test coverage** ensuring reliability

## 🌟 Legacy Support

The project includes the original FlatBuffers-based implementation in `examples/legacy/`:

- **Original L4NetworkQuests**: TCP/UDP with FlatBuffers serialization
- **Preserved Dependencies**: Original CMake configuration and dependencies
- **Educational Reference**: Shows evolution from prototype to production library

```bash
# Build legacy examples
cd examples/legacy
mkdir build && cd build
cmake ..
make
```

## 🧹 Project Maintenance

### Cleanup Old Modules

If you're upgrading from a previous version or want to clean build artifacts:

```bash
# Run cleanup script to remove old modules and build artifacts
chmod +x scripts/cleanup.sh
./scripts/cleanup.sh
```

This will remove:
- Old legacy directories from root (`flatbuffers/`, `tcp/`, `udp/`)
- Build artifacts and generated files
- IDE configuration files
- Temporary and cache files

## 🤝 Contributing

NetworkQuests welcomes contributions! Whether you're fixing bugs, adding features, improving documentation, or sharing educational content:

1. **Fork the repository**
2. **Create a feature branch**: `git checkout -b feature/amazing-feature`
3. **Make your changes** with tests and documentation
4. **Commit your changes**: `git commit -m 'Add amazing feature'`
5. **Push to the branch**: `git push origin feature/amazing-feature`
6. **Open a Pull Request**

### Areas for Contribution
- Additional protocol implementations (MQTT, CoAP, etc.)
- Performance optimizations
- Platform-specific enhancements
- Educational content and examples
- Documentation improvements

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 🙏 Acknowledgments

- **C++ Standards Committee** for the amazing C++20 features
- **RFC Authors** for the detailed protocol specifications
- **Open Source Community** for inspiration and best practices
- **Educational Institutions** that inspire better learning resources

## 📞 Support

- 📖 **Documentation**: Check the comprehensive docs in the `docs/` directory
- 💬 **Issues**: Open an issue for bugs or feature requests
- 📧 **Discussions**: Use GitHub Discussions for questions and ideas
- 🌟 **Star the repository** if you find it useful!

---

**NetworkQuests: Where Network Programming Education Meets Production Reality** 🌐

*Start your journey into network programming with modern C++20 and build amazing networked applications!*