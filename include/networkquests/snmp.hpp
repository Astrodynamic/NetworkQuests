#pragma once

#include "networkquests/tcp.hpp"
#include "networkquests/udp.hpp"
#include "networkquests/common.hpp"
#include "networkquests/logger.hpp"

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <chrono>
#include <functional>
#include <memory>
#include <atomic>
#include <thread>
#include <regex>
#include <variant>
#include <cstdint>

namespace networkquests::snmp {

// Forward declarations
class SnmpManager;
class SnmpAgent;
class SnmpConnection;
class SnmpMessage;
class SnmpPdu;
class ObjectIdentifier;
class VariableBinding;
class MibObject;

// SNMP version constants (RFC 3411)
enum class SnmpVersion : int32_t {
    V1 = 0,     // SNMPv1 (RFC 1157)
    V2C = 1,    // SNMPv2c (RFC 1901)
    V3 = 3      // SNMPv3 (RFC 3411)
};

// SNMP PDU types (RFC 1157)
enum class PduType : uint8_t {
    GET_REQUEST = 0xA0,         // GetRequest-PDU [0]
    GET_NEXT_REQUEST = 0xA1,    // GetNextRequest-PDU [1]
    GET_RESPONSE = 0xA2,        // GetResponse-PDU [2]
    SET_REQUEST = 0xA3,         // SetRequest-PDU [3]
    TRAP_V1 = 0xA4,            // Trap-PDU [4] (SNMPv1)
    GET_BULK_REQUEST = 0xA5,    // GetBulkRequest-PDU [5] (SNMPv2c)
    INFORM_REQUEST = 0xA6,      // InformRequest-PDU [6] (SNMPv2c)
    TRAP_V2 = 0xA7,            // SNMPv2-Trap-PDU [7] (SNMPv2c)
    REPORT = 0xA8              // Report-PDU [8] (SNMPv3)
};

// SNMP error status codes (RFC 1157)
enum class ErrorStatus : int32_t {
    NO_ERROR = 0,
    TOO_BIG = 1,
    NO_SUCH_NAME = 2,
    BAD_VALUE = 3,
    READ_ONLY = 4,
    GEN_ERR = 5,
    NO_ACCESS = 6,            // SNMPv2c
    WRONG_TYPE = 7,           // SNMPv2c
    WRONG_LENGTH = 8,         // SNMPv2c
    WRONG_ENCODING = 9,       // SNMPv2c
    WRONG_VALUE = 10,         // SNMPv2c
    NO_CREATION = 11,         // SNMPv2c
    INCONSISTENT_VALUE = 12,  // SNMPv2c
    RESOURCE_UNAVAILABLE = 13, // SNMPv2c
    COMMIT_FAILED = 14,       // SNMPv2c
    UNDO_FAILED = 15,         // SNMPv2c
    AUTHORIZATION_ERROR = 16, // SNMPv2c
    NOT_WRITABLE = 17,        // SNMPv2c
    INCONSISTENT_NAME = 18    // SNMPv2c
};

// Generic trap types (RFC 1157)
enum class GenericTrap : int32_t {
    COLD_START = 0,
    WARM_START = 1,
    LINK_DOWN = 2,
    LINK_UP = 3,
    AUTHENTICATION_FAILURE = 4,
    EGP_NEIGHBOR_LOSS = 5,
    ENTERPRISE_SPECIFIC = 6
};

// ASN.1 data types for SNMP (RFC 1155)
enum class DataType : uint8_t {
    // Universal types
    INTEGER = 0x02,
    OCTET_STRING = 0x04,
    NULL_VALUE = 0x05,
    OBJECT_IDENTIFIER = 0x06,
    SEQUENCE = 0x30,
    
    // Application-specific types (RFC 1155)
    IP_ADDRESS = 0x40,        // [APPLICATION 0] IMPLICIT OCTET STRING (SIZE (4))
    COUNTER32 = 0x41,         // [APPLICATION 1] IMPLICIT INTEGER (0..4294967295)
    GAUGE32 = 0x42,           // [APPLICATION 2] IMPLICIT INTEGER (0..4294967295)
    TIME_TICKS = 0x43,        // [APPLICATION 3] IMPLICIT INTEGER (0..4294967295)
    OPAQUE = 0x44,            // [APPLICATION 4] IMPLICIT OCTET STRING
    NSAP_ADDRESS = 0x45,      // [APPLICATION 5] IMPLICIT OCTET STRING
    COUNTER64 = 0x46,         // [APPLICATION 6] IMPLICIT INTEGER (0..18446744073709551615)
    UNSIGNED32 = 0x47,        // [APPLICATION 7] IMPLICIT INTEGER (0..4294967295)
    
    // Exception values (SNMPv2c)
    NO_SUCH_OBJECT = 0x80,    // [0] IMPLICIT NULL
    NO_SUCH_INSTANCE = 0x81,  // [1] IMPLICIT NULL
    END_OF_MIB_VIEW = 0x82    // [2] IMPLICIT NULL
};

// SNMP security models (SNMPv3)
enum class SecurityModel : int32_t {
    ANY = 0,
    SNMP_V1 = 1,
    SNMP_V2C = 2,
    USM = 3  // User-based Security Model
};

// SNMP security levels (SNMPv3)
enum class SecurityLevel : int32_t {
    NO_AUTH_NO_PRIV = 1,    // noAuthNoPriv
    AUTH_NO_PRIV = 2,       // authNoPriv
    AUTH_PRIV = 3           // authPriv
};

// Authentication protocols (SNMPv3 USM)
enum class AuthProtocol {
    NONE,
    MD5,
    SHA1,
    SHA224,
    SHA256,
    SHA384,
    SHA512
};

// Privacy protocols (SNMPv3 USM)
enum class PrivProtocol {
    NONE,
    DES,
    AES128,
    AES192,
    AES256
};

// SNMP value type variant
using SnmpValue = std::variant<
    int32_t,                    // INTEGER
    uint32_t,                   // COUNTER32, GAUGE32, TIME_TICKS, UNSIGNED32
    uint64_t,                   // COUNTER64
    std::string,                // OCTET_STRING
    std::vector<uint8_t>,       // OPAQUE, IP_ADDRESS
    ObjectIdentifier,           // OBJECT_IDENTIFIER
    std::monostate              // NULL_VALUE
>;

// Object Identifier class
class ObjectIdentifier {
public:
    ObjectIdentifier();
    explicit ObjectIdentifier(std::string_view oid_string);
    explicit ObjectIdentifier(const std::vector<uint32_t>& components);
    
    // Component access
    void append(uint32_t component);
    void prepend(uint32_t component);
    std::vector<uint32_t> components() const;
    size_t length() const;
    uint32_t operator[](size_t index) const;
    
    // String conversion
    std::string to_string() const;
    static Result<ObjectIdentifier> from_string(std::string_view oid_str);
    
    // Comparison operators
    bool operator==(const ObjectIdentifier& other) const;
    bool operator<(const ObjectIdentifier& other) const;
    bool operator<=(const ObjectIdentifier& other) const;
    bool operator>(const ObjectIdentifier& other) const;
    bool operator>=(const ObjectIdentifier& other) const;
    
    // Hierarchy operations
    bool is_child_of(const ObjectIdentifier& parent) const;
    bool is_parent_of(const ObjectIdentifier& child) const;
    ObjectIdentifier get_parent() const;
    ObjectIdentifier get_next() const;
    
    // Validation
    bool is_valid() const;
    
private:
    std::vector<uint32_t> components_;
};

// Variable binding (name-value pair)
class VariableBinding {
public:
    VariableBinding();
    VariableBinding(const ObjectIdentifier& oid, const SnmpValue& value, DataType type);
    
    // Accessors
    const ObjectIdentifier& oid() const;
    const SnmpValue& value() const;
    DataType type() const;
    
    void set_oid(const ObjectIdentifier& oid);
    void set_value(const SnmpValue& value, DataType type);
    void set_exception(DataType exception_type);
    
    // Validation
    bool is_valid() const;
    bool is_exception() const;
    
    // Serialization
    std::vector<uint8_t> encode() const;
    static Result<VariableBinding> decode(const std::vector<uint8_t>& data, size_t& offset);
    
private:
    ObjectIdentifier oid_;
    SnmpValue value_;
    DataType type_;
};

// SNMP PDU base class
class SnmpPdu {
public:
    SnmpPdu(PduType type);
    virtual ~SnmpPdu() = default;
    
    // Basic properties
    PduType type() const;
    int32_t request_id() const;
    ErrorStatus error_status() const;
    int32_t error_index() const;
    
    void set_request_id(int32_t request_id);
    void set_error_status(ErrorStatus status);
    void set_error_index(int32_t index);
    
    // Variable bindings
    void add_variable_binding(const VariableBinding& vb);
    void add_variable_binding(const ObjectIdentifier& oid, const SnmpValue& value, DataType type);
    const std::vector<VariableBinding>& variable_bindings() const;
    void clear_variable_bindings();
    
    // Serialization
    virtual std::vector<uint8_t> encode() const;
    static Result<std::unique_ptr<SnmpPdu>> decode(const std::vector<uint8_t>& data, size_t& offset);
    
    // Validation
    virtual bool is_valid() const;
    
protected:
    PduType type_;
    int32_t request_id_;
    ErrorStatus error_status_;
    int32_t error_index_;
    std::vector<VariableBinding> variable_bindings_;
};

// GetBulk PDU (SNMPv2c specific)
class GetBulkPdu : public SnmpPdu {
public:
    GetBulkPdu();
    
    // GetBulk specific parameters
    int32_t non_repeaters() const;
    int32_t max_repetitions() const;
    
    void set_non_repeaters(int32_t non_repeaters);
    void set_max_repetitions(int32_t max_repetitions);
    
    // Override serialization
    std::vector<uint8_t> encode() const override;
    bool is_valid() const override;
    
private:
    int32_t non_repeaters_;
    int32_t max_repetitions_;
};

// Trap PDU (SNMPv1 specific)
class TrapV1Pdu : public SnmpPdu {
public:
    TrapV1Pdu();
    
    // Trap specific fields
    const ObjectIdentifier& enterprise() const;
    std::vector<uint8_t> agent_addr() const;
    GenericTrap generic_trap() const;
    int32_t specific_trap() const;
    uint32_t timestamp() const;
    
    void set_enterprise(const ObjectIdentifier& enterprise);
    void set_agent_addr(const std::vector<uint8_t>& agent_addr);
    void set_generic_trap(GenericTrap generic_trap);
    void set_specific_trap(int32_t specific_trap);
    void set_timestamp(uint32_t timestamp);
    
    // Override serialization
    std::vector<uint8_t> encode() const override;
    bool is_valid() const override;
    
private:
    ObjectIdentifier enterprise_;
    std::vector<uint8_t> agent_addr_;
    GenericTrap generic_trap_;
    int32_t specific_trap_;
    uint32_t timestamp_;
};

// SNMP message container
class SnmpMessage {
public:
    SnmpMessage(SnmpVersion version, std::string_view community);
    
    // Message properties
    SnmpVersion version() const;
    const std::string& community() const;
    const SnmpPdu* pdu() const;
    
    void set_version(SnmpVersion version);
    void set_community(std::string_view community);
    void set_pdu(std::unique_ptr<SnmpPdu> pdu);
    
    // Serialization
    std::vector<uint8_t> encode() const;
    static Result<SnmpMessage> decode(const std::vector<uint8_t>& data);
    
    // Validation
    bool is_valid() const;
    size_t estimated_size() const;
    
private:
    SnmpVersion version_;
    std::string community_;
    std::unique_ptr<SnmpPdu> pdu_;
};

// MIB object for agent implementation
class MibObject {
public:
    using GetHandler = std::function<Result<SnmpValue>()>;
    using SetHandler = std::function<Result<void>(const SnmpValue&)>;
    
    MibObject(const ObjectIdentifier& oid, DataType type, bool writable = false);
    
    // Object properties
    const ObjectIdentifier& oid() const;
    DataType type() const;
    bool is_writable() const;
    
    // Value access
    Result<SnmpValue> get_value();
    Result<void> set_value(const SnmpValue& value);
    
    // Handler management
    void set_get_handler(GetHandler handler);
    void set_set_handler(SetHandler handler);
    
    // Static value management
    void set_static_value(const SnmpValue& value);
    
private:
    ObjectIdentifier oid_;
    DataType type_;
    bool writable_;
    GetHandler get_handler_;
    SetHandler set_handler_;
    SnmpValue static_value_;
    bool has_static_value_;
};

// SNMP connection wrapper
class SnmpConnection {
public:
    SnmpConnection(udp::UdpSocket socket);
    
    // Connection info
    SocketAddress local_address() const;
    SocketAddress remote_address() const;
    bool is_connected() const;
    
    // Message operations
    Result<void> send_message(const SnmpMessage& message);
    Result<SnmpMessage> receive_message(std::chrono::milliseconds timeout = std::chrono::milliseconds(5000));
    
    // Close connection
    void close();
    
private:
    udp::UdpSocket socket_;
};

// SNMP Manager (client) implementation
class SnmpManager {
public:
    SnmpManager();
    explicit SnmpManager(std::chrono::milliseconds timeout);
    
    // Connection management
    Result<void> connect(const SocketAddress& agent_addr);
    void disconnect();
    bool is_connected() const;
    
    // Basic operations
    Result<SnmpValue> get(const ObjectIdentifier& oid, std::string_view community = "public");
    Result<std::vector<VariableBinding>> get_multiple(const std::vector<ObjectIdentifier>& oids, 
                                                      std::string_view community = "public");
    Result<VariableBinding> get_next(const ObjectIdentifier& oid, std::string_view community = "public");
    Result<void> set(const ObjectIdentifier& oid, const SnmpValue& value, DataType type,
                     std::string_view community = "private");
    
    // Advanced operations (SNMPv2c)
    Result<std::vector<VariableBinding>> get_bulk(const std::vector<ObjectIdentifier>& oids,
                                                  int32_t non_repeaters, int32_t max_repetitions,
                                                  std::string_view community = "public");
    Result<void> inform(const ObjectIdentifier& oid, const SnmpValue& value, DataType type,
                       std::string_view community = "public");
    
    // Table walking
    Result<std::vector<VariableBinding>> walk_table(const ObjectIdentifier& table_oid,
                                                    std::string_view community = "public");
    
    // Configuration
    void set_timeout(std::chrono::milliseconds timeout);
    void set_retries(int retries);
    void set_version(SnmpVersion version);
    SocketAddress local_address() const;
    SocketAddress remote_address() const;
    
private:
    std::unique_ptr<SnmpConnection> connection_;
    std::chrono::milliseconds timeout_;
    int retries_;
    SnmpVersion version_;
    std::atomic<int32_t> next_request_id_;
    
    int32_t generate_request_id();
    Result<SnmpMessage> send_request_and_wait_response(const SnmpMessage& request);
};

// SNMP Agent (server) implementation
class SnmpAgent {
public:
    using TrapHandler = std::function<void(const SnmpMessage&, const SocketAddress&)>;
    
    explicit SnmpAgent(Port port = 161);
    SnmpAgent(const SocketAddress& bind_addr);
    
    // Server lifecycle
    Result<void> start();
    void stop();
    bool is_running() const;
    
    // MIB management
    void add_mib_object(std::unique_ptr<MibObject> object);
    void add_static_object(const ObjectIdentifier& oid, const SnmpValue& value, DataType type, bool writable = false);
    template<typename T>
    void add_dynamic_object(const ObjectIdentifier& oid, T* variable, DataType type, bool writable = false);
    void remove_mib_object(const ObjectIdentifier& oid);
    void clear_mib();
    
    // Community management
    void add_read_community(std::string_view community);
    void add_write_community(std::string_view community);
    void remove_community(std::string_view community);
    bool is_valid_read_community(std::string_view community) const;
    bool is_valid_write_community(std::string_view community) const;
    
    // Trap sending
    Result<void> send_trap(const SocketAddress& manager_addr, const ObjectIdentifier& trap_oid,
                          const std::vector<VariableBinding>& variables = {},
                          std::string_view community = "public");
    Result<void> send_v1_trap(const SocketAddress& manager_addr, const ObjectIdentifier& enterprise,
                             GenericTrap generic_trap, int32_t specific_trap,
                             const std::vector<VariableBinding>& variables = {},
                             std::string_view community = "public");
    
    // Event handlers
    void set_trap_handler(TrapHandler handler);
    
    // Configuration
    void set_system_description(std::string_view description);
    void set_system_contact(std::string_view contact);
    void set_system_name(std::string_view name);
    void set_system_location(std::string_view location);
    void set_max_message_size(size_t size);
    
    // Server information
    SocketAddress local_address() const;
    size_t active_connections() const;
    std::vector<ObjectIdentifier> get_mib_oids() const;
    
private:
    std::unique_ptr<udp::UdpSocket> socket_;
    SocketAddress bind_addr_;
    std::atomic<bool> running_;
    std::thread server_thread_;
    
    // MIB storage
    std::unordered_map<ObjectIdentifier, std::unique_ptr<MibObject>> mib_objects_;
    std::vector<ObjectIdentifier> sorted_oids_;
    
    // Community strings
    std::unordered_set<std::string> read_communities_;
    std::unordered_set<std::string> write_communities_;
    
    // Event handlers
    TrapHandler trap_handler_;
    
    // Configuration
    size_t max_message_size_;
    std::atomic<size_t> active_connections_;
    
    // System information
    std::string system_description_;
    std::string system_contact_;
    std::string system_name_;
    std::string system_location_;
    std::chrono::steady_clock::time_point start_time_;
    
    // Private methods
    void server_loop();
    void handle_request(const std::vector<uint8_t>& data, const SocketAddress& client_addr);
    std::unique_ptr<SnmpPdu> process_get_request(const SnmpPdu* request_pdu, std::string_view community);
    std::unique_ptr<SnmpPdu> process_get_next_request(const SnmpPdu* request_pdu, std::string_view community);
    std::unique_ptr<SnmpPdu> process_get_bulk_request(const GetBulkPdu* request_pdu, std::string_view community);
    std::unique_ptr<SnmpPdu> process_set_request(const SnmpPdu* request_pdu, std::string_view community);
    void process_trap(const SnmpMessage& message, const SocketAddress& sender_addr);
    
    MibObject* find_mib_object(const ObjectIdentifier& oid);
    ObjectIdentifier find_next_oid(const ObjectIdentifier& oid);
    void sort_mib_oids();
    void initialize_system_mib();
    uint32_t get_uptime() const;
};

// SNMP utility functions namespace
namespace snmp_utils {
    // ASN.1 BER encoding/decoding
    std::vector<uint8_t> encode_integer(int32_t value);
    std::vector<uint8_t> encode_unsigned32(uint32_t value);
    std::vector<uint8_t> encode_counter64(uint64_t value);
    std::vector<uint8_t> encode_octet_string(const std::string& value);
    std::vector<uint8_t> encode_octet_string(const std::vector<uint8_t>& value);
    std::vector<uint8_t> encode_object_identifier(const ObjectIdentifier& oid);
    std::vector<uint8_t> encode_null();
    std::vector<uint8_t> encode_sequence(const std::vector<uint8_t>& content);
    std::vector<uint8_t> encode_length(size_t length);
    
    Result<int32_t> decode_integer(const std::vector<uint8_t>& data, size_t& offset);
    Result<uint32_t> decode_unsigned32(const std::vector<uint8_t>& data, size_t& offset);
    Result<uint64_t> decode_counter64(const std::vector<uint8_t>& data, size_t& offset);
    Result<std::string> decode_octet_string(const std::vector<uint8_t>& data, size_t& offset);
    Result<std::vector<uint8_t>> decode_octet_string_bytes(const std::vector<uint8_t>& data, size_t& offset);
    Result<ObjectIdentifier> decode_object_identifier(const std::vector<uint8_t>& data, size_t& offset);
    Result<size_t> decode_length(const std::vector<uint8_t>& data, size_t& offset);
    
    // OID utilities
    bool is_valid_oid_string(std::string_view oid_str);
    std::vector<std::string> split_oid_string(std::string_view oid_str);
    ObjectIdentifier join_oids(const ObjectIdentifier& base, const ObjectIdentifier& suffix);
    
    // Data type utilities
    std::string_view data_type_to_string(DataType type);
    std::string_view pdu_type_to_string(PduType type);
    std::string_view error_status_to_string(ErrorStatus status);
    std::string_view generic_trap_to_string(GenericTrap trap);
    std::string_view version_to_string(SnmpVersion version);
    
    // Value conversion utilities
    std::string snmp_value_to_string(const SnmpValue& value, DataType type);
    Result<SnmpValue> string_to_snmp_value(std::string_view str, DataType type);
    bool is_compatible_type(const SnmpValue& value, DataType type);
    
    // Network utilities
    std::vector<uint8_t> ip_address_to_bytes(std::string_view ip_str);
    std::string ip_address_from_bytes(const std::vector<uint8_t>& bytes);
    bool is_valid_ip_address(std::string_view ip_str);
    
    // Time utilities
    uint32_t get_system_uptime_ticks();
    std::string format_uptime(uint32_t ticks);
    std::chrono::system_clock::time_point ticks_to_time_point(uint32_t ticks);
    
    // Standard OID constants
    namespace oids {
        extern const ObjectIdentifier SYSTEM;              // 1.3.6.1.2.1.1
        extern const ObjectIdentifier SYS_DESCR;          // 1.3.6.1.2.1.1.1.0
        extern const ObjectIdentifier SYS_OBJECT_ID;      // 1.3.6.1.2.1.1.2.0
        extern const ObjectIdentifier SYS_UP_TIME;        // 1.3.6.1.2.1.1.3.0
        extern const ObjectIdentifier SYS_CONTACT;        // 1.3.6.1.2.1.1.4.0
        extern const ObjectIdentifier SYS_NAME;           // 1.3.6.1.2.1.1.5.0
        extern const ObjectIdentifier SYS_LOCATION;       // 1.3.6.1.2.1.1.6.0
        extern const ObjectIdentifier SYS_SERVICES;       // 1.3.6.1.2.1.1.7.0
        
        extern const ObjectIdentifier SNMP_TRAPS;         // 1.3.6.1.6.3.1.1.5
        extern const ObjectIdentifier COLD_START;         // 1.3.6.1.6.3.1.1.5.1
        extern const ObjectIdentifier WARM_START;         // 1.3.6.1.6.3.1.1.5.2
        extern const ObjectIdentifier LINK_DOWN;          // 1.3.6.1.6.3.1.1.5.3
        extern const ObjectIdentifier LINK_UP;            // 1.3.6.1.6.3.1.1.5.4
        extern const ObjectIdentifier AUTH_FAILURE;       // 1.3.6.1.6.3.1.1.5.5
        
        extern const ObjectIdentifier ENTERPRISES;        // 1.3.6.1.4.1
        extern const ObjectIdentifier NETWORKQUESTS;      // 1.3.6.1.4.1.99999 (educational)
    }
}

// Template implementation for dynamic objects
template<typename T>
void SnmpAgent::add_dynamic_object(const ObjectIdentifier& oid, T* variable, DataType type, bool writable) {
    auto mib_obj = std::make_unique<MibObject>(oid, type, writable);
    
    // Set get handler
    mib_obj->set_get_handler([variable, type]() -> Result<SnmpValue> {
        if constexpr (std::is_same_v<T, int32_t>) {
            return SnmpValue(*variable);
        } else if constexpr (std::is_same_v<T, uint32_t>) {
            return SnmpValue(*variable);
        } else if constexpr (std::is_same_v<T, uint64_t>) {
            return SnmpValue(*variable);
        } else if constexpr (std::is_same_v<T, std::string>) {
            return SnmpValue(*variable);
        } else {
            return make_error("Unsupported data type for dynamic object");
        }
    });
    
    // Set set handler if writable
    if (writable) {
        mib_obj->set_set_handler([variable, type](const SnmpValue& value) -> Result<void> {
            if constexpr (std::is_same_v<T, int32_t>) {
                if (std::holds_alternative<int32_t>(value)) {
                    *variable = std::get<int32_t>(value);
                    return make_success();
                }
            } else if constexpr (std::is_same_v<T, uint32_t>) {
                if (std::holds_alternative<uint32_t>(value)) {
                    *variable = std::get<uint32_t>(value);
                    return make_success();
                }
            } else if constexpr (std::is_same_v<T, uint64_t>) {
                if (std::holds_alternative<uint64_t>(value)) {
                    *variable = std::get<uint64_t>(value);
                    return make_success();
                }
            } else if constexpr (std::is_same_v<T, std::string>) {
                if (std::holds_alternative<std::string>(value)) {
                    *variable = std::get<std::string>(value);
                    return make_success();
                }
            }
            return make_error("Type mismatch in set operation");
        });
    }
    
    add_mib_object(std::move(mib_obj));
}

} // namespace networkquests::snmp

// Hash specialization for ObjectIdentifier to use in unordered_map
namespace std {
    template<>
    struct hash<networkquests::snmp::ObjectIdentifier> {
        size_t operator()(const networkquests::snmp::ObjectIdentifier& oid) const {
            size_t seed = 0;
            auto components = oid.components();
            for (uint32_t component : components) {
                seed ^= std::hash<uint32_t>{}(component) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            }
            return seed;
        }
    };
}