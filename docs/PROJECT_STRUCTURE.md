# NetworkQuests Project Structure

This document describes the complete project structure and organization of the NetworkQuests library.

## 📁 Root Directory Structure

```
NetworkQuests/                           # Root project directory
├── 📄 CMakeLists.txt                    # Main CMake configuration
├── 📄 README.md                         # Project overview and quick start
├── 📄 PROGRESS.md                       # Development progress and completion status
├── 📄 LICENSE                           # MIT License
├── 📄 .gitignore                        # Git ignore patterns
├── 📄 CMakePresets.json                 # CMake preset configurations
├── 📄 Makefile                          # Convenience build commands
│
├── 📁 include/                          # PUBLIC API HEADERS
├── 📁 src/                              # IMPLEMENTATION SOURCE CODE
├── 📁 examples/                         # EXAMPLE APPLICATIONS
├── 📁 docs/                             # PROJECT DOCUMENTATION
├── 📁 tests/                            # TEST SUITES
├── 📁 cmake/                            # CMAKE MODULES AND CONFIGURATION
└── 📁 scripts/                          # BUILD AND UTILITY SCRIPTS
```

## 📁 include/ - Public API Headers

**Purpose**: Contains all public headers that users will include when using NetworkQuests.

```
include/networkquests/
├── 📄 common.hpp                        # Core utilities (Result<T>, SocketAddress, etc.)
├── 📄 logger.hpp                        # Thread-safe logging system
├── 📄 socket.hpp                        # Cross-platform socket abstraction
├── 📄 tcp.hpp                           # TCP protocol client/server
├── 📄 udp.hpp                           # UDP protocol client/server
├── 📄 http.hpp                          # HTTP protocol client/server
├── 📄 dns.hpp                           # DNS protocol resolver/server
├── 📄 ftp.hpp                           # FTP protocol client/server
├── 📄 websocket.hpp                     # WebSocket protocol client/server
├── 📄 smtp.hpp                          # SMTP protocol client/server
└── 📄 snmp.hpp                          # SNMP protocol manager/agent
```

**Design Principles**:
- **Single Include**: Each protocol can be included independently
- **Namespace Organization**: All under `networkquests` namespace
- **Forward Declarations**: Minimal dependencies between headers
- **Documentation**: Comprehensive inline documentation

## 📁 src/ - Implementation Source Code

**Purpose**: Contains all implementation files organized by protocol and functionality.

```
src/
├── 📄 CMakeLists.txt                    # Main source build configuration
├── 📁 utils/                            # Core utilities implementation
│   ├── 📄 CMakeLists.txt                # Utilities build configuration
│   ├── 📄 common.cpp                    # Result<T>, error handling, SocketAddress
│   ├── 📄 logger.cpp                    # Thread-safe logging implementation
│   └── 📄 socket.cpp                    # Cross-platform socket abstraction
├── 📁 tcp/                              # TCP Protocol Implementation
│   ├── 📄 CMakeLists.txt                # TCP build configuration
│   ├── 📄 tcp_client.cpp                # TCP client implementation
│   ├── 📄 tcp_server.cpp                # TCP server implementation
│   └── 📄 tcp_utils.cpp                 # TCP utility functions
├── 📁 udp/                              # UDP Protocol Implementation
│   ├── 📄 CMakeLists.txt                # UDP build configuration
│   ├── 📄 udp_client.cpp                # UDP client implementation
│   ├── 📄 udp_server.cpp                # UDP server implementation
│   └── 📄 udp_utils.cpp                 # UDP utility functions
├── 📁 http/                             # HTTP Protocol Implementation
│   ├── 📄 CMakeLists.txt                # HTTP build configuration
│   ├── 📄 http_client.cpp               # HTTP client implementation
│   ├── 📄 http_server.cpp               # HTTP server with routing
│   ├── 📄 http_message.cpp              # HTTP request/response classes
│   ├── 📄 http_parser.cpp               # HTTP message parsing
│   └── 📄 http_utils.cpp                # HTTP utility functions
├── 📁 dns/                              # DNS Protocol Implementation
│   ├── 📄 CMakeLists.txt                # DNS build configuration
│   ├── 📄 dns_client.cpp                # DNS resolver implementation
│   ├── 📄 dns_server.cpp                # DNS authoritative server
│   ├── 📄 dns_message.cpp               # DNS message format handling
│   ├── 📄 dns_cache.cpp                 # DNS caching implementation
│   └── 📄 dns_utils.cpp                 # DNS utility functions
├── 📁 ftp/                              # FTP Protocol Implementation
│   ├── 📄 CMakeLists.txt                # FTP build configuration
│   ├── 📄 ftp_client.cpp                # FTP client implementation
│   ├── 📄 ftp_server.cpp                # FTP server implementation
│   ├── 📄 ftp_session.cpp               # FTP session management
│   └── 📄 ftp_utils.cpp                 # FTP utility functions
├── 📁 websocket/                        # WebSocket Protocol Implementation
│   ├── 📄 CMakeLists.txt                # WebSocket build configuration
│   ├── 📄 websocket_client.cpp          # WebSocket client implementation
│   ├── 📄 websocket_server.cpp          # WebSocket server implementation
│   ├── 📄 websocket_frame.cpp           # WebSocket frame handling
│   └── 📄 websocket_utils.cpp           # WebSocket utility functions
├── 📁 smtp/                             # SMTP Protocol Implementation
│   ├── 📄 CMakeLists.txt                # SMTP build configuration
│   ├── 📄 smtp_client.cpp               # SMTP client implementation
│   ├── 📄 smtp_server.cpp               # SMTP server implementation
│   ├── 📄 smtp_message.cpp              # Email message composition
│   └── 📄 smtp_utils.cpp                # SMTP utility functions
└── 📁 snmp/                             # SNMP Protocol Implementation
    ├── 📄 CMakeLists.txt                # SNMP build configuration
    ├── 📄 snmp_manager.cpp              # SNMP manager implementation
    ├── 📄 snmp_agent.cpp                # SNMP agent implementation
    ├── 📄 snmp_message.cpp              # SNMP message handling
    ├── 📄 snmp_core.cpp                 # SNMP core classes (OID, MIB)
    └── 📄 snmp_utils.cpp                # SNMP utility functions (ASN.1 BER)
```

**Organization Principles**:
- **Protocol Isolation**: Each protocol is self-contained
- **Modular Building**: Each directory can be built independently
- **Shared Utilities**: Common code in utils/ directory
- **Clean Dependencies**: Protocols depend only on utils/

## 📁 examples/ - Example Applications

**Purpose**: Demonstrates practical usage of all protocols with educational examples.

```
examples/
├── 📄 CMakeLists.txt                    # Examples build configuration
├── 📁 legacy/                           # Original FlatBuffers L4NetworkQuests
│   ├── 📄 README.md                     # Legacy project documentation
│   ├── 📄 CMakeLists.txt                # Legacy build system
│   ├── 📁 flatbuffers/                  # FlatBuffers schemas and generation
│   ├── 📁 tcp/                          # Original TCP client/server
│   └── 📁 udp/                          # Original UDP client/server
├── 📄 tcp_echo_server.cpp               # Simple TCP echo server
├── 📄 tcp_echo_client.cpp               # TCP echo client
├── 📄 udp_echo_server.cpp               # UDP echo server
├── 📄 udp_echo_client.cpp               # UDP echo client
├── 📄 http_web_server.cpp               # Full-featured HTTP web server
├── 📄 http_rest_client.cpp              # HTTP REST API client
├── 📄 dns_resolver.cpp                  # DNS resolver example
├── 📄 dns_server.cpp                    # DNS authoritative server
├── 📄 ftp_file_server.cpp               # FTP file server
├── 📄 ftp_client.cpp                    # FTP client application
├── 📄 websocket_chat_server.cpp         # WebSocket chat server
├── 📄 websocket_chat_client.cpp         # WebSocket chat client
├── 📄 smtp_mail_server.cpp              # SMTP mail server
├── 📄 smtp_mail_client.cpp              # SMTP mail client
├── 📄 snmp_agent.cpp                    # SNMP agent application
└── 📄 snmp_client.cpp                   # SNMP manager client
```

**Educational Structure**:
- **Progressive Complexity**: From simple echo to complex applications
- **Real-World Scenarios**: Practical applications (web server, chat, email)
- **Interactive Examples**: User-friendly interfaces for learning
- **Legacy Preservation**: Original project maintained for reference

## 📁 docs/ - Project Documentation

**Purpose**: Comprehensive documentation covering all aspects of the project.

```
docs/
├── 📄 GETTING_STARTED.md                # Quick start and installation guide
├── 📄 ARCHITECTURE.md                   # System architecture and design
├── 📄 PROJECT_STRUCTURE.md              # This file - project organization
├── 📄 tcp.md                            # TCP protocol implementation guide
├── 📄 udp.md                            # UDP protocol implementation guide
├── 📄 http.md                           # HTTP protocol implementation guide
├── 📄 dns.md                            # DNS protocol implementation guide
├── 📄 ftp.md                            # FTP protocol implementation guide
├── 📄 websocket.md                      # WebSocket protocol implementation guide
├── 📄 smtp.md                           # SMTP protocol implementation guide
└── 📄 snmp.md                           # SNMP protocol implementation guide
```

**Documentation Standards**:
- **Theory and Practice**: Each guide covers both protocol theory and implementation
- **Code Examples**: Extensive code samples and usage patterns
- **Educational Content**: Suitable for both learning and reference
- **Complete Coverage**: All features and capabilities documented

## 📁 tests/ - Test Suites

**Purpose**: Comprehensive testing for all components and protocols.

```
tests/
├── 📄 CMakeLists.txt                    # Test build configuration
├── 📁 unit/                             # Unit tests for individual components
│   ├── 📄 test_common.cpp               # Core utilities tests
│   ├── 📄 test_tcp.cpp                  # TCP protocol tests
│   ├── 📄 test_udp.cpp                  # UDP protocol tests
│   ├── 📄 test_http.cpp                 # HTTP protocol tests
│   ├── 📄 test_dns.cpp                  # DNS protocol tests
│   ├── 📄 test_ftp.cpp                  # FTP protocol tests
│   ├── 📄 test_websocket.cpp            # WebSocket protocol tests
│   ├── 📄 test_smtp.cpp                 # SMTP protocol tests
│   └── 📄 test_snmp.cpp                 # SNMP protocol tests
└── 📁 integration/                      # Integration and end-to-end tests
    ├── 📄 test_client_server.cpp        # Client-server interaction tests
    └── 📄 test_protocol_interop.cpp     # Protocol interoperability tests
```

**Testing Strategy**:
- **Unit Coverage**: Individual class and function testing
- **Integration Testing**: End-to-end protocol testing
- **Cross-Platform**: Tests run on all supported platforms
- **Automated**: Integrated with CI/CD systems

## 📁 cmake/ - CMake Modules and Configuration

**Purpose**: CMake configuration files for package management and build system.

```
cmake/
├── 📄 NetworkQuestsConfig.cmake.in      # Package configuration template
└── 📄 NetworkQuestsConfigVersion.cmake.in # Version configuration template
```

**Package Management**:
- **FindPackage Support**: Proper CMake package configuration
- **Version Management**: Semantic versioning support
- **Dependency Resolution**: Automatic dependency management
- **Cross-Platform**: Works on all supported platforms

## 📁 scripts/ - Build and Utility Scripts

**Purpose**: Automation scripts for building, installing, and maintaining the project.

```
scripts/
├── 📄 install.sh                       # Comprehensive installation script
└── 📄 cleanup.sh                       # Project cleanup script
```

**Script Capabilities**:
- **One-Command Installation**: Complete build and installation
- **Configurable Options**: Build type, features, installation path
- **Cross-Platform**: Works on Linux, macOS, and Windows (WSL)
- **Maintenance**: Cleanup and project maintenance utilities

## 🔧 Build System Architecture

### Dependency Graph

```
Core Utilities (utils/)
    ↓
Protocol Implementations (tcp/, udp/, http/, etc.)
    ↓
Examples and Tests
```

### CMake Target Organization

```cmake
# Core library targets
networkquests_core          # Core utilities
networkquests_tcp           # TCP protocol
networkquests_udp           # UDP protocol
networkquests_http          # HTTP protocol
networkquests_dns           # DNS protocol
networkquests_ftp           # FTP protocol
networkquests_websocket     # WebSocket protocol
networkquests_smtp          # SMTP protocol
networkquests_snmp          # SNMP protocol

# Interface target for users
networkquests               # Links all protocols
```

### Build Configuration

- **Modular Building**: Each protocol can be built independently
- **Optional Dependencies**: Boost and OpenSSL are optional
- **Feature Flags**: Enable/disable specific features
- **Cross-Platform**: Unified build system for all platforms

## 📦 Installation Structure

When installed, NetworkQuests follows standard directory conventions:

```
${CMAKE_INSTALL_PREFIX}/
├── include/networkquests/              # Public headers
├── lib/                                # Libraries
│   ├── libnetworkquests.a              # Static library
│   └── cmake/NetworkQuests/            # CMake package files
├── bin/examples/                       # Example applications
└── share/
    ├── doc/NetworkQuests/              # Documentation
    └── NetworkQuests/
        ├── legacy/                     # Legacy examples
        └── scripts/                    # Utility scripts
```

## 🎯 Design Goals Achieved

1. **Modularity**: Each protocol is independent and can be used separately
2. **Educational Value**: Clear structure supports learning and teaching
3. **Professional Quality**: Follows industry standards and best practices
4. **Maintainability**: Clean separation of concerns and responsibilities
5. **Extensibility**: Easy to add new protocols and features
6. **Cross-Platform**: Consistent structure across all platforms
7. **Documentation**: Comprehensive documentation at all levels

This structure ensures that NetworkQuests serves both as an educational resource and a production-ready networking library.