# DNS Protocol Implementation

## Table of Contents

1. [Overview](#overview)
2. [DNS Protocol Theory](#dns-protocol-theory)
3. [Implementation Architecture](#implementation-architecture)
4. [API Reference](#api-reference)
5. [Usage Examples](#usage-examples)
6. [Advanced Features](#advanced-features)
7. [Performance Considerations](#performance-considerations)
8. [Security Considerations](#security-considerations)
9. [Troubleshooting](#troubleshooting)
10. [References](#references)

## Overview

The Domain Name System (DNS) is a hierarchical and distributed naming system for computers, services, or other resources connected to the Internet or a private network. It translates human-readable domain names into IP addresses and provides various types of information about domain names.

### Key Features

- **Complete RFC 1035 Implementation**: Full DNS message parsing and generation
- **Multiple Record Types**: A, AAAA, NS, MX, TXT, CNAME, PTR, SOA records
- **Client/Server Architecture**: Both client resolver and authoritative server
- **Transport Support**: UDP and TCP with automatic fallback
- **Caching System**: TTL-aware caching for improved performance
- **Zone Management**: Simplified zone file loading and management
- **Educational Focus**: Comprehensive logging and debugging features

## DNS Protocol Theory

### Protocol Basics

DNS operates on **port 53** using both UDP and TCP protocols:

- **UDP**: Primary transport for queries (512 bytes limit)
- **TCP**: Used for zone transfers and large responses

### DNS Message Structure

Every DNS message follows this format:

```
+---------------------+
|        Header       | 12 bytes
+---------------------+
|       Question      | Variable length
+---------------------+
|        Answer       | Variable length
+---------------------+
|      Authority      | Variable length
+---------------------+
|      Additional     | Variable length
+---------------------+
```

#### Header Format (12 bytes)

```cpp
struct DnsHeader {
    uint16_t id;                    // Query identifier
    DnsFlags flags;                 // Control flags
    uint16_t question_count;        // Number of questions
    uint16_t answer_count;          // Number of answer RRs
    uint16_t authority_count;       // Number of authority RRs
    uint16_t additional_count;      // Number of additional RRs
};
```

#### DNS Flags

| Bit | Field | Description |
|-----|-------|-------------|
| 0 | QR | Query (0) or Response (1) |
| 1-4 | OPCODE | Operation code (0=QUERY, 1=IQUERY, 2=STATUS) |
| 5 | AA | Authoritative Answer |
| 6 | TC | Truncated Message |
| 7 | RD | Recursion Desired |
| 8 | RA | Recursion Available |
| 9-11 | Z | Reserved (must be zero) |
| 12-15 | RCODE | Response Code |

### Resource Record Types

| Type | Value | Description |
|------|-------|-------------|
| A | 1 | IPv4 address |
| NS | 2 | Name server |
| CNAME | 5 | Canonical name alias |
| SOA | 6 | Start of authority |
| PTR | 12 | Pointer record |
| MX | 15 | Mail exchange |
| TXT | 16 | Text record |
| AAAA | 28 | IPv6 address |

### DNS Name Encoding

Domain names use label encoding:

```
example.com → 7example3com0
```

- Each label prefixed with length byte
- Maximum label length: 63 bytes
- Maximum name length: 255 bytes
- Names ending with null byte (0)

## Implementation Architecture

### Core Components

```cpp
namespace NetworkQuests::Dns {
    // Message handling
    class DnsMessage;
    class DnsQuestion;
    class DnsResourceRecord;
    
    // Record types
    class ARecord, AAAARecord;
    class NSRecord, CNAMERecord, PTRRecord;
    class MXRecord, TXTRecord, SOARecord;
    
    // Network components
    class DnsClient;
    class DnsServer;
    class DnsZone;
    
    // Utilities
    namespace Utils { /* ... */ }
}
```

### Class Hierarchy

```
DnsRData (abstract base)
├── ARecord (IPv4 addresses)
├── AAAARecord (IPv6 addresses)  
├── NSRecord (name servers)
├── CNAMERecord (aliases)
├── PTRRecord (reverse lookups)
├── MXRecord (mail servers)
├── TXTRecord (text data)
└── SOARecord (zone authority)
```

## API Reference

### DnsClient

The `DnsClient` class provides a high-level interface for DNS queries.

#### Basic Usage

```cpp
#include "networkquests/dns.hpp"
using namespace NetworkQuests::Dns;

// Create client with default server (8.8.8.8)
DnsClient client;

// Or specify custom server
DnsClient client("1.1.1.1", 53);
```

#### Configuration Methods

```cpp
// Server configuration
void set_server(const std::string& server, uint16_t port = 53);
void set_timeout(std::chrono::milliseconds timeout);
void set_retries(int retries);

// Cache management
void enable_cache(bool enable = true);
void clear_cache();
size_t get_cache_size() const;
```

#### Query Methods

```cpp
// Generic query
Result<DnsMessage> query(const std::string& domain, 
                        DnsType type = DnsType::A, 
                        DnsClass qclass = DnsClass::IN);

// Convenience methods
Result<std::vector<std::string>> resolve_a(const std::string& domain);
Result<std::vector<std::string>> resolve_aaaa(const std::string& domain);
Result<std::vector<std::string>> resolve_ns(const std::string& domain);
Result<std::vector<std::string>> resolve_mx(const std::string& domain);
Result<std::vector<std::string>> resolve_txt(const std::string& domain);
Result<std::string> resolve_ptr(const std::string& ip_address);
```

### DnsServer

The `DnsServer` class implements a basic authoritative DNS server.

#### Basic Setup

```cpp
// Create server on port 5353 (non-privileged)
DnsServer server(5353);

// Start the server
auto result = server.start();
if (!result.has_value()) {
    std::cerr << "Failed to start: " << result.error().message << std::endl;
}
```

#### Zone Management

```cpp
// Create and configure a zone
auto zone = std::make_shared<DnsZone>("example.com.");

// Add records
zone->add_record(DnsResourceRecord(
    "example.com.", DnsType::A, DnsClass::IN, 300,
    std::make_unique<ARecord>("192.168.1.1")
));

// Add zone to server
server.add_zone(zone);
```

### DnsMessage

The `DnsMessage` class represents complete DNS messages.

#### Creating Queries

```cpp
// Create a query message
auto query = DnsMessage::create_query("example.com", DnsType::A);

// Add multiple questions
query.add_question(DnsQuestion("example.com", DnsType::MX));
```

#### Processing Responses

```cpp
auto response = client.query("example.com");
if (response.has_value()) {
    for (const auto& answer : response.value().get_answers()) {
        if (answer.get_type() == DnsType::A) {
            auto a_record = dynamic_cast<const ARecord*>(answer.get_rdata());
            if (a_record) {
                std::cout << "IP: " << a_record->get_address_string() << std::endl;
            }
        }
    }
}
```

## Usage Examples

### Example 1: Simple Domain Resolution

```cpp
#include "networkquests/dns.hpp"
#include <iostream>

int main() {
    using namespace NetworkQuests::Dns;
    
    DnsClient client;
    
    // Resolve IPv4 addresses
    auto result = client.resolve_a("google.com");
    if (result.has_value()) {
        std::cout << "Google.com addresses:\n";
        for (const auto& ip : result.value()) {
            std::cout << "  " << ip << "\n";
        }
    }
    
    return 0;
}
```

### Example 2: Mail Server Discovery

```cpp
#include "networkquests/dns.hpp"
#include <iostream>

int main() {
    using namespace NetworkQuests::Dns;
    
    DnsClient client;
    
    // Get MX records
    auto mx_result = client.resolve_mx("example.com");
    if (mx_result.has_value()) {
        std::cout << "Mail servers for example.com:\n";
        for (const auto& mx : mx_result.value()) {
            std::cout << "  " << mx << "\n";
        }
    }
    
    return 0;
}
```

### Example 3: Reverse DNS Lookup

```cpp
#include "networkquests/dns.hpp"
#include <iostream>

int main() {
    using namespace NetworkQuests::Dns;
    
    DnsClient client;
    
    // Reverse lookup
    auto result = client.resolve_ptr("8.8.8.8");
    if (result.has_value()) {
        std::cout << "8.8.8.8 resolves to: " << result.value() << "\n";
    }
    
    return 0;
}
```

### Example 4: Custom DNS Server

```cpp
#include "networkquests/dns.hpp"
#include <iostream>
#include <memory>

int main() {
    using namespace NetworkQuests::Dns;
    
    // Create server
    DnsServer server(5353);
    
    // Create zone
    auto zone = std::make_shared<DnsZone>("test.local.");
    
    // Add SOA record
    zone->add_record(DnsResourceRecord(
        "test.local.", DnsType::SOA, DnsClass::IN, 3600,
        std::make_unique<SOARecord>("ns1.test.local.", "admin.test.local.",
                                   2024010101, 3600, 1800, 604800, 86400)
    ));
    
    // Add A record
    zone->add_record(DnsResourceRecord(
        "test.local.", DnsType::A, DnsClass::IN, 300,
        std::make_unique<ARecord>("192.168.1.100")
    ));
    
    // Add zone to server
    server.add_zone(zone);
    
    // Start server
    auto result = server.start();
    if (result.has_value()) {
        std::cout << "DNS server running on port 5353\n";
        std::cout << "Test with: dig @127.0.0.1 -p 5353 test.local\n";
        
        // Keep running
        std::string input;
        std::getline(std::cin, input);
    }
    
    return 0;
}
```

## Advanced Features

### Caching System

The DNS client includes an intelligent caching system:

```cpp
DnsClient client;
client.enable_cache(true);  // Enable caching (default)

// First query hits the network
auto result1 = client.resolve_a("example.com");

// Second query uses cache (faster)
auto result2 = client.resolve_a("example.com");

// Cache statistics
std::cout << "Cache entries: " << client.get_cache_size() << std::endl;

// Clear cache
client.clear_cache();
```

### Zone File Loading

Simple zone file format support:

```cpp
auto zone = std::make_shared<DnsZone>("example.com.");

std::string zone_data = R"(
example.com. 300 IN A 192.168.1.1
www.example.com. 300 IN A 192.168.1.2
mail.example.com. 300 IN A 192.168.1.3
example.com. 3600 IN MX 10 mail.example.com.
example.com. 300 IN TXT "v=spf1 mx ~all"
)";

auto result = zone->load_from_string(zone_data);
```

### Custom Record Types

Adding custom record handling:

```cpp
class CustomRecord : public DnsRData {
public:
    CustomRecord(const std::string& data) : data_(data) {}
    
    std::vector<uint8_t> serialize() const override {
        return std::vector<uint8_t>(data_.begin(), data_.end());
    }
    
    std::unique_ptr<DnsRData> clone() const override {
        return std::make_unique<CustomRecord>(data_);
    }
    
    std::string to_string() const override {
        return data_;
    }
    
private:
    std::string data_;
};
```

## Performance Considerations

### Client Performance

1. **Use Caching**: Enable client-side caching for repeated queries
2. **Connection Reuse**: TCP connections are reused when possible
3. **Timeout Tuning**: Adjust timeouts based on network conditions
4. **Parallel Queries**: Use multiple client instances for concurrent queries

```cpp
// Optimized client configuration
DnsClient client("1.1.1.1");
client.set_timeout(std::chrono::milliseconds(2000));  // 2s timeout
client.set_retries(2);  // 2 retries
client.enable_cache(true);  // Enable caching
```

### Server Performance

1. **Zone Pre-loading**: Load zones at startup
2. **Thread Pool**: Use worker threads for concurrent requests
3. **Memory Management**: Efficient record storage
4. **Response Caching**: Cache responses for popular queries

### Memory Usage

- **Client Cache**: Automatically expires old entries
- **Server Zones**: Efficient storage of resource records
- **Message Parsing**: Zero-copy operations where possible

## Security Considerations

### DNS Security Threats

1. **DNS Spoofing**: Validate response IDs and sources
2. **Cache Poisoning**: Implement proper TTL handling
3. **Amplification Attacks**: Rate limiting and query validation
4. **Zone Transfer Security**: Restrict zone transfers

### Implementation Security

```cpp
// Secure client configuration
DnsClient client("9.9.9.9");  // Use secure DNS provider

// Validate responses
auto result = client.query("example.com");
if (result.has_value()) {
    auto response = result.value();
    
    // Check response code
    if (response.get_header().flags.response_code == DnsResponseCode::NO_ERROR) {
        // Process answers
        for (const auto& answer : response.get_answers()) {
            // Validate TTL
            if (answer.get_ttl() > 0 && answer.get_ttl() < 86400) {
                // Process record
            }
        }
    }
}
```

### Server Security

```cpp
DnsServer server(5353);

// Disable recursion for authoritative servers
server.set_recursion_enabled(false);

// Add only trusted zones
auto zone = std::make_shared<DnsZone>("trusted.local.");
server.add_zone(zone);
```

## Troubleshooting

### Common Issues

#### "Query timeout"

**Cause**: Network connectivity or DNS server issues
**Solution**: 
```cpp
client.set_timeout(std::chrono::milliseconds(10000));  // Increase timeout
client.set_server("8.8.8.8");  // Try different server
```

#### "Invalid DNS response"

**Cause**: Malformed response or network corruption
**Solution**: 
```cpp
// Enable debug logging
Logger::set_level(LogLevel::DEBUG);

// Try TCP instead of UDP
client.set_retries(0);  // Force TCP fallback
```

#### "Zone not found"

**Cause**: Zone not properly loaded or configured
**Solution**:
```cpp
// Verify zone configuration
auto zone = std::make_shared<DnsZone>("example.com.");
// Make sure domain ends with dot!

// Check zone records
auto records = zone->find_records("example.com.", DnsType::A, DnsClass::IN);
```

### Debug Logging

Enable comprehensive logging:

```cpp
// Set debug level
Logger::set_level(LogLevel::DEBUG);

// Query with logging
auto result = client.query("example.com");
// Check console output for detailed information
```

### Network Testing

Test DNS functionality:

```bash
# Test with dig
dig @127.0.0.1 -p 5353 example.local

# Test with nslookup
nslookup -port=5353 example.local 127.0.0.1

# Test with host
host -p 5353 example.local 127.0.0.1
```

## References

### RFCs and Standards

- **RFC 1035**: Domain Names - Implementation and Specification (Primary DNS Standard)
- **RFC 1034**: Domain Names - Concepts and Facilities
- **RFC 2181**: Clarifications to the DNS Specification
- **RFC 3596**: DNS Extensions to Support IP Version 6 (AAAA records)
- **RFC 2915**: The Naming Authority Pointer (NAPTR) DNS Resource Record

### Educational Resources

- [DNS and BIND](https://www.oreilly.com/library/view/dns-and-bind/0596100574/) - O'Reilly Book
- [How DNS Works](https://howdns.works/) - Interactive Guide
- [DNS Performance Testing](https://www.dnsperf.com/) - Performance Benchmarks

### Tools and Utilities

- **dig**: Command-line DNS lookup tool
- **nslookup**: Interactive DNS lookup
- **host**: Simple DNS lookup utility
- **Wireshark**: Network protocol analyzer for DNS traffic
- **DNSutils**: Collection of DNS utilities

---

This implementation provides a solid foundation for understanding DNS protocol mechanics while offering practical functionality for real-world applications. The educational focus ensures comprehensive logging and clear error messages to aid in learning and debugging.