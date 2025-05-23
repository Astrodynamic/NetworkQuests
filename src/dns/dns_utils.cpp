#include "networkquests/dns.hpp"
#include <cstring>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <regex>

namespace NetworkQuests::Dns::Utils {

// DNS name encoding/decoding
std::vector<uint8_t> encode_dns_name(const std::string& name) {
    std::vector<uint8_t> encoded;
    
    if (name.empty() || name == ".") {
        encoded.push_back(0);  // Root domain
        return encoded;
    }
    
    std::string normalized = name;
    if (normalized.back() == '.') {
        normalized.pop_back();  // Remove trailing dot
    }
    
    std::istringstream iss(normalized);
    std::string label;
    
    while (std::getline(iss, label, '.')) {
        if (label.length() > DNS_MAX_LABEL_SIZE) {
            // Truncate oversized labels
            label.resize(DNS_MAX_LABEL_SIZE);
        }
        
        encoded.push_back(static_cast<uint8_t>(label.length()));
        for (char c : label) {
            encoded.push_back(static_cast<uint8_t>(c));
        }
    }
    
    encoded.push_back(0);  // Null terminator
    return encoded;
}

Result<std::string> decode_dns_name(const uint8_t* data, size_t size, size_t& offset) {
    std::string name;
    bool first_label = true;
    size_t original_offset = offset;
    bool jumped = false;
    
    while (offset < size) {
        uint8_t length = data[offset];
        
        // Check for compression pointer
        if ((length & 0xC0) == 0xC0) {
            if (offset + 1 >= size) {
                return make_error(NetworkError::INVALID_DATA, "Incomplete compression pointer");
            }
            
            // Extract pointer offset
            uint16_t pointer = ((length & 0x3F) << 8) | data[offset + 1];
            
            if (pointer >= size) {
                return make_error(NetworkError::INVALID_DATA, "Invalid compression pointer");
            }
            
            if (!jumped) {
                offset += 2;  // Only advance original offset on first jump
                jumped = true;
            }
            
            // Follow the pointer
            size_t temp_offset = pointer;
            auto result = decode_dns_name(data, size, temp_offset);
            if (!result.has_value()) {
                return result;
            }
            
            if (!first_label && !name.empty()) {
                name += ".";
            }
            name += result.value();
            break;
        }
        
        // Regular label
        if (length == 0) {
            offset++;
            break;  // End of name
        }
        
        if (length > DNS_MAX_LABEL_SIZE) {
            return make_error(NetworkError::INVALID_DATA, "Label too long");
        }
        
        if (offset + 1 + length > size) {
            return make_error(NetworkError::INVALID_DATA, "Label extends beyond data");
        }
        
        if (!first_label) {
            name += ".";
        }
        
        for (size_t i = 1; i <= length; ++i) {
            name += static_cast<char>(data[offset + i]);
        }
        
        offset += 1 + length;
        first_label = false;
    }
    
    if (!jumped) {
        // If we didn't jump, offset is already correct
    }
    
    return name;
}

// String conversion helpers
std::string dns_type_to_string(DnsType type) {
    switch (type) {
        case DnsType::A: return "A";
        case DnsType::NS: return "NS";
        case DnsType::MD: return "MD";
        case DnsType::MF: return "MF";
        case DnsType::CNAME: return "CNAME";
        case DnsType::SOA: return "SOA";
        case DnsType::MB: return "MB";
        case DnsType::MG: return "MG";
        case DnsType::MR: return "MR";
        case DnsType::NULL_RR: return "NULL";
        case DnsType::WKS: return "WKS";
        case DnsType::PTR: return "PTR";
        case DnsType::HINFO: return "HINFO";
        case DnsType::MINFO: return "MINFO";
        case DnsType::MX: return "MX";
        case DnsType::TXT: return "TXT";
        case DnsType::AAAA: return "AAAA";
        case DnsType::AXFR: return "AXFR";
        case DnsType::MAILB: return "MAILB";
        case DnsType::MAILA: return "MAILA";
        case DnsType::ANY: return "ANY";
        default: return "UNKNOWN";
    }
}

std::string dns_class_to_string(DnsClass qclass) {
    switch (qclass) {
        case DnsClass::IN: return "IN";
        case DnsClass::CS: return "CS";
        case DnsClass::CH: return "CH";
        case DnsClass::HS: return "HS";
        case DnsClass::ANY: return "ANY";
        default: return "UNKNOWN";
    }
}

std::string dns_response_code_to_string(DnsResponseCode code) {
    switch (code) {
        case DnsResponseCode::NO_ERROR: return "NOERROR";
        case DnsResponseCode::FORMAT_ERROR: return "FORMERR";
        case DnsResponseCode::SERVER_FAILURE: return "SERVFAIL";
        case DnsResponseCode::NAME_ERROR: return "NXDOMAIN";
        case DnsResponseCode::NOT_IMPLEMENTED: return "NOTIMP";
        case DnsResponseCode::REFUSED: return "REFUSED";
        default: return "UNKNOWN";
    }
}

Result<DnsType> string_to_dns_type(const std::string& type_str) {
    std::string upper_str = type_str;
    std::transform(upper_str.begin(), upper_str.end(), upper_str.begin(), ::toupper);
    
    if (upper_str == "A") return DnsType::A;
    if (upper_str == "NS") return DnsType::NS;
    if (upper_str == "MD") return DnsType::MD;
    if (upper_str == "MF") return DnsType::MF;
    if (upper_str == "CNAME") return DnsType::CNAME;
    if (upper_str == "SOA") return DnsType::SOA;
    if (upper_str == "MB") return DnsType::MB;
    if (upper_str == "MG") return DnsType::MG;
    if (upper_str == "MR") return DnsType::MR;
    if (upper_str == "NULL") return DnsType::NULL_RR;
    if (upper_str == "WKS") return DnsType::WKS;
    if (upper_str == "PTR") return DnsType::PTR;
    if (upper_str == "HINFO") return DnsType::HINFO;
    if (upper_str == "MINFO") return DnsType::MINFO;
    if (upper_str == "MX") return DnsType::MX;
    if (upper_str == "TXT") return DnsType::TXT;
    if (upper_str == "AAAA") return DnsType::AAAA;
    if (upper_str == "AXFR") return DnsType::AXFR;
    if (upper_str == "MAILB") return DnsType::MAILB;
    if (upper_str == "MAILA") return DnsType::MAILA;
    if (upper_str == "ANY") return DnsType::ANY;
    
    return make_error(NetworkError::INVALID_DATA, "Unknown DNS type: " + type_str);
}

Result<DnsClass> string_to_dns_class(const std::string& class_str) {
    std::string upper_str = class_str;
    std::transform(upper_str.begin(), upper_str.end(), upper_str.begin(), ::toupper);
    
    if (upper_str == "IN") return DnsClass::IN;
    if (upper_str == "CS") return DnsClass::CS;
    if (upper_str == "CH") return DnsClass::CH;
    if (upper_str == "HS") return DnsClass::HS;
    if (upper_str == "ANY") return DnsClass::ANY;
    
    return make_error(NetworkError::INVALID_DATA, "Unknown DNS class: " + class_str);
}

// IP address utilities
bool is_valid_ipv4(const std::string& ip) {
    std::regex ipv4_regex(R"(^(\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3})$)");
    std::smatch match;
    
    if (!std::regex_match(ip, match, ipv4_regex)) {
        return false;
    }
    
    for (int i = 1; i <= 4; ++i) {
        int octet = std::stoi(match[i].str());
        if (octet < 0 || octet > 255) {
            return false;
        }
    }
    
    return true;
}

bool is_valid_ipv6(const std::string& ip) {
    // Simplified IPv6 validation
    std::regex ipv6_regex(R"(^([0-9a-fA-F]{0,4}:){2,7}[0-9a-fA-F]{0,4}$|^::$|^::1$|^::ffff:[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}$)");
    return std::regex_match(ip, ipv6_regex);
}

Result<uint32_t> ipv4_string_to_uint32(const std::string& ip) {
    if (!is_valid_ipv4(ip)) {
        return make_error(NetworkError::INVALID_DATA, "Invalid IPv4 address: " + ip);
    }
    
    std::istringstream iss(ip);
    std::string token;
    uint32_t result = 0;
    int shift = 24;
    
    while (std::getline(iss, token, '.') && shift >= 0) {
        int octet = std::stoi(token);
        result |= (static_cast<uint32_t>(octet) << shift);
        shift -= 8;
    }
    
    return result;
}

std::string ipv4_uint32_to_string(uint32_t ip) {
    std::ostringstream oss;
    oss << ((ip >> 24) & 0xFF) << "."
        << ((ip >> 16) & 0xFF) << "."
        << ((ip >> 8) & 0xFF) << "."
        << (ip & 0xFF);
    return oss.str();
}

Result<std::array<uint8_t, 16>> ipv6_string_to_bytes(const std::string& ip) {
    if (!is_valid_ipv6(ip)) {
        return make_error(NetworkError::INVALID_DATA, "Invalid IPv6 address: " + ip);
    }
    
    std::array<uint8_t, 16> result = {};
    
    // Simplified conversion - this would need a more complete implementation
    // for full IPv6 support including :: notation
    if (ip == "::1") {
        result[15] = 1;  // Loopback
        return result;
    }
    
    // For now, just return zeros for complex IPv6 addresses
    // A complete implementation would handle all IPv6 formats
    return result;
}

std::string ipv6_bytes_to_string(const uint8_t bytes[16]) {
    std::ostringstream oss;
    for (int i = 0; i < 16; i += 2) {
        if (i > 0) oss << ":";
        uint16_t word = (static_cast<uint16_t>(bytes[i]) << 8) | bytes[i + 1];
        oss << std::hex << word;
    }
    return oss.str();
}

// Domain name validation
bool is_valid_domain_name(const std::string& name) {
    if (name.empty() || name.length() > DNS_MAX_NAME_SIZE) {
        return false;
    }
    
    if (name == ".") {
        return true;  // Root domain
    }
    
    std::string working_name = name;
    if (working_name.back() == '.') {
        working_name.pop_back();
    }
    
    std::istringstream iss(working_name);
    std::string label;
    
    while (std::getline(iss, label, '.')) {
        if (label.empty() || label.length() > DNS_MAX_LABEL_SIZE) {
            return false;
        }
        
        // Label must start and end with alphanumeric
        if (!std::isalnum(label.front()) || !std::isalnum(label.back())) {
            if (label.length() > 1) return false;
            if (!std::isalnum(label.front())) return false;
        }
        
        // Label can contain hyphens in the middle
        for (char c : label) {
            if (!std::isalnum(c) && c != '-') {
                return false;
            }
        }
    }
    
    return true;
}

std::string normalize_domain_name(const std::string& name) {
    if (name.empty()) return ".";
    
    std::string result = name;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    
    if (result.back() != '.') {
        result += '.';
    }
    
    return result;
}

// Network byte order conversion
uint16_t htons_portable(uint16_t value) {
    return ((value & 0xFF00) >> 8) | ((value & 0x00FF) << 8);
}

uint32_t htonl_portable(uint32_t value) {
    return ((value & 0xFF000000) >> 24) |
           ((value & 0x00FF0000) >> 8) |
           ((value & 0x0000FF00) << 8) |
           ((value & 0x000000FF) << 24);
}

uint16_t ntohs_portable(uint16_t value) {
    return htons_portable(value);  // Same operation
}

uint32_t ntohl_portable(uint32_t value) {
    return htonl_portable(value);  // Same operation
}

} // namespace NetworkQuests::Dns::Utils