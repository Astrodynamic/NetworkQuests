# FTP (File Transfer Protocol) - NetworkQuests Implementation

## Table of Contents
1. [Protocol Overview](#protocol-overview)
2. [FTP Theory](#ftp-theory)
3. [Implementation Architecture](#implementation-architecture)
4. [API Reference](#api-reference)
5. [Usage Examples](#usage-examples)
6. [Best Practices](#best-practices)
7. [Troubleshooting](#troubleshooting)

## Protocol Overview

The File Transfer Protocol (FTP) is a standard communication protocol used to transfer files between computers on a network. FTP is built on a client-server model architecture and uses separate control and data connections between the client and server.

### Key Features
- **RFC 959 Compliance**: Full implementation of FTP according to RFC 959
- **Dual Connection Model**: Separate control and data connections
- **Transfer Modes**: Support for both ASCII and binary transfer modes
- **Connection Modes**: Both active and passive data connection modes
- **Authentication**: User-based authentication with permission management
- **Anonymous Access**: Optional anonymous access support
- **Comprehensive Commands**: Full suite of FTP commands for file and directory operations

### FTP Characteristics
- **Protocol**: TCP-based (reliable transport)
- **Default Ports**: 21 (control), 20 (data in active mode)
- **Connection Type**: Connection-oriented
- **Data Format**: Text commands, binary/ASCII data
- **Architecture**: Client-server with dual connections

## FTP Theory

### Connection Model

FTP uses two separate TCP connections:

1. **Control Connection (Port 21)**:
   - Used for sending commands and receiving responses
   - Remains open throughout the session
   - Text-based protocol using CRLF line endings

2. **Data Connection**:
   - Used for transferring files and directory listings
   - Established per transfer operation
   - Can use active or passive mode

### Data Connection Modes

#### Active Mode (PORT)
```
Client ←--control--→ Server (Port 21)
Client ←--data----→ Server (Port 20)
```
- Client opens a random port and sends PORT command
- Server connects back to client's specified port
- May have issues with firewalls/NAT

#### Passive Mode (PASV)
```
Client ←--control--→ Server (Port 21)
Client ←--data----→ Server (Random Port)
```
- Server opens a random port and sends PASV response
- Client connects to server's specified port
- Better compatibility with firewalls/NAT

### FTP Commands

FTP uses text-based commands sent over the control connection:

| Command | Purpose | Example |
|---------|---------|---------|
| USER | Username for login | `USER anonymous` |
| PASS | Password for login | `PASS guest@example.com` |
| PWD | Print working directory | `PWD` |
| CWD | Change working directory | `CWD /home/user` |
| LIST | List directory contents | `LIST` |
| RETR | Retrieve (download) file | `RETR file.txt` |
| STOR | Store (upload) file | `STOR file.txt` |
| DELE | Delete file | `DELE oldfile.txt` |
| MKD | Make directory | `MKD newdir` |
| RMD | Remove directory | `RMD olddir` |
| TYPE | Set transfer type | `TYPE I` (binary) |
| MODE | Set transfer mode | `MODE S` (stream) |
| STRU | Set file structure | `STRU F` (file) |
| QUIT | Terminate session | `QUIT` |

### Response Codes

FTP uses 3-digit numeric response codes:

| Range | Type | Meaning |
|-------|------|---------|
| 1xx | Positive Preliminary | Action initiated, expect another reply |
| 2xx | Positive Completion | Action completed successfully |
| 3xx | Positive Intermediate | Need more information to complete |
| 4xx | Transient Negative | Temporary failure, retry possible |
| 5xx | Permanent Negative | Permanent failure, don't retry |

Common codes:
- `220` - Service ready
- `230` - User logged in
- `150` - File status okay, about to open data connection
- `226` - Transfer complete
- `550` - File not found

## Implementation Architecture

### Core Components

```cpp
// Main Classes
class FtpClient;        // Client implementation
class FtpServer;        // Server implementation
class FtpSession;       // Individual client session
class FtpCommand;       // Command parsing and validation
class FtpResponse;      // Response formatting
class FtpDataConnection; // Data connection management

// Supporting Classes
class FtpUser;          // User account information
class FtpUserPermissions; // User permission settings
namespace Utils;        // Utility functions
```

### Class Relationships

```
FtpServer
├── TcpServer (control connection listener)
├── FtpSession[] (one per client)
│   ├── TcpSocket (control connection)
│   ├── FtpDataConnection (data transfers)
│   └── FtpCommand/FtpResponse (protocol handling)
└── FtpUser[] (user database)

FtpClient
├── TcpSocket (control connection)
├── FtpDataConnection (data transfers)
└── FtpCommand/FtpResponse (protocol handling)
```

### Thread Safety

- **FtpServer**: Thread-safe server with one thread per client session
- **FtpSession**: Single-threaded per session (one thread handles one client)
- **FtpClient**: Single-threaded (not designed for concurrent use)
- **FtpDataConnection**: Thread-safe for individual operations

## API Reference

### FtpClient Class

The FtpClient class provides a complete FTP client implementation.

#### Basic Connection Management

```cpp
class FtpClient {
public:
    // Connection management
    Result<void> connect(const std::string& host, uint16_t port = 21);
    void disconnect();
    bool is_connected() const;
    
    // Authentication
    Result<FtpResponse> login(const std::string& username, const std::string& password);
    Result<FtpResponse> login_anonymous();
    Result<FtpResponse> logout();
    bool is_logged_in() const;
};
```

#### Configuration

```cpp
class FtpClient {
public:
    // Transfer settings
    void set_transfer_mode(FtpTransferMode mode);
    FtpTransferMode get_transfer_mode() const;
    
    void set_connection_mode(FtpConnectionMode mode);
    FtpConnectionMode get_connection_mode() const;
    
    void set_timeout(std::chrono::seconds timeout);
    std::chrono::seconds get_timeout() const;
    
    // Progress tracking
    void set_progress_callback(std::function<void(size_t, size_t)> callback);
};
```

#### Directory Operations

```cpp
class FtpClient {
public:
    // Directory navigation
    Result<FtpResponse> print_working_directory();
    Result<FtpResponse> change_directory(const std::string& path);
    Result<FtpResponse> change_to_parent_directory();
    
    // Directory listing
    Result<std::string> list_directory(const std::string& path = "");
    Result<std::vector<std::string>> name_list(const std::string& path = "");
    
    // Directory management
    Result<FtpResponse> make_directory(const std::string& path);
    Result<FtpResponse> remove_directory(const std::string& path);
};
```

#### File Operations

```cpp
class FtpClient {
public:
    // File transfer
    Result<FtpResponse> upload_file(const std::filesystem::path& local_path, 
                                   const std::string& remote_path = "");
    Result<FtpResponse> download_file(const std::string& remote_path, 
                                     const std::filesystem::path& local_path = "");
    
    // File management
    Result<FtpResponse> delete_file(const std::string& remote_path);
    Result<FtpResponse> rename_file(const std::string& from_path, 
                                   const std::string& to_path);
    
    // File information
    Result<FtpResponse> get_file_size(const std::string& remote_path);
    Result<FtpResponse> get_modification_time(const std::string& remote_path);
};
```

#### System Commands

```cpp
class FtpClient {
public:
    // System information
    Result<FtpResponse> system_type();
    Result<FtpResponse> help(const std::string& command = "");
    Result<FtpResponse> noop();
    Result<FtpResponse> quit();
    
    // Raw command interface
    Result<FtpResponse> send_command(const std::string& command, 
                                    const std::string& args = "");
    Result<FtpResponse> send_command(const FtpCommand& command);
};
```

### FtpServer Class

The FtpServer class provides a complete FTP server implementation.

#### Server Management

```cpp
class FtpServer {
public:
    explicit FtpServer(uint16_t port = 21);
    ~FtpServer();
    
    // Server lifecycle
    Result<void> start();
    void stop();
    bool is_running() const;
    
    // Server configuration
    void set_root_directory(const std::string& path);
    std::string get_root_directory() const;
    
    void set_max_connections(size_t max_connections);
    size_t get_max_connections() const;
    
    void set_session_timeout(std::chrono::minutes timeout);
    std::chrono::minutes get_session_timeout() const;
    
    void set_welcome_message(const std::string& message);
    std::string get_welcome_message() const;
};
```

#### User Management

```cpp
class FtpServer {
public:
    // User management
    void add_user(const FtpUser& user);
    void remove_user(const std::string& username);
    bool authenticate_user(const std::string& username, 
                          const std::string& password) const;
    
    // Anonymous access
    void enable_anonymous_access(bool enable, 
                                const FtpUserPermissions& permissions = {});
    bool is_anonymous_enabled() const;
    
    // Server statistics
    uint16_t get_port() const;
    size_t get_active_sessions() const;
    size_t get_total_connections() const;
};
```

### FtpUser and Permissions

```cpp
struct FtpUserPermissions {
    bool can_read = true;
    bool can_write = false;
    bool can_delete = false;
    bool can_list = true;
    bool can_create_dirs = false;
    bool can_rename = false;
    std::string root_directory = "/";
    std::vector<std::string> allowed_commands;
    
    bool has_command_permission(const std::string& command) const;
};

class FtpUser {
public:
    FtpUser(const std::string& username, const std::string& password, 
            const FtpUserPermissions& permissions, bool is_anonymous = false);
    
    std::string username;
    std::string password;
    FtpUserPermissions permissions;
    bool is_anonymous;
    std::chrono::system_clock::time_point last_login;
};
```

### Enumerations

```cpp
enum class FtpTransferMode {
    ASCII,  // Text mode with CRLF conversion
    BINARY  // Binary mode (no conversion)
};

enum class FtpConnectionMode {
    ACTIVE,  // Server connects to client (PORT)
    PASSIVE  // Client connects to server (PASV)
};

enum class FtpResponseCode : uint16_t {
    // 1xx: Positive Preliminary
    RESTART_MARKER = 110,
    SERVICE_READY_SHORTLY = 120,
    DATA_CONNECTION_ALREADY_OPEN = 125,
    FILE_STATUS_OK = 150,
    
    // 2xx: Positive Completion
    COMMAND_OK = 200,
    COMMAND_NOT_IMPLEMENTED = 202,
    SYSTEM_STATUS = 211,
    DIRECTORY_STATUS = 212,
    FILE_STATUS = 213,
    HELP_MESSAGE = 214,
    SYSTEM_TYPE = 215,
    SERVICE_READY = 220,
    SERVICE_CLOSING = 221,
    DATA_CONNECTION_OPEN = 225,
    DATA_CONNECTION_CLOSED = 226,
    ENTERING_PASSIVE = 227,
    ENTERING_EPSV = 229,
    USER_LOGGED_IN = 230,
    FILE_ACTION_OK = 250,
    PATHNAME_CREATED = 257,
    
    // 3xx: Positive Intermediate
    USERNAME_OK = 331,
    NEED_ACCOUNT = 332,
    FILE_ACTION_PENDING = 350,
    
    // 4xx: Transient Negative
    SERVICE_UNAVAILABLE = 421,
    CANT_OPEN_DATA = 425,
    CONNECTION_CLOSED = 426,
    FILE_ACTION_NOT_TAKEN = 450,
    LOCAL_ERROR = 451,
    INSUFFICIENT_STORAGE = 452,
    
    // 5xx: Permanent Negative
    SYNTAX_ERROR = 500,
    SYNTAX_ERROR_PARAMS = 501,
    COMMAND_NOT_IMPLEMENTED = 502,
    BAD_SEQUENCE = 503,
    PARAMETER_NOT_IMPLEMENTED = 504,
    NOT_LOGGED_IN = 530,
    NEED_ACCOUNT_FOR_STORING = 532,
    FILE_ACTION_NOT_TAKEN_NO_FILE = 550,
    PAGE_TYPE_UNKNOWN = 551,
    EXCEEDED_STORAGE = 552,
    FILENAME_NOT_ALLOWED = 553
};
```

## Usage Examples

### Basic FTP Client

```cpp
#include "networkquests/ftp.hpp"
#include <iostream>

using namespace NetworkQuests::Ftp;

int main() {
    FtpClient client;
    
    // Connect to server
    auto connect_result = client.connect("ftp.example.com", 21);
    if (!connect_result) {
        std::cerr << "Connection failed: " << connect_result.error().message() << std::endl;
        return 1;
    }
    
    // Login
    auto login_result = client.login("username", "password");
    if (!login_result || !login_result->is_success()) {
        std::cerr << "Login failed" << std::endl;
        return 1;
    }
    
    // Set binary transfer mode
    client.set_transfer_mode(FtpTransferMode::BINARY);
    
    // Upload a file
    auto upload_result = client.upload_file("local_file.txt", "remote_file.txt");
    if (upload_result && upload_result->is_success()) {
        std::cout << "Upload successful" << std::endl;
    }
    
    // Download a file
    auto download_result = client.download_file("remote_file.txt", "downloaded_file.txt");
    if (download_result && download_result->is_success()) {
        std::cout << "Download successful" << std::endl;
    }
    
    // List directory
    auto list_result = client.list_directory();
    if (list_result) {
        std::cout << "Directory listing:\n" << *list_result << std::endl;
    }
    
    // Disconnect
    client.disconnect();
    
    return 0;
}
```

### FTP Client with Progress Tracking

```cpp
#include "networkquests/ftp.hpp"
#include <iostream>
#include <iomanip>

using namespace NetworkQuests::Ftp;

void show_progress(size_t transferred, size_t total) {
    if (total > 0) {
        double percent = (static_cast<double>(transferred) / total) * 100.0;
        std::cout << "\rProgress: " << std::fixed << std::setprecision(1) 
                  << percent << "% (" << transferred << "/" << total << " bytes)" << std::flush;
    } else {
        std::cout << "\rTransferred: " << transferred << " bytes" << std::flush;
    }
}

int main() {
    FtpClient client;
    
    // Set progress callback
    client.set_progress_callback(show_progress);
    
    // Connect and login (as in previous example)
    // ...
    
    // Upload large file with progress tracking
    auto upload_result = client.upload_file("large_file.zip", "backup.zip");
    if (upload_result && upload_result->is_success()) {
        std::cout << "\nUpload completed successfully!" << std::endl;
    }
    
    return 0;
}
```

### Anonymous FTP Access

```cpp
#include "networkquests/ftp.hpp"
#include <iostream>

using namespace NetworkQuests::Ftp;

int main() {
    FtpClient client;
    
    // Connect to public FTP server
    if (!client.connect("ftp.example.com")) {
        std::cerr << "Connection failed" << std::endl;
        return 1;
    }
    
    // Anonymous login
    auto login_result = client.login_anonymous();
    if (!login_result || !login_result->is_success()) {
        std::cerr << "Anonymous login failed" << std::endl;
        return 1;
    }
    
    // Browse public directory
    auto list_result = client.list_directory("/pub");
    if (list_result) {
        std::cout << "Public directory contents:\n" << *list_result << std::endl;
    }
    
    // Download public file
    auto download_result = client.download_file("/pub/readme.txt", "readme.txt");
    if (download_result && download_result->is_success()) {
        std::cout << "File downloaded successfully" << std::endl;
    }
    
    return 0;
}
```

### Basic FTP Server

```cpp
#include "networkquests/ftp.hpp"
#include <iostream>
#include <thread>
#include <chrono>

using namespace NetworkQuests::Ftp;

int main() {
    FtpServer server(2121); // Use non-standard port
    
    // Configure server
    server.set_root_directory("./ftp_root");
    server.set_max_connections(10);
    server.set_welcome_message("Welcome to My FTP Server");
    
    // Create user with full permissions
    FtpUserPermissions user_perms;
    user_perms.can_read = true;
    user_perms.can_write = true;
    user_perms.can_delete = true;
    user_perms.can_list = true;
    user_perms.can_create_dirs = true;
    user_perms.can_rename = true;
    user_perms.root_directory = "./ftp_root";
    
    FtpUser user("testuser", "testpass", user_perms);
    server.add_user(user);
    
    // Enable anonymous access with read-only permissions
    FtpUserPermissions anon_perms;
    anon_perms.can_read = true;
    anon_perms.can_list = true;
    anon_perms.root_directory = "./ftp_root";
    
    server.enable_anonymous_access(true, anon_perms);
    
    // Start server
    auto start_result = server.start();
    if (!start_result) {
        std::cerr << "Failed to start server: " << start_result.error().message() << std::endl;
        return 1;
    }
    
    std::cout << "FTP server started on port " << server.get_port() << std::endl;
    std::cout << "Press Ctrl+C to stop..." << std::endl;
    
    // Keep server running
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        // Print server statistics
        static int counter = 0;
        if (++counter % 60 == 0) { // Every minute
            std::cout << "Active sessions: " << server.get_active_sessions() 
                      << ", Total connections: " << server.get_total_connections() << std::endl;
        }
    }
    
    return 0;
}
```

### Advanced Server Configuration

```cpp
#include "networkquests/ftp.hpp"
#include <iostream>
#include <filesystem>

using namespace NetworkQuests::Ftp;

int main() {
    FtpServer server(21);
    
    // Create directory structure
    std::filesystem::create_directories("./ftp_root/public");
    std::filesystem::create_directories("./ftp_root/private");
    std::filesystem::create_directories("./ftp_root/uploads");
    
    server.set_root_directory("./ftp_root");
    
    // Create admin user with full access
    FtpUserPermissions admin_perms;
    admin_perms.can_read = true;
    admin_perms.can_write = true;
    admin_perms.can_delete = true;
    admin_perms.can_list = true;
    admin_perms.can_create_dirs = true;
    admin_perms.can_rename = true;
    admin_perms.root_directory = "./ftp_root";
    
    FtpUser admin("admin", "secure_password", admin_perms);
    server.add_user(admin);
    
    // Create regular user with limited access
    FtpUserPermissions user_perms;
    user_perms.can_read = true;
    user_perms.can_write = true;
    user_perms.can_list = true;
    user_perms.can_delete = false;
    user_perms.can_create_dirs = false;
    user_perms.can_rename = false;
    user_perms.root_directory = "./ftp_root/uploads";
    
    FtpUser regular_user("user", "user_password", user_perms);
    server.add_user(regular_user);
    
    // Configure anonymous access to public directory only
    FtpUserPermissions anon_perms;
    anon_perms.can_read = true;
    anon_perms.can_list = true;
    anon_perms.root_directory = "./ftp_root/public";
    
    server.enable_anonymous_access(true, anon_perms);
    
    // Advanced settings
    server.set_max_connections(50);
    server.set_session_timeout(std::chrono::minutes(15));
    
    // Start server
    auto result = server.start();
    if (result) {
        std::cout << "Advanced FTP server started successfully!" << std::endl;
        
        // Server management loop would go here
        // ...
        
    } else {
        std::cerr << "Failed to start server: " << result.error().message() << std::endl;
    }
    
    return 0;
}
```

## Best Practices

### Client Best Practices

1. **Connection Management**:
   ```cpp
   // Always check connection results
   auto result = client.connect(host, port);
   if (!result) {
       // Handle connection failure
       std::cerr << "Connection failed: " << result.error().message() << std::endl;
       return;
   }
   
   // Use RAII for automatic cleanup
   class FtpConnection {
       FtpClient& client_;
   public:
       FtpConnection(FtpClient& client) : client_(client) {}
       ~FtpConnection() { client_.disconnect(); }
   };
   ```

2. **Error Handling**:
   ```cpp
   // Always check operation results
   auto upload_result = client.upload_file("file.txt");
   if (!upload_result || !upload_result->is_success()) {
       std::cerr << "Upload failed: " 
                 << (upload_result ? upload_result->get_message() : "Unknown error") 
                 << std::endl;
   }
   ```

3. **Transfer Mode Selection**:
   ```cpp
   // Use binary mode for non-text files
   client.set_transfer_mode(FtpTransferMode::BINARY);
   
   // Use ASCII mode only for text files that need line ending conversion
   client.set_transfer_mode(FtpTransferMode::ASCII);
   ```

4. **Connection Mode**:
   ```cpp
   // Prefer passive mode for better firewall compatibility
   client.set_connection_mode(FtpConnectionMode::PASSIVE);
   ```

### Server Best Practices

1. **Security**:
   ```cpp
   // Use secure root directory
   server.set_root_directory("/var/ftp/secure");
   
   // Limit anonymous access
   FtpUserPermissions anon_perms;
   anon_perms.can_read = true;
   anon_perms.can_list = true;
   anon_perms.can_write = false;  // Read-only
   anon_perms.root_directory = "/var/ftp/public";
   server.enable_anonymous_access(true, anon_perms);
   ```

2. **Resource Management**:
   ```cpp
   // Limit concurrent connections
   server.set_max_connections(20);
   
   // Set reasonable session timeout
   server.set_session_timeout(std::chrono::minutes(10));
   ```

3. **User Management**:
   ```cpp
   // Use strong passwords for regular users
   FtpUser user("john", "strong_random_password_123!", user_perms);
   
   // Apply principle of least privilege
   FtpUserPermissions limited_perms;
   limited_perms.can_read = true;
   limited_perms.can_write = false;  // No write access
   limited_perms.can_delete = false; // No delete access
   ```

### Performance Optimization

1. **Buffer Sizes**:
   ```cpp
   // The implementation uses optimized buffer sizes automatically
   // FTP_BUFFER_SIZE is set to 4096 bytes for good performance
   ```

2. **Progress Tracking**:
   ```cpp
   // Use progress callbacks for large file transfers
   client.set_progress_callback([](size_t transferred, size_t total) {
       // Update progress indicator
       if (total > 0) {
           double percent = (double)transferred / total * 100.0;
           update_progress_bar(percent);
       }
   });
   ```

3. **Connection Pooling** (for high-frequency operations):
   ```cpp
   // For applications with many short-lived operations,
   // consider maintaining persistent connections
   class FtpConnectionPool {
       std::vector<std::unique_ptr<FtpClient>> pool_;
   public:
       FtpClient* acquire() {
           // Return available connection or create new one
       }
       void release(FtpClient* client) {
           // Return connection to pool
       }
   };
   ```

## Troubleshooting

### Common Connection Issues

1. **"Connection refused"**:
   - Check if FTP server is running
   - Verify port number (default 21)
   - Check firewall settings

2. **"Connection timeout"**:
   - Network connectivity issues
   - Server may be overloaded
   - Increase timeout value

3. **"Login failed"**:
   - Incorrect username/password
   - Account may be disabled
   - Check user permissions

### Data Transfer Issues

1. **"Can't open data connection"**:
   - Try switching between active/passive modes
   - Check firewall configuration
   - Verify network topology

2. **Transfer hangs or fails**:
   - Network connectivity problems
   - Check transfer mode (ASCII vs Binary)
   - Verify file permissions on server

3. **Corrupted transfers**:
   - Use binary mode for non-text files
   - Check for network packet loss
   - Verify checksums if available

### Server-Specific Issues

1. **"Permission denied"**:
   - Check user permissions
   - Verify file/directory ownership
   - Check root directory access

2. **"Maximum connections exceeded"**:
   - Increase max_connections setting
   - Check for connection leaks
   - Monitor server resources

### Debugging Tips

1. **Enable Logging**:
   ```cpp
   Logger::set_level(Logger::Level::DEBUG);
   ```

2. **Raw Command Interface**:
   ```cpp
   // Use raw commands for debugging
   auto result = client.send_command("HELP");
   std::cout << "Response: " << result->get_code_number() 
             << " " << result->get_message() << std::endl;
   ```

3. **Network Analysis**:
   - Use tools like Wireshark to capture FTP traffic
   - Monitor both control (port 21) and data connections
   - Check for proper command/response sequences

4. **Server Logs**:
   ```cpp
   // Monitor server statistics
   std::cout << "Active sessions: " << server.get_active_sessions() << std::endl;
   std::cout << "Total connections: " << server.get_total_connections() << std::endl;
   ```

### Platform-Specific Considerations

1. **Windows**:
   - Ensure Windows Sockets (Winsock) is properly initialized
   - Check Windows Firewall settings
   - Consider Windows Defender interference

2. **Linux/Unix**:
   - Check iptables/firewall rules
   - Verify port availability (`netstat -an | grep :21`)
   - Check SELinux policies if applicable

3. **Passive Mode and NAT**:
   - Configure NAT to forward data port ranges
   - Use PASV mode for clients behind NAT
   - Consider using EPSV for IPv6 compatibility

This completes the comprehensive FTP documentation for the NetworkQuests library. The implementation provides a robust, RFC 959-compliant FTP solution suitable for both educational and production use.