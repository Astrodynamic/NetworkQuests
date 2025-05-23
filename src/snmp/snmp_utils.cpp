#include "networkquests/snmp.hpp"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <regex>

namespace networkquests::snmp::snmp_utils {

// ASN.1 BER encoding functions

std::vector<uint8_t> encode_length(size_t length) {
    std::vector<uint8_t> result;
    
    if (length < 0x80) {
        // Short form: length fits in 7 bits
        result.push_back(static_cast<uint8_t>(length));
    } else {
        // Long form: first byte has bit 7 set and bits 6-0 indicate number of octets
        std::vector<uint8_t> length_octets;
        size_t temp_length = length;
        
        while (temp_length > 0) {
            length_octets.insert(length_octets.begin(), static_cast<uint8_t>(temp_length & 0xFF));
            temp_length >>= 8;
        }
        
        if (length_octets.size() > 127) {
            throw std::runtime_error("Length too large for BER encoding");
        }
        
        result.push_back(0x80 | static_cast<uint8_t>(length_octets.size()));
        result.insert(result.end(), length_octets.begin(), length_octets.end());
    }
    
    return result;
}

Result<size_t> decode_length(const std::vector<uint8_t>& data, size_t& offset) {
    if (offset >= data.size()) {
        return make_error("Insufficient data for length");
    }
    
    uint8_t first_byte = data[offset++];
    
    if ((first_byte & 0x80) == 0) {
        // Short form
        return static_cast<size_t>(first_byte);
    } else {
        // Long form
        size_t num_octets = first_byte & 0x7F;
        
        if (num_octets == 0) {
            return make_error("Indefinite length not supported");
        }
        
        if (num_octets > sizeof(size_t)) {
            return make_error("Length too large");
        }
        
        if (offset + num_octets > data.size()) {
            return make_error("Insufficient data for length octets");
        }
        
        size_t length = 0;
        for (size_t i = 0; i < num_octets; ++i) {
            length = (length << 8) | data[offset++];
        }
        
        return length;
    }
}

std::vector<uint8_t> encode_integer(int32_t value) {
    std::vector<uint8_t> result;
    result.push_back(static_cast<uint8_t>(DataType::INTEGER));
    
    // Convert to bytes (big-endian, two's complement)
    std::vector<uint8_t> value_bytes;
    
    if (value == 0) {
        value_bytes.push_back(0x00);
    } else {
        bool negative = value < 0;
        uint32_t abs_value = static_cast<uint32_t>(negative ? -value : value);
        
        // Extract bytes
        std::vector<uint8_t> temp_bytes;
        while (abs_value > 0) {
            temp_bytes.insert(temp_bytes.begin(), static_cast<uint8_t>(abs_value & 0xFF));
            abs_value >>= 8;
        }
        
        if (negative) {
            // Two's complement
            bool carry = true;
            for (auto it = temp_bytes.rbegin(); it != temp_bytes.rend(); ++it) {
                *it = ~(*it);
                if (carry) {
                    if (*it == 0xFF) {
                        *it = 0x00;
                    } else {
                        (*it)++;
                        carry = false;
                    }
                }
            }
            
            // Ensure the sign bit is set
            if ((temp_bytes[0] & 0x80) == 0) {
                temp_bytes.insert(temp_bytes.begin(), 0xFF);
            }
        } else {
            // Ensure the sign bit is not set for positive numbers
            if ((temp_bytes[0] & 0x80) != 0) {
                temp_bytes.insert(temp_bytes.begin(), 0x00);
            }
        }
        
        value_bytes = std::move(temp_bytes);
    }
    
    // Add length
    auto length_bytes = encode_length(value_bytes.size());
    result.insert(result.end(), length_bytes.begin(), length_bytes.end());
    
    // Add value
    result.insert(result.end(), value_bytes.begin(), value_bytes.end());
    
    return result;
}

Result<int32_t> decode_integer(const std::vector<uint8_t>& data, size_t& offset) {
    if (offset >= data.size() || data[offset] != static_cast<uint8_t>(DataType::INTEGER)) {
        return make_error("Invalid integer tag");
    }
    offset++;
    
    auto length_result = decode_length(data, offset);
    if (!length_result) {
        return make_error("Failed to decode integer length");
    }
    
    size_t length = length_result.value();
    if (length == 0 || length > 4) {
        return make_error("Invalid integer length");
    }
    
    if (offset + length > data.size()) {
        return make_error("Insufficient data for integer value");
    }
    
    int32_t value = 0;
    bool negative = (data[offset] & 0x80) != 0;
    
    for (size_t i = 0; i < length; ++i) {
        value = (value << 8) | data[offset++];
    }
    
    // Handle sign extension for negative numbers
    if (negative && length < 4) {
        uint32_t sign_extend = 0xFFFFFFFF << (length * 8);
        value |= static_cast<int32_t>(sign_extend);
    }
    
    return value;
}

std::vector<uint8_t> encode_unsigned32(uint32_t value) {
    std::vector<uint8_t> result;
    
    // Determine which type to use based on context (caller should specify)
    result.push_back(static_cast<uint8_t>(DataType::COUNTER32)); // Default to COUNTER32
    
    std::vector<uint8_t> value_bytes;
    if (value == 0) {
        value_bytes.push_back(0x00);
    } else {
        while (value > 0) {
            value_bytes.insert(value_bytes.begin(), static_cast<uint8_t>(value & 0xFF));
            value >>= 8;
        }
        
        // Ensure MSB is not set (for unsigned interpretation)
        if ((value_bytes[0] & 0x80) != 0) {
            value_bytes.insert(value_bytes.begin(), 0x00);
        }
    }
    
    auto length_bytes = encode_length(value_bytes.size());
    result.insert(result.end(), length_bytes.begin(), length_bytes.end());
    result.insert(result.end(), value_bytes.begin(), value_bytes.end());
    
    return result;
}

Result<uint32_t> decode_unsigned32(const std::vector<uint8_t>& data, size_t& offset) {
    if (offset >= data.size()) {
        return make_error("Insufficient data for unsigned32 tag");
    }
    
    uint8_t tag = data[offset++];
    if (tag != static_cast<uint8_t>(DataType::COUNTER32) && 
        tag != static_cast<uint8_t>(DataType::GAUGE32) &&
        tag != static_cast<uint8_t>(DataType::TIME_TICKS) &&
        tag != static_cast<uint8_t>(DataType::UNSIGNED32)) {
        return make_error("Invalid unsigned32 tag");
    }
    
    auto length_result = decode_length(data, offset);
    if (!length_result) {
        return make_error("Failed to decode unsigned32 length");
    }
    
    size_t length = length_result.value();
    if (length == 0 || length > 5) { // Allow 5 bytes for leading zero
        return make_error("Invalid unsigned32 length");
    }
    
    if (offset + length > data.size()) {
        return make_error("Insufficient data for unsigned32 value");
    }
    
    uint32_t value = 0;
    for (size_t i = 0; i < length; ++i) {
        value = (value << 8) | data[offset++];
    }
    
    return value;
}

std::vector<uint8_t> encode_counter64(uint64_t value) {
    std::vector<uint8_t> result;
    result.push_back(static_cast<uint8_t>(DataType::COUNTER64));
    
    std::vector<uint8_t> value_bytes;
    if (value == 0) {
        value_bytes.push_back(0x00);
    } else {
        while (value > 0) {
            value_bytes.insert(value_bytes.begin(), static_cast<uint8_t>(value & 0xFF));
            value >>= 8;
        }
        
        // Ensure MSB is not set (for unsigned interpretation)
        if ((value_bytes[0] & 0x80) != 0) {
            value_bytes.insert(value_bytes.begin(), 0x00);
        }
    }
    
    auto length_bytes = encode_length(value_bytes.size());
    result.insert(result.end(), length_bytes.begin(), length_bytes.end());
    result.insert(result.end(), value_bytes.begin(), value_bytes.end());
    
    return result;
}

Result<uint64_t> decode_counter64(const std::vector<uint8_t>& data, size_t& offset) {
    if (offset >= data.size() || data[offset] != static_cast<uint8_t>(DataType::COUNTER64)) {
        return make_error("Invalid counter64 tag");
    }
    offset++;
    
    auto length_result = decode_length(data, offset);
    if (!length_result) {
        return make_error("Failed to decode counter64 length");
    }
    
    size_t length = length_result.value();
    if (length == 0 || length > 9) { // Allow 9 bytes for leading zero
        return make_error("Invalid counter64 length");
    }
    
    if (offset + length > data.size()) {
        return make_error("Insufficient data for counter64 value");
    }
    
    uint64_t value = 0;
    for (size_t i = 0; i < length; ++i) {
        value = (value << 8) | data[offset++];
    }
    
    return value;
}

std::vector<uint8_t> encode_octet_string(const std::string& value) {
    std::vector<uint8_t> result;
    result.push_back(static_cast<uint8_t>(DataType::OCTET_STRING));
    
    auto length_bytes = encode_length(value.size());
    result.insert(result.end(), length_bytes.begin(), length_bytes.end());
    
    result.insert(result.end(), value.begin(), value.end());
    
    return result;
}

std::vector<uint8_t> encode_octet_string(const std::vector<uint8_t>& value) {
    std::vector<uint8_t> result;
    result.push_back(static_cast<uint8_t>(DataType::OCTET_STRING));
    
    auto length_bytes = encode_length(value.size());
    result.insert(result.end(), length_bytes.begin(), length_bytes.end());
    
    result.insert(result.end(), value.begin(), value.end());
    
    return result;
}

Result<std::string> decode_octet_string(const std::vector<uint8_t>& data, size_t& offset) {
    if (offset >= data.size() || data[offset] != static_cast<uint8_t>(DataType::OCTET_STRING)) {
        return make_error("Invalid octet string tag");
    }
    offset++;
    
    auto length_result = decode_length(data, offset);
    if (!length_result) {
        return make_error("Failed to decode octet string length");
    }
    
    size_t length = length_result.value();
    if (offset + length > data.size()) {
        return make_error("Insufficient data for octet string value");
    }
    
    std::string result(data.begin() + offset, data.begin() + offset + length);
    offset += length;
    
    return result;
}

Result<std::vector<uint8_t>> decode_octet_string_bytes(const std::vector<uint8_t>& data, size_t& offset) {
    if (offset >= data.size()) {
        return make_error("Insufficient data for octet string tag");
    }
    
    uint8_t tag = data[offset++];
    if (tag != static_cast<uint8_t>(DataType::OCTET_STRING) &&
        tag != static_cast<uint8_t>(DataType::IP_ADDRESS) &&
        tag != static_cast<uint8_t>(DataType::OPAQUE)) {
        return make_error("Invalid octet string tag");
    }
    
    auto length_result = decode_length(data, offset);
    if (!length_result) {
        return make_error("Failed to decode octet string length");
    }
    
    size_t length = length_result.value();
    if (offset + length > data.size()) {
        return make_error("Insufficient data for octet string value");
    }
    
    std::vector<uint8_t> result(data.begin() + offset, data.begin() + offset + length);
    offset += length;
    
    return result;
}

std::vector<uint8_t> encode_object_identifier(const ObjectIdentifier& oid) {
    std::vector<uint8_t> result;
    result.push_back(static_cast<uint8_t>(DataType::OBJECT_IDENTIFIER));
    
    auto components = oid.components();
    if (components.size() < 2) {
        // Invalid OID
        auto length_bytes = encode_length(0);
        result.insert(result.end(), length_bytes.begin(), length_bytes.end());
        return result;
    }
    
    std::vector<uint8_t> encoded_oid;
    
    // First two components are encoded together
    uint32_t first_byte = components[0] * 40 + components[1];
    
    // Encode using variable-length encoding
    auto encode_subid = [&encoded_oid](uint32_t value) {
        if (value == 0) {
            encoded_oid.push_back(0x00);
            return;
        }
        
        std::vector<uint8_t> temp;
        while (value > 0) {
            temp.insert(temp.begin(), (value & 0x7F));
            value >>= 7;
        }
        
        // Set continuation bit for all except last byte
        for (size_t i = 0; i < temp.size() - 1; ++i) {
            temp[i] |= 0x80;
        }
        
        encoded_oid.insert(encoded_oid.end(), temp.begin(), temp.end());
    };
    
    encode_subid(first_byte);
    
    // Encode remaining components
    for (size_t i = 2; i < components.size(); ++i) {
        encode_subid(components[i]);
    }
    
    auto length_bytes = encode_length(encoded_oid.size());
    result.insert(result.end(), length_bytes.begin(), length_bytes.end());
    result.insert(result.end(), encoded_oid.begin(), encoded_oid.end());
    
    return result;
}

Result<ObjectIdentifier> decode_object_identifier(const std::vector<uint8_t>& data, size_t& offset) {
    if (offset >= data.size() || data[offset] != static_cast<uint8_t>(DataType::OBJECT_IDENTIFIER)) {
        return make_error("Invalid object identifier tag");
    }
    offset++;
    
    auto length_result = decode_length(data, offset);
    if (!length_result) {
        return make_error("Failed to decode object identifier length");
    }
    
    size_t length = length_result.value();
    if (length == 0) {
        return ObjectIdentifier(); // Empty OID
    }
    
    if (offset + length > data.size()) {
        return make_error("Insufficient data for object identifier value");
    }
    
    std::vector<uint32_t> components;
    size_t end_offset = offset + length;
    
    // Decode first subidentifier
    uint32_t first_subid = 0;
    do {
        if (offset >= end_offset) {
            return make_error("Incomplete object identifier");
        }
        
        uint8_t byte = data[offset++];
        first_subid = (first_subid << 7) | (byte & 0x7F);
        
        if ((byte & 0x80) == 0) {
            break;
        }
    } while (true);
    
    // Split first subidentifier into first two components
    components.push_back(first_subid / 40);
    components.push_back(first_subid % 40);
    
    // Decode remaining subidentifiers
    while (offset < end_offset) {
        uint32_t subid = 0;
        
        do {
            if (offset >= end_offset) {
                return make_error("Incomplete object identifier");
            }
            
            uint8_t byte = data[offset++];
            subid = (subid << 7) | (byte & 0x7F);
            
            if ((byte & 0x80) == 0) {
                break;
            }
        } while (true);
        
        components.push_back(subid);
    }
    
    return ObjectIdentifier(components);
}

std::vector<uint8_t> encode_null() {
    std::vector<uint8_t> result;
    result.push_back(static_cast<uint8_t>(DataType::NULL_VALUE));
    result.push_back(0x00); // Length is always 0 for NULL
    return result;
}

std::vector<uint8_t> encode_sequence(const std::vector<uint8_t>& content) {
    std::vector<uint8_t> result;
    result.push_back(static_cast<uint8_t>(DataType::SEQUENCE));
    
    auto length_bytes = encode_length(content.size());
    result.insert(result.end(), length_bytes.begin(), length_bytes.end());
    result.insert(result.end(), content.begin(), content.end());
    
    return result;
}

// OID utility functions

bool is_valid_oid_string(std::string_view oid_str) {
    if (oid_str.empty()) {
        return false;
    }
    
    // Check if string starts/ends with dot
    if (oid_str.front() == '.' || oid_str.back() == '.') {
        return false;
    }
    
    // Check for consecutive dots
    if (oid_str.find("..") != std::string::npos) {
        return false;
    }
    
    // Split and validate each component
    auto components = split_oid_string(oid_str);
    if (components.empty()) {
        return false;
    }
    
    for (const auto& comp : components) {
        if (comp.empty()) {
            return false;
        }
        
        // Check if all characters are digits
        for (char c : comp) {
            if (!std::isdigit(c)) {
                return false;
            }
        }
        
        // Check for leading zeros (except for "0")
        if (comp.size() > 1 && comp[0] == '0') {
            return false;
        }
    }
    
    return true;
}

std::vector<std::string> split_oid_string(std::string_view oid_str) {
    std::vector<std::string> result;
    std::string current;
    
    for (char c : oid_str) {
        if (c == '.') {
            if (!current.empty()) {
                result.push_back(current);
                current.clear();
            }
        } else {
            current += c;
        }
    }
    
    if (!current.empty()) {
        result.push_back(current);
    }
    
    return result;
}

ObjectIdentifier join_oids(const ObjectIdentifier& base, const ObjectIdentifier& suffix) {
    auto base_components = base.components();
    auto suffix_components = suffix.components();
    
    std::vector<uint32_t> result_components;
    result_components.reserve(base_components.size() + suffix_components.size());
    
    result_components.insert(result_components.end(), base_components.begin(), base_components.end());
    result_components.insert(result_components.end(), suffix_components.begin(), suffix_components.end());
    
    return ObjectIdentifier(result_components);
}

// String conversion utilities

std::string_view data_type_to_string(DataType type) {
    switch (type) {
        case DataType::INTEGER: return "INTEGER";
        case DataType::OCTET_STRING: return "OCTET_STRING";
        case DataType::NULL_VALUE: return "NULL";
        case DataType::OBJECT_IDENTIFIER: return "OBJECT_IDENTIFIER";
        case DataType::SEQUENCE: return "SEQUENCE";
        case DataType::IP_ADDRESS: return "IP_ADDRESS";
        case DataType::COUNTER32: return "COUNTER32";
        case DataType::GAUGE32: return "GAUGE32";
        case DataType::TIME_TICKS: return "TIME_TICKS";
        case DataType::OPAQUE: return "OPAQUE";
        case DataType::NSAP_ADDRESS: return "NSAP_ADDRESS";
        case DataType::COUNTER64: return "COUNTER64";
        case DataType::UNSIGNED32: return "UNSIGNED32";
        case DataType::NO_SUCH_OBJECT: return "NO_SUCH_OBJECT";
        case DataType::NO_SUCH_INSTANCE: return "NO_SUCH_INSTANCE";
        case DataType::END_OF_MIB_VIEW: return "END_OF_MIB_VIEW";
        default: return "UNKNOWN";
    }
}

std::string_view pdu_type_to_string(PduType type) {
    switch (type) {
        case PduType::GET_REQUEST: return "GET_REQUEST";
        case PduType::GET_NEXT_REQUEST: return "GET_NEXT_REQUEST";
        case PduType::GET_RESPONSE: return "GET_RESPONSE";
        case PduType::SET_REQUEST: return "SET_REQUEST";
        case PduType::TRAP_V1: return "TRAP_V1";
        case PduType::GET_BULK_REQUEST: return "GET_BULK_REQUEST";
        case PduType::INFORM_REQUEST: return "INFORM_REQUEST";
        case PduType::TRAP_V2: return "TRAP_V2";
        case PduType::REPORT: return "REPORT";
        default: return "UNKNOWN";
    }
}

std::string_view error_status_to_string(ErrorStatus status) {
    switch (status) {
        case ErrorStatus::NO_ERROR: return "NO_ERROR";
        case ErrorStatus::TOO_BIG: return "TOO_BIG";
        case ErrorStatus::NO_SUCH_NAME: return "NO_SUCH_NAME";
        case ErrorStatus::BAD_VALUE: return "BAD_VALUE";
        case ErrorStatus::READ_ONLY: return "READ_ONLY";
        case ErrorStatus::GEN_ERR: return "GEN_ERR";
        case ErrorStatus::NO_ACCESS: return "NO_ACCESS";
        case ErrorStatus::WRONG_TYPE: return "WRONG_TYPE";
        case ErrorStatus::WRONG_LENGTH: return "WRONG_LENGTH";
        case ErrorStatus::WRONG_ENCODING: return "WRONG_ENCODING";
        case ErrorStatus::WRONG_VALUE: return "WRONG_VALUE";
        case ErrorStatus::NO_CREATION: return "NO_CREATION";
        case ErrorStatus::INCONSISTENT_VALUE: return "INCONSISTENT_VALUE";
        case ErrorStatus::RESOURCE_UNAVAILABLE: return "RESOURCE_UNAVAILABLE";
        case ErrorStatus::COMMIT_FAILED: return "COMMIT_FAILED";
        case ErrorStatus::UNDO_FAILED: return "UNDO_FAILED";
        case ErrorStatus::AUTHORIZATION_ERROR: return "AUTHORIZATION_ERROR";
        case ErrorStatus::NOT_WRITABLE: return "NOT_WRITABLE";
        case ErrorStatus::INCONSISTENT_NAME: return "INCONSISTENT_NAME";
        default: return "UNKNOWN";
    }
}

std::string_view generic_trap_to_string(GenericTrap trap) {
    switch (trap) {
        case GenericTrap::COLD_START: return "COLD_START";
        case GenericTrap::WARM_START: return "WARM_START";
        case GenericTrap::LINK_DOWN: return "LINK_DOWN";
        case GenericTrap::LINK_UP: return "LINK_UP";
        case GenericTrap::AUTHENTICATION_FAILURE: return "AUTHENTICATION_FAILURE";
        case GenericTrap::EGP_NEIGHBOR_LOSS: return "EGP_NEIGHBOR_LOSS";
        case GenericTrap::ENTERPRISE_SPECIFIC: return "ENTERPRISE_SPECIFIC";
        default: return "UNKNOWN";
    }
}

std::string_view version_to_string(SnmpVersion version) {
    switch (version) {
        case SnmpVersion::V1: return "SNMPv1";
        case SnmpVersion::V2C: return "SNMPv2c";
        case SnmpVersion::V3: return "SNMPv3";
        default: return "UNKNOWN";
    }
}

// Value conversion utilities

std::string snmp_value_to_string(const SnmpValue& value, DataType type) {
    std::ostringstream oss;
    
    switch (type) {
        case DataType::INTEGER:
            if (std::holds_alternative<int32_t>(value)) {
                oss << std::get<int32_t>(value);
            }
            break;
            
        case DataType::COUNTER32:
        case DataType::GAUGE32:
        case DataType::TIME_TICKS:
        case DataType::UNSIGNED32:
            if (std::holds_alternative<uint32_t>(value)) {
                oss << std::get<uint32_t>(value);
            }
            break;
            
        case DataType::COUNTER64:
            if (std::holds_alternative<uint64_t>(value)) {
                oss << std::get<uint64_t>(value);
            }
            break;
            
        case DataType::OCTET_STRING:
            if (std::holds_alternative<std::string>(value)) {
                oss << std::get<std::string>(value);
            }
            break;
            
        case DataType::IP_ADDRESS:
        case DataType::OPAQUE:
            if (std::holds_alternative<std::vector<uint8_t>>(value)) {
                const auto& bytes = std::get<std::vector<uint8_t>>(value);
                if (type == DataType::IP_ADDRESS && bytes.size() == 4) {
                    oss << static_cast<int>(bytes[0]) << "." 
                        << static_cast<int>(bytes[1]) << "."
                        << static_cast<int>(bytes[2]) << "."
                        << static_cast<int>(bytes[3]);
                } else {
                    oss << "0x";
                    for (uint8_t byte : bytes) {
                        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
                    }
                }
            }
            break;
            
        case DataType::OBJECT_IDENTIFIER:
            if (std::holds_alternative<ObjectIdentifier>(value)) {
                oss << std::get<ObjectIdentifier>(value).to_string();
            }
            break;
            
        case DataType::NULL_VALUE:
            oss << "NULL";
            break;
            
        default:
            oss << "UNKNOWN_TYPE";
            break;
    }
    
    return oss.str();
}

Result<SnmpValue> string_to_snmp_value(std::string_view str, DataType type) {
    try {
        switch (type) {
            case DataType::INTEGER: {
                int32_t value = std::stoi(std::string(str));
                return SnmpValue(value);
            }
            
            case DataType::COUNTER32:
            case DataType::GAUGE32:
            case DataType::TIME_TICKS:
            case DataType::UNSIGNED32: {
                uint32_t value = std::stoul(std::string(str));
                return SnmpValue(value);
            }
            
            case DataType::COUNTER64: {
                uint64_t value = std::stoull(std::string(str));
                return SnmpValue(value);
            }
            
            case DataType::OCTET_STRING: {
                return SnmpValue(std::string(str));
            }
            
            case DataType::IP_ADDRESS: {
                auto bytes = ip_address_to_bytes(str);
                if (bytes.empty()) {
                    return make_error("Invalid IP address format");
                }
                return SnmpValue(bytes);
            }
            
            case DataType::OBJECT_IDENTIFIER: {
                auto oid_result = ObjectIdentifier::from_string(str);
                if (!oid_result) {
                    return make_error("Invalid OID format");
                }
                return SnmpValue(oid_result.value());
            }
            
            case DataType::NULL_VALUE: {
                return SnmpValue(std::monostate{});
            }
            
            default:
                return make_error("Unsupported data type for string conversion");
        }
    } catch (const std::exception& e) {
        return make_error("Failed to convert string to SNMP value: " + std::string(e.what()));
    }
}

bool is_compatible_type(const SnmpValue& value, DataType type) {
    switch (type) {
        case DataType::INTEGER:
            return std::holds_alternative<int32_t>(value);
            
        case DataType::COUNTER32:
        case DataType::GAUGE32:
        case DataType::TIME_TICKS:
        case DataType::UNSIGNED32:
            return std::holds_alternative<uint32_t>(value);
            
        case DataType::COUNTER64:
            return std::holds_alternative<uint64_t>(value);
            
        case DataType::OCTET_STRING:
            return std::holds_alternative<std::string>(value);
            
        case DataType::IP_ADDRESS:
        case DataType::OPAQUE:
            return std::holds_alternative<std::vector<uint8_t>>(value);
            
        case DataType::OBJECT_IDENTIFIER:
            return std::holds_alternative<ObjectIdentifier>(value);
            
        case DataType::NULL_VALUE:
            return std::holds_alternative<std::monostate>(value);
            
        default:
            return false;
    }
}

// Network utilities

std::vector<uint8_t> ip_address_to_bytes(std::string_view ip_str) {
    std::vector<uint8_t> result;
    std::string current;
    
    for (char c : ip_str) {
        if (c == '.') {
            if (current.empty()) {
                return {}; // Invalid format
            }
            
            try {
                int value = std::stoi(current);
                if (value < 0 || value > 255) {
                    return {}; // Invalid range
                }
                result.push_back(static_cast<uint8_t>(value));
                current.clear();
            } catch (...) {
                return {}; // Invalid number
            }
        } else if (std::isdigit(c)) {
            current += c;
        } else {
            return {}; // Invalid character
        }
    }
    
    // Handle last component
    if (!current.empty()) {
        try {
            int value = std::stoi(current);
            if (value < 0 || value > 255) {
                return {}; // Invalid range
            }
            result.push_back(static_cast<uint8_t>(value));
        } catch (...) {
            return {}; // Invalid number
        }
    }
    
    return result.size() == 4 ? result : std::vector<uint8_t>{};
}

std::string ip_address_from_bytes(const std::vector<uint8_t>& bytes) {
    if (bytes.size() != 4) {
        return "";
    }
    
    return std::to_string(bytes[0]) + "." +
           std::to_string(bytes[1]) + "." +
           std::to_string(bytes[2]) + "." +
           std::to_string(bytes[3]);
}

bool is_valid_ip_address(std::string_view ip_str) {
    return !ip_address_to_bytes(ip_str).empty();
}

// Time utilities

uint32_t get_system_uptime_ticks() {
    static auto start_time = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time);
    return static_cast<uint32_t>(duration.count() / 10); // Convert to centiseconds
}

std::string format_uptime(uint32_t ticks) {
    uint32_t centiseconds = ticks;
    uint32_t seconds = centiseconds / 100;
    uint32_t minutes = seconds / 60;
    uint32_t hours = minutes / 60;
    uint32_t days = hours / 24;
    
    centiseconds %= 100;
    seconds %= 60;
    minutes %= 60;
    hours %= 24;
    
    std::ostringstream oss;
    if (days > 0) {
        oss << days << " days, ";
    }
    oss << std::setfill('0') << std::setw(2) << hours << ":"
        << std::setw(2) << minutes << ":"
        << std::setw(2) << seconds << "."
        << std::setw(2) << centiseconds;
    
    return oss.str();
}

std::chrono::system_clock::time_point ticks_to_time_point(uint32_t ticks) {
    auto milliseconds = std::chrono::milliseconds(ticks * 10);
    return std::chrono::system_clock::now() - milliseconds;
}

// Standard OID constants
namespace oids {
    const ObjectIdentifier SYSTEM("1.3.6.1.2.1.1");
    const ObjectIdentifier SYS_DESCR("1.3.6.1.2.1.1.1.0");
    const ObjectIdentifier SYS_OBJECT_ID("1.3.6.1.2.1.1.2.0");
    const ObjectIdentifier SYS_UP_TIME("1.3.6.1.2.1.1.3.0");
    const ObjectIdentifier SYS_CONTACT("1.3.6.1.2.1.1.4.0");
    const ObjectIdentifier SYS_NAME("1.3.6.1.2.1.1.5.0");
    const ObjectIdentifier SYS_LOCATION("1.3.6.1.2.1.1.6.0");
    const ObjectIdentifier SYS_SERVICES("1.3.6.1.2.1.1.7.0");
    
    const ObjectIdentifier SNMP_TRAPS("1.3.6.1.6.3.1.1.5");
    const ObjectIdentifier COLD_START("1.3.6.1.6.3.1.1.5.1");
    const ObjectIdentifier WARM_START("1.3.6.1.6.3.1.1.5.2");
    const ObjectIdentifier LINK_DOWN("1.3.6.1.6.3.1.1.5.3");
    const ObjectIdentifier LINK_UP("1.3.6.1.6.3.1.1.5.4");
    const ObjectIdentifier AUTH_FAILURE("1.3.6.1.6.3.1.1.5.5");
    
    const ObjectIdentifier ENTERPRISES("1.3.6.1.4.1");
    const ObjectIdentifier NETWORKQUESTS("1.3.6.1.4.1.99999");
}

} // namespace networkquests::snmp::snmp_utils