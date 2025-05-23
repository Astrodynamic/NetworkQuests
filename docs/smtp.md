# SMTP Protocol Implementation

## Table of Contents
- [Overview](#overview)
- [Protocol Theory](#protocol-theory)
- [Implementation Architecture](#implementation-architecture)
- [API Reference](#api-reference)
- [Usage Examples](#usage-examples)
- [Advanced Features](#advanced-features)
- [Best Practices](#best-practices)
- [Troubleshooting](#troubleshooting)
- [Performance](#performance)
- [Security Considerations](#security-considerations)

## Overview

The Simple Mail Transfer Protocol (SMTP) is an Internet standard for electronic mail transmission. This implementation provides both client and server functionality following RFC 5321 specifications, offering a complete solution for email sending and receiving in educational and development environments.

### Key Features

- **RFC 5321 Compliant**: Full implementation of SMTP protocol standards
- **Client and Server**: Both sending and receiving email capabilities
- **Authentication Support**: PLAIN and LOGIN authentication methods
- **MIME Support**: Complete email formatting with attachments
- **Multi-threading**: Concurrent connection handling for servers
- **Extension Framework**: Support for SMTP extensions (SIZE, AUTH, STARTTLS)
- **Educational Focus**: Clear code structure for learning networking concepts

## Protocol Theory

### SMTP Fundamentals

SMTP is a text-based protocol that operates on TCP port 25 (standard) or 587 (submission). The protocol follows a command-response pattern where the client sends commands and the server responds with numeric status codes.

#### Basic SMTP Session Flow

```
Client                           Server
  |                               |
  |--- TCP Connection ----------->|
  |<---------- 220 Ready ---------|
  |--- EHLO client.example ------>|
  |<----- 250 Hello client -------|
  |--- MAIL FROM:<sender> ------->|
  |<--------- 250 OK -------------|
  |--- RCPT TO:<recipient> ------>|
  |<--------- 250 OK -------------|
  |--- DATA ---------------------->|
  |<-- 354 Start mail input ------|
  |--- Message Content ----------->|
  |--- . (End of data) ----------->|
  |<--------- 250 OK -------------|
  |--- QUIT ---------------------->|
  |<--- 221 Goodbye --------------|
  |<------- Connection Close -----|
```

### SMTP Commands

| Command | Purpose | Example |
|---------|---------|---------|
| EHLO | Extended Hello | `EHLO client.example.com` |
| HELO | Hello (basic) | `HELO client.example.com` |
| MAIL FROM | Specify sender | `MAIL FROM:<user@example.com>` |
| RCPT TO | Specify recipient | `RCPT TO:<dest@example.com>` |
| DATA | Start message transfer | `DATA` |
| RSET | Reset session | `RSET` |
| QUIT | End session | `QUIT` |
| AUTH | Authenticate | `AUTH PLAIN <credentials>` |
| VRFY | Verify address | `VRFY user@example.com` |
| NOOP | No operation | `NOOP` |

### Response Codes

- **2xx Success**: Command completed successfully
- **3xx Intermediate**: Command accepted, waiting for more data
- **4xx Temporary Failure**: Command failed temporarily, try again
- **5xx Permanent Failure**: Command failed permanently

## Implementation Architecture

### Core Components

```
SmtpClient
├── SmtpConnection      # Connection management
├── SmtpMessage         # Email message handling
├── EmailAddress        # Address validation and formatting
└── smtp_utils          # Protocol utilities

SmtpServer
├── SmtpConnection      # Connection handling
├── SessionState        # Per-client session tracking
├── MessageHandler      # Incoming message processing
└── Authentication      # User validation
```

### Class Hierarchy

```cpp
// Core message handling
EmailAddress               // Email address representation
SmtpMessage               // Complete email message
SmtpCommand_              // SMTP command representation
SmtpResponse              // SMTP response representation

// Network layer
SmtpConnection            // Protocol communication layer
SmtpClient                // Email sending client
SmtpServer                // Email receiving server

// Utilities
smtp_utils::*             // Protocol helper functions
```

## API Reference

### EmailAddress Class

Represents an email address with optional display name.

```cpp
class EmailAddress {
public:
    EmailAddress();
    EmailAddress(std::string_view address);
    EmailAddress(std::string_view name, std::string_view address);
    
    // Accessors
    const std::string& name() const;
    const std::string& address() const;
    
    // Validation
    bool is_valid() const;
    
    // Formatting
    std::string to_string() const;
    std::string to_address_only() const;
    
    // Parsing
    static Result<EmailAddress> from_string(std::string_view str);
};
```

#### Usage Examples

```cpp
// Simple address
EmailAddress addr1("user@example.com");

// Address with display name
EmailAddress addr2("John Doe", "john@example.com");

// From string with name
auto addr3 = EmailAddress::from_string("Jane Smith <jane@example.com>");

// Validation
if (addr1.is_valid()) {
    std::cout << "Valid address: " << addr1.to_string() << std::endl;
}
```

### SmtpMessage Class

Represents a complete email message with headers, body, and attachments.

```cpp
class SmtpMessage {
public:
    SmtpMessage() = default;
    
    // Basic properties
    void set_from(const EmailAddress& from);
    void add_to(const EmailAddress& to);
    void add_cc(const EmailAddress& cc);
    void add_bcc(const EmailAddress& bcc);
    void set_subject(std::string_view subject);
    void set_body(std::string_view body);
    void set_priority(EmailPriority priority);
    
    // Headers
    void set_header(std::string_view name, std::string_view value);
    std::optional<std::string> get_header(std::string_view name) const;
    
    // Attachments
    void add_attachment(const Attachment& attachment);
    void add_attachment_from_file(std::string_view filename, 
                                  std::string_view content_type = "");
    
    // Message generation
    std::string to_mime_string() const;
    bool is_valid() const;
    size_t estimated_size() const;
};
```

#### Usage Examples

```cpp
SmtpMessage message;

// Set basic fields
message.set_from(EmailAddress("sender@example.com"));
message.add_to(EmailAddress("John Doe", "john@example.com"));
message.add_cc(EmailAddress("jane@example.com"));
message.set_subject("Important Message");
message.set_body("Hello, this is the message body.");
message.set_priority(EmailPriority::High);

// Custom headers
message.set_header("X-Custom-Header", "custom-value");
message.set_header("Reply-To", "noreply@example.com");

// Attachments
message.add_attachment_from_file("document.pdf");

// Generate MIME content
std::string mime_content = message.to_mime_string();
```

### SmtpClient Class

Client for sending emails through SMTP servers.

```cpp
class SmtpClient {
public:
    SmtpClient();
    explicit SmtpClient(std::chrono::milliseconds timeout);
    
    // Connection
    Result<void> connect(std::string_view hostname, Port port = 25);
    Result<void> connect_secure(std::string_view hostname, Port port = 465);
    void disconnect();
    bool is_connected() const;
    
    // Authentication
    Result<void> authenticate(std::string_view username, 
                             std::string_view password, 
                             SmtpAuthMethod method = SmtpAuthMethod::PLAIN);
    Result<void> start_tls();
    
    // Mail operations
    Result<void> send_message(const SmtpMessage& message);
    Result<void> send_raw_message(const EmailAddress& from, 
                                 const std::vector<EmailAddress>& to,
                                 std::string_view message_data);
    
    // Server capabilities
    Result<std::vector<SmtpExtension>> get_extensions();
    bool supports_extension(SmtpExtension extension) const;
    
    // Settings
    void set_timeout(std::chrono::milliseconds timeout);
    void set_hostname(std::string_view hostname);
    void set_max_message_size(size_t size);
};
```

#### Usage Examples

```cpp
// Basic email sending
SmtpClient client;
auto result = client.connect("smtp.example.com", 587);
if (result) {
    SmtpMessage msg;
    msg.set_from(EmailAddress("sender@example.com"));
    msg.add_to(EmailAddress("recipient@example.com"));
    msg.set_subject("Test Message");
    msg.set_body("Hello, World!");
    
    auto send_result = client.send_message(msg);
    if (send_result) {
        std::cout << "Message sent successfully!" << std::endl;
    }
    client.disconnect();
}

// Authenticated sending
SmtpClient auth_client;
auth_client.connect("smtp.gmail.com", 587);
auth_client.authenticate("username", "password", SmtpAuthMethod::PLAIN);
auth_client.send_message(message);
auth_client.disconnect();
```

### SmtpServer Class

Server for receiving emails via SMTP.

```cpp
class SmtpServer {
public:
    using MessageHandler = std::function<void(const SmtpMessage&, const SocketAddress&)>;
    using AuthValidator = std::function<bool(std::string_view, std::string_view)>;
    using RecipientValidator = std::function<bool(const EmailAddress&)>;
    
    explicit SmtpServer(Port port = 25);
    SmtpServer(const SocketAddress& bind_addr);
    
    // Server lifecycle
    Result<void> start();
    void stop();
    bool is_running() const;
    
    // Event handlers
    void set_message_handler(MessageHandler handler);
    void set_auth_validator(AuthValidator validator);
    void set_recipient_validator(RecipientValidator validator);
    
    // Configuration
    void set_hostname(std::string_view hostname);
    void set_max_connections(int max_conn);
    void set_max_message_size(size_t size);
    void set_require_auth(bool require);
    void add_supported_extension(SmtpExtension extension);
    
    // Server information
    SocketAddress local_address() const;
    size_t active_connections() const;
};
```

#### Usage Examples

```cpp
// Basic SMTP server
SmtpServer server(2525);

// Set up message handler
server.set_message_handler([](const SmtpMessage& msg, const SocketAddress& sender) {
    std::cout << "Received message from: " << msg.from().to_string() << std::endl;
    std::cout << "Subject: " << msg.subject() << std::endl;
    
    // Save message to file or database
    std::ofstream file("received_mail.eml");
    file << msg.to_mime_string();
});

// Set up authentication
server.set_auth_validator([](std::string_view username, std::string_view password) {
    return username == "admin" && password == "secret";
});

// Configure server
server.set_hostname("mail.example.com");
server.set_max_connections(100);
server.set_require_auth(false);
server.add_supported_extension(SmtpExtension::AUTH);

// Start server
auto result = server.start();
if (result) {
    std::cout << "SMTP server started on port 2525" << std::endl;
    
    // Keep running until user input
    std::cout << "Press Enter to stop..." << std::endl;
    std::cin.get();
    
    server.stop();
}
```

## Usage Examples

### Simple Email Sending

```cpp
#include "networkquests/smtp.hpp"
using namespace networkquests::smtp;

int main() {
    SmtpClient client;
    
    // Connect to SMTP server
    auto connect_result = client.connect("localhost", 25);
    if (!connect_result) {
        std::cerr << "Failed to connect: " << connect_result.error().message << std::endl;
        return 1;
    }
    
    // Create message
    SmtpMessage message;
    message.set_from(EmailAddress("NetworkQuests", "demo@networkquests.edu"));
    message.add_to(EmailAddress("Student", "student@university.edu"));
    message.set_subject("Welcome to NetworkQuests SMTP!");
    message.set_body("This is your first email sent using NetworkQuests SMTP implementation.");
    
    // Send message
    auto send_result = client.send_message(message);
    if (send_result) {
        std::cout << "Email sent successfully!" << std::endl;
    } else {
        std::cerr << "Failed to send: " << send_result.error().message << std::endl;
    }
    
    client.disconnect();
    return 0;
}
```

### Email with Attachments

```cpp
SmtpMessage message;
message.set_from(EmailAddress("sender@example.com"));
message.add_to(EmailAddress("recipient@example.com"));
message.set_subject("Document Attached");
message.set_body("Please find the attached document.");

// Add file attachment
message.add_attachment_from_file("report.pdf", "application/pdf");

// Add inline image
SmtpMessage::Attachment image;
image.filename = "logo.png";
image.content_type = "image/png";
image.inline_attachment = true;

// Read file content
std::ifstream file("logo.png", std::ios::binary);
if (file) {
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    image.data.resize(size);
    file.read(reinterpret_cast<char*>(image.data.data()), size);
    
    message.add_attachment(image);
}

// Send with attachments
SmtpClient client;
client.connect("smtp.example.com", 587);
client.send_message(message);
client.disconnect();
```

### Bulk Email Sending

```cpp
SmtpClient client;
client.connect("smtp.example.com", 587);
client.authenticate("username", "password");

std::vector<EmailAddress> recipients = {
    EmailAddress("user1@example.com"),
    EmailAddress("user2@example.com"),
    EmailAddress("user3@example.com")
};

for (const auto& recipient : recipients) {
    SmtpMessage message;
    message.set_from(EmailAddress("newsletter@company.com"));
    message.add_to(recipient);
    message.set_subject("Monthly Newsletter");
    message.set_body("Welcome to our monthly newsletter...");
    
    auto result = client.send_message(message);
    if (result) {
        std::cout << "Sent to " << recipient.to_string() << std::endl;
    } else {
        std::cerr << "Failed to send to " << recipient.to_string() << std::endl;
    }
    
    // Rate limiting
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

client.disconnect();
```

### Custom SMTP Server

```cpp
class MailServer {
private:
    SmtpServer server_;
    std::vector<SmtpMessage> received_messages_;
    std::mutex messages_mutex_;

public:
    MailServer(Port port) : server_(port) {
        setup_handlers();
    }
    
    void setup_handlers() {
        // Message handler
        server_.set_message_handler([this](const SmtpMessage& message, const SocketAddress& sender) {
            std::lock_guard<std::mutex> lock(messages_mutex_);
            received_messages_.push_back(message);
            
            std::cout << "New message received:" << std::endl;
            std::cout << "  From: " << message.from().to_string() << std::endl;
            std::cout << "  Subject: " << message.subject() << std::endl;
            std::cout << "  Sender IP: " << sender.to_string() << std::endl;
            
            // Save to file
            save_message(message);
        });
        
        // Authentication handler
        server_.set_auth_validator([](std::string_view username, std::string_view password) {
            // Simple authentication - in real code, use secure storage
            return username == "admin" && password == "secret123";
        });
        
        // Recipient validation
        server_.set_recipient_validator([](const EmailAddress& email) {
            // Accept all emails for @localhost domain
            return email.address().find("@localhost") != std::string::npos;
        });
        
        // Configure server
        server_.set_hostname("networkquests.local");
        server_.set_max_connections(50);
        server_.add_supported_extension(SmtpExtension::AUTH);
        server_.add_supported_extension(SmtpExtension::SIZE);
    }
    
    Result<void> start() {
        return server_.start();
    }
    
    void stop() {
        server_.stop();
    }
    
    size_t message_count() const {
        std::lock_guard<std::mutex> lock(messages_mutex_);
        return received_messages_.size();
    }

private:
    void save_message(const SmtpMessage& message) {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto tm = std::localtime(&time_t);
        
        std::ostringstream filename;
        filename << "mail_" << std::put_time(tm, "%Y%m%d_%H%M%S") << ".eml";
        
        std::ofstream file(filename.str());
        if (file) {
            file << message.to_mime_string();
        }
    }
};

int main() {
    MailServer server(2525);
    
    auto result = server.start();
    if (result) {
        std::cout << "Mail server started on port 2525" << std::endl;
        std::cout << "Press Enter to stop..." << std::endl;
        std::cin.get();
        
        server.stop();
        std::cout << "Total messages received: " << server.message_count() << std::endl;
    } else {
        std::cerr << "Failed to start server: " << result.error().message << std::endl;
    }
    
    return 0;
}
```

## Advanced Features

### SMTP Extensions

The implementation supports several SMTP extensions:

```cpp
// Check server capabilities
SmtpClient client;
client.connect("smtp.example.com", 587);

auto extensions = client.get_extensions();
if (extensions) {
    for (auto ext : extensions.value()) {
        std::cout << "Supported: " << smtp_utils::extension_to_string(ext) << std::endl;
    }
}

// Use specific extensions
if (client.supports_extension(SmtpExtension::STARTTLS)) {
    client.start_tls();
}

if (client.supports_extension(SmtpExtension::AUTH)) {
    client.authenticate("user", "pass", SmtpAuthMethod::PLAIN);
}
```

### Custom Message Headers

```cpp
SmtpMessage message;
message.set_from(EmailAddress("sender@example.com"));
message.add_to(EmailAddress("recipient@example.com"));
message.set_subject("Custom Headers Demo");
message.set_body("This message has custom headers.");

// Add custom headers
message.set_header("X-Mailer", "NetworkQuests SMTP 1.0");
message.set_header("X-Priority", "High");
message.set_header("Reply-To", "noreply@example.com");
message.set_header("Organization", "NetworkQuests Project");
message.set_header("X-Custom-ID", "MSG-12345");

// The headers will be included in the MIME output
std::string mime = message.to_mime_string();
```

### Email Address Parsing

```cpp
// Parse various email address formats
auto addr1 = EmailAddress::from_string("user@example.com");
auto addr2 = EmailAddress::from_string("John Doe <john@example.com>");
auto addr3 = EmailAddress::from_string("\"Smith, Jane\" <jane@example.com>");

if (addr1) {
    std::cout << "Parsed: " << addr1.value().to_string() << std::endl;
}

// Extract parts
std::string domain = smtp_utils::extract_domain("user@example.com");
std::string local = smtp_utils::extract_local_part("user@example.com");

// Validation
bool valid = smtp_utils::is_valid_email_address("test@example.com");
```

### MIME Encoding

```cpp
// Automatic MIME encoding for non-ASCII content
SmtpMessage message;
message.set_subject("Тест сообщения 测试消息");  // Unicode subject
message.set_body("Content with émojis 🚀 and üñíçödé characters");

// Headers are automatically encoded using RFC 2047
std::string encoded_subject = smtp_utils::encode_mime_header("Тест сообщения", "UTF-8");
std::cout << "Encoded: " << encoded_subject << std::endl;

// Base64 encoding utilities
std::string original = "Hello, World!";
std::string encoded = smtp_utils::base64_encode(original);
auto decoded = smtp_utils::base64_decode(encoded);

std::cout << "Original: " << original << std::endl;
std::cout << "Encoded: " << encoded << std::endl;
std::cout << "Decoded: " << decoded.value() << std::endl;
```

## Best Practices

### Error Handling

Always check return values and handle errors appropriately:

```cpp
SmtpClient client;

auto connect_result = client.connect("smtp.example.com", 587);
if (!connect_result) {
    std::cerr << "Connection failed: " << connect_result.error().message << std::endl;
    return;
}

auto auth_result = client.authenticate("user", "pass");
if (!auth_result) {
    std::cerr << "Authentication failed: " << auth_result.error().message << std::endl;
    client.disconnect();
    return;
}

SmtpMessage message;
// ... set message properties ...

auto send_result = client.send_message(message);
if (!send_result) {
    std::cerr << "Send failed: " << send_result.error().message << std::endl;
} else {
    std::cout << "Message sent successfully!" << std::endl;
}

client.disconnect();
```

### Resource Management

Use RAII and proper cleanup:

```cpp
class SmtpConnection {
private:
    SmtpClient client_;
    bool connected_ = false;

public:
    SmtpConnection(std::string_view host, Port port) {
        auto result = client_.connect(host, port);
        if (result) {
            connected_ = true;
        } else {
            throw std::runtime_error("Failed to connect: " + result.error().message);
        }
    }
    
    ~SmtpConnection() {
        if (connected_) {
            client_.disconnect();
        }
    }
    
    SmtpClient& client() { return client_; }
    bool is_connected() const { return connected_; }
};

// Usage
try {
    SmtpConnection conn("smtp.example.com", 587);
    // ... use conn.client() ...
    // Automatic cleanup when leaving scope
} catch (const std::exception& e) {
    std::cerr << "Connection error: " << e.what() << std::endl;
}
```

### Message Validation

Always validate messages before sending:

```cpp
SmtpMessage message;
message.set_from(EmailAddress("sender@example.com"));
message.add_to(EmailAddress("recipient@example.com"));
message.set_subject("Test Message");
message.set_body("Hello, World!");

if (!message.is_valid()) {
    std::cerr << "Invalid message - missing required fields" << std::endl;
    return;
}

// Check message size
size_t estimated_size = message.estimated_size();
if (estimated_size > 10 * 1024 * 1024) {  // 10MB limit
    std::cerr << "Message too large: " << estimated_size << " bytes" << std::endl;
    return;
}

// Validate email addresses
if (!message.from().is_valid()) {
    std::cerr << "Invalid sender address" << std::endl;
    return;
}

for (const auto& addr : message.to()) {
    if (!addr.is_valid()) {
        std::cerr << "Invalid recipient: " << addr.to_string() << std::endl;
        return;
    }
}
```

### Security Best Practices

```cpp
// Always use authentication when available
SmtpClient client;
client.connect("smtp.example.com", 587);

// Check for STARTTLS support
if (client.supports_extension(SmtpExtension::STARTTLS)) {
    auto tls_result = client.start_tls();
    if (!tls_result) {
        std::cerr << "Failed to start TLS" << std::endl;
        client.disconnect();
        return;
    }
}

// Use secure authentication
if (client.supports_extension(SmtpExtension::AUTH)) {
    auto auth_result = client.authenticate("username", "password", SmtpAuthMethod::PLAIN);
    if (!auth_result) {
        std::cerr << "Authentication failed" << std::endl;
        client.disconnect();
        return;
    }
}

// Set reasonable timeouts
client.set_timeout(std::chrono::seconds(30));
```

## Troubleshooting

### Common Issues and Solutions

#### Connection Problems

**Problem**: "Failed to connect to SMTP server"

**Solutions**:
- Check if the server is running and accessible
- Verify the hostname and port number
- Check firewall settings
- Test with telnet: `telnet smtp.example.com 25`

```cpp
// Debug connection issues
SmtpClient client;
client.set_timeout(std::chrono::seconds(10));  // Shorter timeout for testing

auto result = client.connect("smtp.example.com", 25);
if (!result) {
    std::cerr << "Connection failed: " << result.error().message << std::endl;
    
    // Try alternative ports
    for (Port port : {587, 465, 2525}) {
        std::cout << "Trying port " << port << "..." << std::endl;
        auto alt_result = client.connect("smtp.example.com", port);
        if (alt_result) {
            std::cout << "Connected on port " << port << std::endl;
            break;
        }
    }
}
```

#### Authentication Issues

**Problem**: "Authentication failed"

**Solutions**:
- Check username and password
- Verify authentication method support
- Check if account is locked or requires app passwords

```cpp
// Debug authentication
SmtpClient client;
client.connect("smtp.gmail.com", 587);

// Check server capabilities
auto extensions = client.get_extensions();
if (extensions) {
    bool has_auth = false;
    for (auto ext : extensions.value()) {
        if (ext == SmtpExtension::AUTH) {
            has_auth = true;
            break;
        }
    }
    
    if (!has_auth) {
        std::cerr << "Server does not support authentication" << std::endl;
        return;
    }
}

// Try different auth methods
std::vector<SmtpAuthMethod> methods = {
    SmtpAuthMethod::PLAIN,
    SmtpAuthMethod::LOGIN
};

for (auto method : methods) {
    std::cout << "Trying auth method: " << smtp_utils::auth_method_to_string(method) << std::endl;
    auto result = client.authenticate("username", "password", method);
    if (result) {
        std::cout << "Authentication successful with " 
                  << smtp_utils::auth_method_to_string(method) << std::endl;
        break;
    } else {
        std::cout << "Auth failed: " << result.error().message << std::endl;
    }
}
```

#### Message Format Issues

**Problem**: "Invalid message format" or "Message rejected"

**Solutions**:
- Validate email addresses
- Check message size limits
- Ensure proper MIME formatting

```cpp
// Debug message issues
SmtpMessage message;
message.set_from(EmailAddress("sender@example.com"));
message.add_to(EmailAddress("recipient@example.com"));
message.set_subject("Test Message");
message.set_body("Hello, World!");

// Validate message
if (!message.is_valid()) {
    std::cerr << "Message validation failed:" << std::endl;
    
    if (!message.from().is_valid()) {
        std::cerr << "  - Invalid sender address" << std::endl;
    }
    
    if (message.to().empty()) {
        std::cerr << "  - No recipients specified" << std::endl;
    }
    
    for (const auto& addr : message.to()) {
        if (!addr.is_valid()) {
            std::cerr << "  - Invalid recipient: " << addr.to_string() << std::endl;
        }
    }
}

// Check message size
size_t size = message.estimated_size();
std::cout << "Message size: " << size << " bytes" << std::endl;

// Generate and inspect MIME content
std::string mime = message.to_mime_string();
std::cout << "MIME content preview:" << std::endl;
std::cout << mime.substr(0, 500) << "..." << std::endl;
```

#### Server Issues

**Problem**: SMTP server not accepting connections

**Solutions**:
- Check server configuration
- Verify port binding
- Check user permissions

```cpp
// Debug server startup
SmtpServer server(2525);

// Set up detailed logging
server.set_message_handler([](const SmtpMessage& msg, const SocketAddress& sender) {
    std::cout << "Message received from " << sender.to_string() << std::endl;
    std::cout << "From: " << msg.from().to_string() << std::endl;
    std::cout << "Subject: " << msg.subject() << std::endl;
});

server.set_auth_validator([](std::string_view user, std::string_view pass) {
    std::cout << "Auth attempt: user=" << user << std::endl;
    return user == "test" && pass == "test123";
});

auto result = server.start();
if (!result) {
    std::cerr << "Server start failed: " << result.error().message << std::endl;
    
    // Try different ports
    for (Port port : {2525, 2526, 8025}) {
        SmtpServer alt_server(port);
        auto alt_result = alt_server.start();
        if (alt_result) {
            std::cout << "Server started on port " << port << std::endl;
            std::cout << "Press Enter to stop..." << std::endl;
            std::cin.get();
            alt_server.stop();
            break;
        }
    }
}
```

### Debugging Tools

#### Protocol Tracing

```cpp
class DebugSmtpClient : public SmtpClient {
public:
    DebugSmtpClient() : SmtpClient() {}
    
    Result<void> connect_debug(std::string_view hostname, Port port) {
        std::cout << "Connecting to " << hostname << ":" << port << std::endl;
        
        auto result = connect(hostname, port);
        if (result) {
            std::cout << "Connected successfully" << std::endl;
            
            // Get server capabilities
            auto extensions = get_extensions();
            if (extensions) {
                std::cout << "Server extensions:" << std::endl;
                for (auto ext : extensions.value()) {
                    std::cout << "  - " << smtp_utils::extension_to_string(ext) << std::endl;
                }
            }
        } else {
            std::cout << "Connection failed: " << result.error().message << std::endl;
        }
        
        return result;
    }
};
```

#### Message Analysis

```cpp
void analyze_message(const SmtpMessage& message) {
    std::cout << "Message Analysis:" << std::endl;
    std::cout << "  Valid: " << (message.is_valid() ? "Yes" : "No") << std::endl;
    std::cout << "  From: " << message.from().to_string() << std::endl;
    std::cout << "  To count: " << message.to().size() << std::endl;
    std::cout << "  CC count: " << message.cc().size() << std::endl;
    std::cout << "  BCC count: " << message.bcc().size() << std::endl;
    std::cout << "  Subject: " << message.subject() << std::endl;
    std::cout << "  Body length: " << message.body().length() << " chars" << std::endl;
    std::cout << "  Attachments: " << message.attachments().size() << std::endl;
    std::cout << "  Estimated size: " << message.estimated_size() << " bytes" << std::endl;
    std::cout << "  Priority: " << static_cast<int>(message.priority()) << std::endl;
    
    // Check encoding needs
    bool needs_encoding = false;
    for (char c : message.subject()) {
        if (static_cast<unsigned char>(c) > 127) {
            needs_encoding = true;
            break;
        }
    }
    std::cout << "  Subject needs encoding: " << (needs_encoding ? "Yes" : "No") << std::endl;
}
```

## Performance

### Optimization Guidelines

#### Connection Reuse

```cpp
class SmtpPool {
private:
    std::vector<std::unique_ptr<SmtpClient>> clients_;
    std::mutex mutex_;
    std::string hostname_;
    Port port_;
    size_t max_size_;

public:
    SmtpPool(std::string_view hostname, Port port, size_t max_size = 10)
        : hostname_(hostname), port_(port), max_size_(max_size) {}
    
    std::unique_ptr<SmtpClient> get_client() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!clients_.empty()) {
            auto client = std::move(clients_.back());
            clients_.pop_back();
            return client;
        }
        
        // Create new client
        auto client = std::make_unique<SmtpClient>();
        auto result = client->connect(hostname_, port_);
        if (result) {
            return client;
        }
        
        return nullptr;
    }
    
    void return_client(std::unique_ptr<SmtpClient> client) {
        if (!client || !client->is_connected()) {
            return;
        }
        
        std::lock_guard<std::mutex> lock(mutex_);
        if (clients_.size() < max_size_) {
            clients_.push_back(std::move(client));
        }
        // Otherwise, let it be destroyed
    }
};

// Usage
SmtpPool pool("smtp.example.com", 587, 5);

auto client = pool.get_client();
if (client) {
    // Send messages
    client->send_message(message1);
    client->send_message(message2);
    
    // Return to pool
    pool.return_client(std::move(client));
}
```

#### Bulk Operations

```cpp
void send_bulk_emails(const std::vector<SmtpMessage>& messages) {
    SmtpClient client;
    client.connect("smtp.example.com", 587);
    client.authenticate("username", "password");
    
    size_t batch_size = 50;  // Send in batches
    size_t total_sent = 0;
    
    for (size_t i = 0; i < messages.size(); i += batch_size) {
        size_t end = std::min(i + batch_size, messages.size());
        
        for (size_t j = i; j < end; ++j) {
            auto result = client.send_message(messages[j]);
            if (result) {
                ++total_sent;
            } else {
                std::cerr << "Failed to send message " << j << ": " 
                          << result.error().message << std::endl;
            }
            
            // Rate limiting
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        
        std::cout << "Sent batch " << (i / batch_size + 1) 
                  << ", total sent: " << total_sent << std::endl;
    }
    
    client.disconnect();
}
```

#### Memory Management

```cpp
// Efficient message creation for large attachments
SmtpMessage create_large_message() {
    SmtpMessage message;
    message.set_from(EmailAddress("sender@example.com"));
    message.add_to(EmailAddress("recipient@example.com"));
    message.set_subject("Large Attachment");
    message.set_body("See attached file.");
    
    // For very large files, consider streaming instead of loading into memory
    std::ifstream file("large_file.zip", std::ios::binary | std::ios::ate);
    if (file) {
        size_t size = file.tellg();
        
        if (size > 50 * 1024 * 1024) {  // 50MB
            std::cerr << "File too large for memory loading" << std::endl;
            return message;
        }
        
        file.seekg(0, std::ios::beg);
        
        SmtpMessage::Attachment attachment;
        attachment.filename = "large_file.zip";
        attachment.content_type = "application/zip";
        attachment.data.resize(size);
        
        file.read(reinterpret_cast<char*>(attachment.data.data()), size);
        message.add_attachment(attachment);
    }
    
    return message;
}
```

### Performance Metrics

```cpp
class SmtpMetrics {
private:
    std::atomic<size_t> total_messages_{0};
    std::atomic<size_t> successful_sends_{0};
    std::atomic<size_t> failed_sends_{0};
    std::chrono::steady_clock::time_point start_time_;
    std::atomic<uint64_t> total_bytes_{0};

public:
    SmtpMetrics() : start_time_(std::chrono::steady_clock::now()) {}
    
    void record_send(bool success, size_t message_size) {
        total_messages_.fetch_add(1);
        total_bytes_.fetch_add(message_size);
        
        if (success) {
            successful_sends_.fetch_add(1);
        } else {
            failed_sends_.fetch_add(1);
        }
    }
    
    void print_stats() {
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - start_time_);
        
        std::cout << "SMTP Performance Stats:" << std::endl;
        std::cout << "  Total messages: " << total_messages_.load() << std::endl;
        std::cout << "  Successful: " << successful_sends_.load() << std::endl;
        std::cout << "  Failed: " << failed_sends_.load() << std::endl;
        std::cout << "  Success rate: " << (100.0 * successful_sends_.load() / total_messages_.load()) << "%" << std::endl;
        std::cout << "  Total bytes: " << total_bytes_.load() << std::endl;
        std::cout << "  Duration: " << duration.count() << " seconds" << std::endl;
        
        if (duration.count() > 0) {
            std::cout << "  Messages/sec: " << (total_messages_.load() / duration.count()) << std::endl;
            std::cout << "  Bytes/sec: " << (total_bytes_.load() / duration.count()) << std::endl;
        }
    }
};
```

## Security Considerations

### Authentication Security

```cpp
// Secure credential handling
class SecureCredentials {
private:
    std::string username_;
    std::string password_;

public:
    SecureCredentials(std::string_view username, std::string_view password)
        : username_(username), password_(password) {}
    
    ~SecureCredentials() {
        // Clear sensitive data
        std::fill(username_.begin(), username_.end(), 0);
        std::fill(password_.begin(), password_.end(), 0);
    }
    
    std::string_view username() const { return username_; }
    std::string_view password() const { return password_; }
    
    // Prevent copying
    SecureCredentials(const SecureCredentials&) = delete;
    SecureCredentials& operator=(const SecureCredentials&) = delete;
};

// Usage
void secure_send(const SmtpMessage& message) {
    SecureCredentials creds("username", "password");
    
    SmtpClient client;
    client.connect("smtp.example.com", 587);
    
    // Always use TLS when available
    if (client.supports_extension(SmtpExtension::STARTTLS)) {
        auto tls_result = client.start_tls();
        if (!tls_result) {
            std::cerr << "Warning: TLS not available" << std::endl;
        }
    }
    
    client.authenticate(creds.username(), creds.password());
    client.send_message(message);
    client.disconnect();
    
    // Credentials automatically cleared when leaving scope
}
```

### Input Validation

```cpp
class SecureSmtpServer : public SmtpServer {
public:
    SecureSmtpServer(Port port) : SmtpServer(port) {
        setup_security();
    }

private:
    void setup_security() {
        // Rate limiting per IP
        std::unordered_map<std::string, std::chrono::steady_clock::time_point> last_connection_;
        std::mutex rate_limit_mutex_;
        
        set_message_handler([this](const SmtpMessage& msg, const SocketAddress& sender) {
            if (validate_message_security(msg, sender)) {
                process_secure_message(msg, sender);
            }
        });
        
        set_recipient_validator([](const EmailAddress& email) {
            // Validate recipient against whitelist
            return validate_recipient_whitelist(email);
        });
        
        set_auth_validator([](std::string_view username, std::string_view password) {
            // Rate limit authentication attempts
            return secure_authenticate(username, password);
        });
    }
    
    bool validate_message_security(const SmtpMessage& message, const SocketAddress& sender) {
        // Check message size
        if (message.estimated_size() > 25 * 1024 * 1024) {  // 25MB limit
            std::cerr << "Message too large from " << sender.to_string() << std::endl;
            return false;
        }
        
        // Validate sender
        if (!message.from().is_valid()) {
            std::cerr << "Invalid sender from " << sender.to_string() << std::endl;
            return false;
        }
        
        // Check for suspicious content
        const std::string& body = message.body();
        const std::string& subject = message.subject();
        
        // Simple spam detection
        if (subject.find("URGENT!!!") != std::string::npos ||
            body.find("Click here now") != std::string::npos) {
            std::cerr << "Suspicious content detected" << std::endl;
            return false;
        }
        
        return true;
    }
    
    static bool validate_recipient_whitelist(const EmailAddress& email) {
        // Only allow specific domains
        std::vector<std::string> allowed_domains = {
            "@localhost",
            "@example.com",
            "@networkquests.edu"
        };
        
        for (const auto& domain : allowed_domains) {
            if (email.address().find(domain) != std::string::npos) {
                return true;
            }
        }
        
        return false;
    }
    
    static bool secure_authenticate(std::string_view username, std::string_view password) {
        // Implement secure password checking with:
        // - Salted hashing
        // - Rate limiting
        // - Account lockout
        
        // Simplified for demo
        return username == "admin" && password.length() >= 8;
    }
    
    void process_secure_message(const SmtpMessage& message, const SocketAddress& sender) {
        // Log security event
        std::cout << "Secure message processed:" << std::endl;
        std::cout << "  Sender IP: " << sender.to_string() << std::endl;
        std::cout << "  From: " << message.from().to_string() << std::endl;
        std::cout << "  Size: " << message.estimated_size() << " bytes" << std::endl;
        
        // Save with security metadata
        // ... implementation ...
    }
};
```

This comprehensive documentation provides everything needed to understand and use the SMTP protocol implementation effectively, from basic concepts to advanced security considerations.