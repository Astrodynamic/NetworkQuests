# SNMP (Simple Network Management Protocol) Guide

## Table of Contents
1. [Overview](#overview)
2. [Protocol Theory](#protocol-theory)
3. [Implementation Architecture](#implementation-architecture)
4. [API Reference](#api-reference)
5. [Basic Usage](#basic-usage)
6. [Advanced Features](#advanced-features)
7. [Examples](#examples)
8. [Best Practices](#best-practices)
9. [Troubleshooting](#troubleshooting)
10. [Performance Optimization](#performance-optimization)
11. [Security Considerations](#security-considerations)

## Overview

The Simple Network Management Protocol (SNMP) is a widely used protocol for network management and monitoring. It enables network administrators to monitor network performance, detect network faults, and provision new devices remotely.

### Key Features

- **Multiple SNMP Versions**: Support for SNMPv1, SNMPv2c
- **Complete SNMP Operations**: GET, SET, GETNEXT, GETBULK, TRAP, INFORM
- **ASN.1 BER Encoding**: Full compliance with ASN.1 Basic Encoding Rules
- **MIB Management**: Comprehensive Management Information Base support
- **Agent and Manager**: Both client (manager) and server (agent) functionality
- **Object Identifier (OID) Support**: Complete OID manipulation and validation
- **Community-based Security**: Read/write community string authentication
- **Educational Focus**: Comprehensive examples and documentation

### Protocol Versions Supported

- **SNMPv1 (RFC 1157)**: Basic SNMP operations with community-based security
- **SNMPv2c (RFC 1901)**: Enhanced operations including GETBULK and improved error handling

## Protocol Theory

### SNMP Architecture

SNMP follows a client-server model:

- **SNMP Manager (Client)**: Sends requests to agents and processes responses
- **SNMP Agent (Server)**: Responds to requests and sends traps/notifications
- **Management Information Base (MIB)**: Database of managed objects

### SNMP Operations

#### Basic Operations

1. **GET**: Retrieve the value of one or more variables
2. **GETNEXT**: Retrieve the next variable in the MIB tree
3. **SET**: Modify the value of one or more variables
4. **GETBULK**: Efficiently retrieve multiple variables (SNMPv2c only)

#### Notification Operations

5. **TRAP**: Asynchronous notification from agent to manager
6. **INFORM**: Acknowledged trap (SNMPv2c only)

### Message Format

SNMP messages consist of:
- **Version**: SNMP version number
- **Community**: Authentication string
- **PDU (Protocol Data Unit)**: Contains the actual operation and data

### Object Identifiers (OIDs)

OIDs uniquely identify managed objects in a hierarchical tree structure:
```
1.3.6.1.2.1.1.1.0 (system description)
├── 1 (iso)
├── 3 (identified-organization)
├── 6 (dod)
├── 1 (internet)
├── 2 (mgmt)
├── 1 (mib-2)
├── 1 (system)
├── 1 (sysDescr)
└── 0 (instance)
```

### Data Types

SNMP supports various data types:
- **INTEGER**: 32-bit signed integer
- **OCTET STRING**: Byte string
- **OBJECT IDENTIFIER**: OID value
- **NULL**: Null value
- **COUNTER**: 32-bit counter (wraps at 2^32)
- **GAUGE**: 32-bit gauge value
- **TIME TICKS**: Time value in centiseconds
- **IP ADDRESS**: IPv4 address

## Implementation Architecture

### Core Components

#### 1. ObjectIdentifier Class
```cpp
class ObjectIdentifier {
public:
    ObjectIdentifier();
    ObjectIdentifier(std::string_view oid_string);
    ObjectIdentifier(const std::vector<uint32_t>& components);
    
    void append(uint32_t component);
    void prepend(uint32_t component);
    std::string to_string() const;
    bool is_child_of(const ObjectIdentifier& parent) const;
    
    static Result<ObjectIdentifier> from_string(std::string_view oid_str);
};
```

#### 2. SnmpManager Class (Client)
```cpp
class SnmpManager {
public:
    SnmpManager();
    explicit SnmpManager(std::chrono::milliseconds timeout);
    
    Result<void> connect(const SocketAddress& agent_addr);
    void disconnect();
    bool is_connected() const;
    
    // Basic operations
    Result<SnmpValue> get(const ObjectIdentifier& oid, std::string_view community);
    Result<std::vector<VariableBinding>> get_multiple(const std::vector<ObjectIdentifier>& oids, 
                                                     std::string_view community);
    Result<VariableBinding> get_next(const ObjectIdentifier& oid, std::string_view community);
    Result<void> set(const ObjectIdentifier& oid, const SnmpValue& value, 
                    DataType type, std::string_view community);
    
    // Advanced operations
    Result<std::vector<VariableBinding>> get_bulk(const std::vector<ObjectIdentifier>& oids,
                                                 int32_t non_repeaters, int32_t max_repetitions,
                                                 std::string_view community);
    Result<void> inform(const ObjectIdentifier& oid, const SnmpValue& value, 
                       DataType type, std::string_view community);
    Result<std::vector<VariableBinding>> walk_table(const ObjectIdentifier& table_oid,
                                                   std::string_view community);
    
    // Configuration
    void set_timeout(std::chrono::milliseconds timeout);
    void set_retries(int retries);
    void set_version(SnmpVersion version);
};
```

#### 3. SnmpAgent Class (Server)
```cpp
class SnmpAgent {
public:
    explicit SnmpAgent(Port port);
    explicit SnmpAgent(const SocketAddress& bind_addr);
    
    Result<void> start();
    void stop();
    bool is_running() const;
    
    // MIB management
    void add_mib_object(std::unique_ptr<MibObject> object);
    void add_static_object(const ObjectIdentifier& oid, const SnmpValue& value, 
                          DataType type, bool writable = false);
    void remove_mib_object(const ObjectIdentifier& oid);
    std::vector<ObjectIdentifier> get_mib_oids() const;
    
    // Community management
    void add_read_community(std::string_view community);
    void add_write_community(std::string_view community);
    void remove_community(std::string_view community);
    
    // Trap sending
    Result<void> send_trap(const SocketAddress& manager_addr, const ObjectIdentifier& trap_oid,
                          const std::vector<VariableBinding>& variables, std::string_view community);
    Result<void> send_v1_trap(const SocketAddress& manager_addr, const ObjectIdentifier& enterprise,
                             GenericTrap generic_trap, int32_t specific_trap,
                             const std::vector<VariableBinding>& variables, std::string_view community);
    
    // Configuration
    void set_system_description(std::string_view description);
    void set_system_contact(std::string_view contact);
    void set_system_name(std::string_view name);
    void set_system_location(std::string_view location);
    void set_trap_handler(TrapHandler handler);
};
```

#### 4. MibObject Class
```cpp
class MibObject {
public:
    MibObject(const ObjectIdentifier& oid, DataType type, bool writable = false);
    
    const ObjectIdentifier& oid() const;
    DataType type() const;
    bool is_writable() const;
    
    // Value access
    Result<SnmpValue> get_value() const;
    Result<void> set_value(const SnmpValue& value);
    
    // Static value management
    void set_static_value(const SnmpValue& value);
    
    // Dynamic value handlers
    void set_get_handler(GetHandler handler);
    void set_set_handler(SetHandler handler);
};
```

### Encoding and Decoding

The implementation uses ASN.1 BER (Basic Encoding Rules) for message encoding:

#### Key Encoding Functions
```cpp
namespace snmp_utils {
    std::vector<uint8_t> encode_length(size_t length);
    Result<size_t> decode_length(const std::vector<uint8_t>& data, size_t& offset);
    
    std::vector<uint8_t> encode_integer(int32_t value);
    Result<int32_t> decode_integer(const std::vector<uint8_t>& data, size_t& offset);
    
    std::vector<uint8_t> encode_string(std::string_view str);
    Result<std::string> decode_string(const std::vector<uint8_t>& data, size_t& offset);
    
    std::vector<uint8_t> encode_oid(const ObjectIdentifier& oid);
    Result<ObjectIdentifier> decode_oid(const std::vector<uint8_t>& data, size_t& offset);
}
```

## API Reference

### Basic SNMP Operations

#### GET Operation
```cpp
#include "networkquests/snmp.hpp"

using namespace networkquests::snmp;

SnmpManager manager;
auto result = manager.connect(SocketAddress::from_string("192.168.1.1", 161));
if (result) {
    auto value_result = manager.get(ObjectIdentifier("1.3.6.1.2.1.1.1.0"), "public");
    if (value_result) {
        std::cout << "System description: " 
                  << snmp_utils::snmp_value_to_string(value_result.value()) << std::endl;
    }
}
```

#### SET Operation
```cpp
auto set_result = manager.set(
    ObjectIdentifier("1.3.6.1.2.1.1.4.0"),  // sysContact
    SnmpValue(std::string("admin@company.com")),
    DataType::OCTET_STRING,
    "private"
);
if (set_result) {
    std::cout << "System contact updated successfully" << std::endl;
}
```

#### GETNEXT Operation
```cpp
auto next_result = manager.get_next(ObjectIdentifier("1.3.6.1.2.1.1"), "public");
if (next_result) {
    const auto& vb = next_result.value();
    std::cout << "Next OID: " << vb.oid().to_string() << std::endl;
    std::cout << "Value: " << snmp_utils::snmp_value_to_string(vb.value()) << std::endl;
}
```

#### GETBULK Operation (SNMPv2c)
```cpp
manager.set_version(SnmpVersion::V2C);
std::vector<ObjectIdentifier> oids = {ObjectIdentifier("1.3.6.1.2.1.1")};
auto bulk_result = manager.get_bulk(oids, 0, 10, "public");
if (bulk_result) {
    for (const auto& vb : bulk_result.value()) {
        std::cout << vb.oid().to_string() << " = " 
                  << snmp_utils::snmp_value_to_string(vb.value()) << std::endl;
    }
}
```

### Agent Operations

#### Creating and Starting an Agent
```cpp
SnmpAgent agent(161);  // Bind to port 161

// Configure system information
agent.set_system_description("My SNMP Agent");
agent.set_system_contact("admin@company.com");
agent.set_system_name("server01");
agent.set_system_location("Data Center A");

// Add custom MIB objects
ObjectIdentifier custom_oid("1.3.6.1.4.1.12345.1.1.0");
agent.add_static_object(custom_oid, std::string("Custom Value"), DataType::OCTET_STRING, true);

// Start the agent
auto result = agent.start();
if (result) {
    std::cout << "Agent started on " << agent.local_address().to_string() << std::endl;
}
```

#### Dynamic MIB Objects
```cpp
ObjectIdentifier cpu_usage_oid("1.3.6.1.4.1.12345.1.2.0");
auto cpu_obj = std::make_unique<MibObject>(cpu_usage_oid, DataType::GAUGE, false);

cpu_obj->set_get_handler([]() -> Result<SnmpValue> {
    // Simulate CPU usage calculation
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<uint32_t> dis(0, 100);
    return SnmpValue(dis(gen));
});

agent.add_mib_object(std::move(cpu_obj));
```

#### Sending Traps
```cpp
// SNMPv2 Trap
ObjectIdentifier trap_oid("1.3.6.1.4.1.12345.0.1");
std::vector<VariableBinding> variables;

VariableBinding vb;
vb.set_oid(ObjectIdentifier("1.3.6.1.4.1.12345.1.1.0"));
vb.set_value(std::string("Threshold exceeded"));
vb.set_type(DataType::OCTET_STRING);
variables.push_back(vb);

SocketAddress manager_addr = SocketAddress::from_string("192.168.1.100", 162);
auto trap_result = agent.send_trap(manager_addr, trap_oid, variables, "public");

// SNMPv1 Trap
ObjectIdentifier enterprise("1.3.6.1.4.1.12345");
auto v1_trap_result = agent.send_v1_trap(
    manager_addr, enterprise, GenericTrap::ENTERPRISE_SPECIFIC, 1, variables, "public"
);
```

### Working with Object Identifiers

#### Creating OIDs
```cpp
// From string
auto oid1 = ObjectIdentifier::from_string("1.3.6.1.2.1.1.1.0");

// From components
ObjectIdentifier oid2({1, 3, 6, 1, 2, 1, 1, 1, 0});

// Building incrementally
ObjectIdentifier base("1.3.6.1.4.1.12345");
base.append(1);      // 1.3.6.1.4.1.12345.1
base.append(1);      // 1.3.6.1.4.1.12345.1.1
base.append(0);      // 1.3.6.1.4.1.12345.1.1.0
```

#### OID Operations
```cpp
ObjectIdentifier parent("1.3.6.1.2.1.1");
ObjectIdentifier child("1.3.6.1.2.1.1.1.0");

bool is_child = child.is_child_of(parent);  // true
std::string oid_str = child.to_string();     // "1.3.6.1.2.1.1.1.0"
auto components = child.components();        // {1, 3, 6, 1, 2, 1, 1, 1, 0}
```

## Basic Usage

### Simple SNMP Manager
```cpp
#include "networkquests/snmp.hpp"
#include <iostream>

using namespace networkquests::snmp;

int main() {
    SnmpManager manager;
    
    // Connect to agent
    auto connect_result = manager.connect(SocketAddress::from_string("127.0.0.1", 161));
    if (!connect_result) {
        std::cerr << "Failed to connect: " << connect_result.error() << std::endl;
        return 1;
    }
    
    // Get system description
    auto get_result = manager.get(ObjectIdentifier("1.3.6.1.2.1.1.1.0"), "public");
    if (get_result) {
        std::cout << "System Description: " 
                  << snmp_utils::snmp_value_to_string(get_result.value()) << std::endl;
    } else {
        std::cerr << "GET failed: " << get_result.error() << std::endl;
    }
    
    return 0;
}
```

### Simple SNMP Agent
```cpp
#include "networkquests/snmp.hpp"
#include <iostream>
#include <thread>

using namespace networkquests::snmp;

int main() {
    SnmpAgent agent(161);
    
    // Configure agent
    agent.set_system_description("Simple SNMP Agent Example");
    agent.set_system_contact("admin@example.com");
    
    // Add custom object
    ObjectIdentifier custom_oid("1.3.6.1.4.1.12345.1.1.0");
    agent.add_static_object(custom_oid, std::string("Hello SNMP!"), DataType::OCTET_STRING);
    
    // Start agent
    auto start_result = agent.start();
    if (!start_result) {
        std::cerr << "Failed to start agent: " << start_result.error() << std::endl;
        return 1;
    }
    
    std::cout << "Agent running on " << agent.local_address().to_string() << std::endl;
    std::cout << "Press Enter to stop..." << std::endl;
    
    std::cin.get();
    
    agent.stop();
    return 0;
}
```

## Advanced Features

### Table Walking
```cpp
// Walk entire system table
ObjectIdentifier system_table("1.3.6.1.2.1.1");
auto walk_result = manager.walk_table(system_table, "public");

if (walk_result) {
    std::cout << "System Table Contents:" << std::endl;
    for (const auto& vb : walk_result.value()) {
        std::cout << vb.oid().to_string() << " = " 
                  << snmp_utils::snmp_value_to_string(vb.value()) << std::endl;
    }
}
```

### Bulk Operations
```cpp
// Efficient bulk retrieval
std::vector<ObjectIdentifier> oids = {
    ObjectIdentifier("1.3.6.1.2.1.1.1"),   // sysDescr
    ObjectIdentifier("1.3.6.1.2.1.1.2"),   // sysObjectID
    ObjectIdentifier("1.3.6.1.2.1.1.3")    // sysUpTime
};

auto bulk_result = manager.get_bulk(oids, 0, 5, "public");
if (bulk_result) {
    for (const auto& vb : bulk_result.value()) {
        std::cout << vb.oid().to_string() << " = " 
                  << snmp_utils::snmp_value_to_string(vb.value()) << std::endl;
    }
}
```

### Dynamic MIB Objects with Callbacks
```cpp
// Counter that increments on each access
ObjectIdentifier counter_oid("1.3.6.1.4.1.12345.1.3.0");
auto counter_obj = std::make_unique<MibObject>(counter_oid, DataType::COUNTER, false);

counter_obj->set_get_handler([]() -> Result<SnmpValue> {
    static uint32_t counter = 0;
    return SnmpValue(++counter);
});

agent.add_mib_object(std::move(counter_obj));

// Writable string object
ObjectIdentifier config_oid("1.3.6.1.4.1.12345.1.4.0");
auto config_obj = std::make_unique<MibObject>(config_oid, DataType::OCTET_STRING, true);

static std::string config_value = "default";
config_obj->set_get_handler([]() -> Result<SnmpValue> {
    return SnmpValue(config_value);
});

config_obj->set_set_handler([](const SnmpValue& value) -> Result<void> {
    if (std::holds_alternative<std::string>(value)) {
        config_value = std::get<std::string>(value);
        return make_success();
    }
    return make_error("Invalid value type");
});

agent.add_mib_object(std::move(config_obj));
```

### Trap Handling
```cpp
// Set up trap handler on agent
agent.set_trap_handler([](const SnmpMessage& trap, const SocketAddress& sender) {
    std::cout << "Received trap from " << sender.to_string() << std::endl;
    std::cout << "Community: " << trap.community() << std::endl;
    
    const auto* pdu = trap.pdu();
    if (pdu) {
        std::cout << "PDU Type: " << snmp_utils::pdu_type_to_string(pdu->type()) << std::endl;
        
        for (const auto& vb : pdu->variable_bindings()) {
            std::cout << "  " << vb.oid().to_string() << " = " 
                      << snmp_utils::snmp_value_to_string(vb.value()) << std::endl;
        }
    }
});
```

### Multiple Operations
```cpp
// Perform multiple GET operations efficiently
std::vector<ObjectIdentifier> system_oids = {
    ObjectIdentifier("1.3.6.1.2.1.1.1.0"),  // sysDescr
    ObjectIdentifier("1.3.6.1.2.1.1.3.0"),  // sysUpTime
    ObjectIdentifier("1.3.6.1.2.1.1.5.0"),  // sysName
    ObjectIdentifier("1.3.6.1.2.1.1.6.0")   // sysLocation
};

auto multi_result = manager.get_multiple(system_oids, "public");
if (multi_result) {
    const auto& results = multi_result.value();
    std::vector<std::string> names = {"Description", "Uptime", "Name", "Location"};
    
    for (size_t i = 0; i < results.size() && i < names.size(); ++i) {
        std::cout << "System " << names[i] << ": " 
                  << snmp_utils::snmp_value_to_string(results[i].value()) << std::endl;
    }
}
```

## Examples

### Complete Network Monitor
```cpp
#include "networkquests/snmp.hpp"
#include <iostream>
#include <thread>
#include <chrono>

class NetworkMonitor {
public:
    NetworkMonitor() : manager_(std::chrono::milliseconds(3000)) {
        manager_.set_version(SnmpVersion::V2C);
    }
    
    void monitor_device(const std::string& host, const std::string& community) {
        auto addr = SocketAddress::from_string(host, 161);
        auto connect_result = manager_.connect(addr);
        
        if (!connect_result) {
            std::cerr << "Failed to connect to " << host << std::endl;
            return;
        }
        
        std::cout << "Monitoring " << host << "..." << std::endl;
        
        while (true) {
            monitor_system_info(community);
            monitor_interface_stats(community);
            
            std::this_thread::sleep_for(std::chrono::seconds(30));
        }
    }

private:
    SnmpManager manager_;
    
    void monitor_system_info(const std::string& community) {
        auto uptime_result = manager_.get(ObjectIdentifier("1.3.6.1.2.1.1.3.0"), community);
        if (uptime_result) {
            uint32_t uptime = std::get<uint32_t>(uptime_result.value());
            std::cout << "System uptime: " << (uptime / 100) << " seconds" << std::endl;
        }
    }
    
    void monitor_interface_stats(const std::string& community) {
        // Walk interface table
        ObjectIdentifier if_table("1.3.6.1.2.1.2.2.1");
        auto walk_result = manager_.walk_table(if_table, community);
        
        if (walk_result) {
            std::cout << "Interface statistics updated" << std::endl;
            // Process interface data...
        }
    }
};

int main() {
    NetworkMonitor monitor;
    monitor.monitor_device("192.168.1.1", "public");
    return 0;
}
```

### SNMP Data Collector
```cpp
class SnmpDataCollector {
public:
    struct DeviceInfo {
        std::string host;
        std::string community;
        std::vector<ObjectIdentifier> oids;
    };
    
    void add_device(const DeviceInfo& info) {
        devices_.push_back(info);
    }
    
    void collect_all() {
        for (const auto& device : devices_) {
            collect_device_data(device);
        }
    }

private:
    std::vector<DeviceInfo> devices_;
    
    void collect_device_data(const DeviceInfo& device) {
        SnmpManager manager;
        auto connect_result = manager.connect(SocketAddress::from_string(device.host, 161));
        
        if (!connect_result) {
            std::cerr << "Failed to connect to " << device.host << std::endl;
            return;
        }
        
        auto results = manager.get_multiple(device.oids, device.community);
        if (results) {
            std::cout << "Data from " << device.host << ":" << std::endl;
            for (size_t i = 0; i < results.value().size(); ++i) {
                const auto& vb = results.value()[i];
                std::cout << "  " << vb.oid().to_string() << " = " 
                          << snmp_utils::snmp_value_to_string(vb.value()) << std::endl;
            }
        }
    }
};
```

## Best Practices

### Performance Optimization

1. **Use GETBULK for Multiple Values**
```cpp
// Instead of multiple GET operations:
// for (const auto& oid : oids) {
//     manager.get(oid, community);
// }

// Use GETBULK:
auto results = manager.get_bulk(oids, 0, oids.size(), community);
```

2. **Reuse Connections**
```cpp
class SnmpSession {
public:
    SnmpSession(const SocketAddress& addr) {
        manager_.connect(addr);
    }
    
    // Reuse connection for multiple operations
    Result<SnmpValue> get(const ObjectIdentifier& oid, std::string_view community) {
        return manager_.get(oid, community);
    }

private:
    SnmpManager manager_;
};
```

3. **Optimize MIB Access**
```cpp
// Use efficient handlers for dynamic objects
auto cpu_obj = std::make_unique<MibObject>(cpu_oid, DataType::GAUGE, false);
cpu_obj->set_get_handler([]() -> Result<SnmpValue> {
    static auto last_update = std::chrono::steady_clock::now();
    static uint32_t cached_value = 0;
    
    auto now = std::chrono::steady_clock::now();
    if (now - last_update > std::chrono::seconds(5)) {
        cached_value = get_actual_cpu_usage();  // Expensive operation
        last_update = now;
    }
    
    return SnmpValue(cached_value);
});
```

### Error Handling

1. **Always Check Results**
```cpp
auto result = manager.get(oid, community);
if (!result) {
    std::cerr << "SNMP GET failed: " << result.error() << std::endl;
    // Handle error appropriately
    return;
}

// Use the value
auto value = result.value();
```

2. **Handle Network Timeouts**
```cpp
manager.set_timeout(std::chrono::milliseconds(10000));  // 10 second timeout
manager.set_retries(3);  // Retry 3 times

auto result = manager.get(oid, community);
if (!result) {
    if (result.error().find("timeout") != std::string::npos) {
        // Handle timeout specifically
        std::cerr << "Network timeout - device may be unreachable" << std::endl;
    } else {
        // Handle other errors
        std::cerr << "SNMP error: " << result.error() << std::endl;
    }
}
```

3. **Validate OIDs**
```cpp
auto oid_result = ObjectIdentifier::from_string(oid_string);
if (!oid_result) {
    std::cerr << "Invalid OID format: " << oid_result.error() << std::endl;
    return;
}

auto oid = oid_result.value();
```

### Security Best Practices

1. **Use Strong Community Strings**
```cpp
// Avoid default communities in production
agent.remove_community("public");
agent.remove_community("private");

// Use strong, unique community strings
agent.add_read_community("MySecureReadCommunity123");
agent.add_write_community("MySecureWriteCommunity456");
```

2. **Limit Access by IP**
```cpp
// Implement IP-based access control in custom handlers
agent.set_trap_handler([](const SnmpMessage& trap, const SocketAddress& sender) {
    // Check if sender is authorized
    if (!is_authorized_manager(sender)) {
        std::cerr << "Unauthorized trap from " << sender.to_string() << std::endl;
        return;
    }
    
    // Process authorized trap
    handle_trap(trap, sender);
});
```

3. **Validate Input Data**
```cpp
auto config_obj = std::make_unique<MibObject>(config_oid, DataType::OCTET_STRING, true);
config_obj->set_set_handler([](const SnmpValue& value) -> Result<void> {
    if (!std::holds_alternative<std::string>(value)) {
        return make_error("Invalid data type");
    }
    
    const auto& str_value = std::get<std::string>(value);
    if (str_value.length() > 255) {
        return make_error("String too long");
    }
    
    if (!is_valid_config(str_value)) {
        return make_error("Invalid configuration value");
    }
    
    apply_config(str_value);
    return make_success();
});
```

### Monitoring and Logging

1. **Security Event Logging**
```cpp
agent.set_trap_handler([](const SnmpMessage& message, const SocketAddress& sender) {
    // Log all incoming traps for security analysis
    LOG_INFO("SNMP_SECURITY", "Trap received from " + sender.to_string() + 
             " with community '" + message.community() + "'");
    
    // Detect suspicious activity
    if (is_suspicious_activity(message, sender)) {
        LOG_WARNING("SNMP_SECURITY", "Suspicious SNMP activity detected from " + 
                   sender.to_string());
        
        // Optional: Send security alert
        send_security_alert(message, sender);
    }
});
```

2. **Rate Limiting**
```cpp
class RateLimitedSnmpAgent : public SnmpAgent {
public:
    RateLimitedSnmpAgent(Port port, size_t max_requests_per_minute = 100)
        : SnmpAgent(port), max_requests_per_minute_(max_requests_per_minute) {}

protected:
    void handle_request(const std::vector<uint8_t>& data, 
                       const SocketAddress& client_addr) override {
        if (!check_rate_limit(client_addr)) {
            LOG_WARNING("SNMP", "Rate limit exceeded for " + client_addr.to_string());
            return;
        }
        
        SnmpAgent::handle_request(data, client_addr);
    }

private:
    bool check_rate_limit(const SocketAddress& addr) {
        auto now = std::chrono::steady_clock::now();
        auto& timestamps = request_timestamps_[addr.to_string()];
        
        // Remove old timestamps
        timestamps.erase(
            std::remove_if(timestamps.begin(), timestamps.end(),
                [now](const auto& timestamp) {
                    return now - timestamp > std::chrono::minutes(1);
                }),
            timestamps.end()
        );
        
        // Check if under limit
        if (timestamps.size() >= max_requests_per_minute_) {
            return false;
        }
        
        timestamps.push_back(now);
        return true;
    }
    
    size_t max_requests_per_minute_;
    std::unordered_map<std::string, std::vector<std::chrono::steady_clock::time_point>> request_timestamps_;
};
```

### Secure Configuration

1. **Minimal MIB Exposure**
```cpp
// Only expose necessary MIB objects
agent.clear_mib();  // Remove all default objects

// Add only required system objects
agent.add_static_object(ObjectIdentifier("1.3.6.1.2.1.1.1.0"), 
                       std::string("Production Server"), DataType::OCTET_STRING, false);

// Avoid exposing sensitive information
// Don't add objects that reveal system internals unnecessarily
```

2. **Secure Defaults**
```cpp
class SecureSnmpAgent : public SnmpAgent {
public:
    SecureSnmpAgent(Port port) : SnmpAgent(port) {
        // Remove default communities
        remove_community("public");
        remove_community("private");
        
        // Set secure defaults
        set_max_message_size(1024);  // Limit message size
        
        // Configure minimal system information
        set_system_description("Network Device");
        set_system_contact("");
        set_system_name("");
        set_system_location("");
    }
};
```

This comprehensive SNMP implementation provides a solid foundation for network management applications while maintaining security and performance considerations. The modular design allows for easy extension and customization for specific use cases.