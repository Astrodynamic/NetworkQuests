#include "networkquests/dns.hpp"
#include <sstream>
#include <cstring>

namespace NetworkQuests::Dns {

// ARecord implementation
ARecord::ARecord(const std::string& address) {
    auto result = Utils::ipv4_string_to_uint32(address);
    if (!result.has_value()) {
        throw std::invalid_argument("Invalid IPv4 address: " + address);
    }
    address_ = result.value();
}

ARecord::ARecord(uint32_t address) : address_(address) {}

std::string ARecord::get_address_string() const {
    return Utils::ipv4_uint32_to_string(address_);
}

std::vector<uint8_t> ARecord::serialize() const {
    std::vector<uint8_t> data(4);
    uint32_t net_addr = Utils::htonl_portable(address_);
    std::memcpy(data.data(), &net_addr, 4);
    return data;
}

std::unique_ptr<DnsRData> ARecord::clone() const {
    return std::make_unique<ARecord>(address_);
}

std::string ARecord::to_string() const {
    return get_address_string();
}

Result<std::unique_ptr<ARecord>> ARecord::deserialize(const uint8_t* data, size_t size) {
    if (size < 4) {
        return make_error(NetworkError::INVALID_DATA, "A record data too short");
    }
    
    uint32_t net_addr;
    std::memcpy(&net_addr, data, 4);
    uint32_t address = Utils::ntohl_portable(net_addr);
    
    return std::make_unique<ARecord>(address);
}

// AAAARecord implementation
AAAARecord::AAAARecord(const std::string& address) {
    auto result = Utils::ipv6_string_to_bytes(address);
    if (!result.has_value()) {
        throw std::invalid_argument("Invalid IPv6 address: " + address);
    }
    std::memcpy(address_, result.value().data(), 16);
}

AAAARecord::AAAARecord(const uint8_t address[16]) {
    std::memcpy(address_, address, 16);
}

std::string AAAARecord::get_address_string() const {
    return Utils::ipv6_bytes_to_string(address_);
}

std::vector<uint8_t> AAAARecord::serialize() const {
    return std::vector<uint8_t>(address_, address_ + 16);
}

std::unique_ptr<DnsRData> AAAARecord::clone() const {
    return std::make_unique<AAAARecord>(address_);
}

std::string AAAARecord::to_string() const {
    return get_address_string();
}

Result<std::unique_ptr<AAAARecord>> AAAARecord::deserialize(const uint8_t* data, size_t size) {
    if (size < 16) {
        return make_error(NetworkError::INVALID_DATA, "AAAA record data too short");
    }
    
    return std::make_unique<AAAARecord>(data);
}

// NSRecord implementation
NSRecord::NSRecord(const std::string& name_server) : name_server_(name_server) {}

std::vector<uint8_t> NSRecord::serialize() const {
    return Utils::encode_dns_name(name_server_);
}

std::unique_ptr<DnsRData> NSRecord::clone() const {
    return std::make_unique<NSRecord>(name_server_);
}

std::string NSRecord::to_string() const {
    return name_server_;
}

Result<std::unique_ptr<NSRecord>> NSRecord::deserialize(const uint8_t* data, size_t size, size_t& offset) {
    auto name_result = Utils::decode_dns_name(data, size, offset);
    if (!name_result.has_value()) {
        return make_error(name_result.error());
    }
    
    return std::make_unique<NSRecord>(name_result.value());
}

// CNAMERecord implementation
CNAMERecord::CNAMERecord(const std::string& canonical_name) : canonical_name_(canonical_name) {}

std::vector<uint8_t> CNAMERecord::serialize() const {
    return Utils::encode_dns_name(canonical_name_);
}

std::unique_ptr<DnsRData> CNAMERecord::clone() const {
    return std::make_unique<CNAMERecord>(canonical_name_);
}

std::string CNAMERecord::to_string() const {
    return canonical_name_;
}

Result<std::unique_ptr<CNAMERecord>> CNAMERecord::deserialize(const uint8_t* data, size_t size, size_t& offset) {
    auto name_result = Utils::decode_dns_name(data, size, offset);
    if (!name_result.has_value()) {
        return make_error(name_result.error());
    }
    
    return std::make_unique<CNAMERecord>(name_result.value());
}

// PTRRecord implementation
PTRRecord::PTRRecord(const std::string& pointer_name) : pointer_name_(pointer_name) {}

std::vector<uint8_t> PTRRecord::serialize() const {
    return Utils::encode_dns_name(pointer_name_);
}

std::unique_ptr<DnsRData> PTRRecord::clone() const {
    return std::make_unique<PTRRecord>(pointer_name_);
}

std::string PTRRecord::to_string() const {
    return pointer_name_;
}

Result<std::unique_ptr<PTRRecord>> PTRRecord::deserialize(const uint8_t* data, size_t size, size_t& offset) {
    auto name_result = Utils::decode_dns_name(data, size, offset);
    if (!name_result.has_value()) {
        return make_error(name_result.error());
    }
    
    return std::make_unique<PTRRecord>(name_result.value());
}

// MXRecord implementation
MXRecord::MXRecord(uint16_t preference, const std::string& exchange) 
    : preference_(preference), exchange_(exchange) {}

std::vector<uint8_t> MXRecord::serialize() const {
    std::vector<uint8_t> data;
    
    // Preference (2 bytes)
    uint16_t net_pref = Utils::htons_portable(preference_);
    data.push_back((net_pref >> 8) & 0xFF);
    data.push_back(net_pref & 0xFF);
    
    // Exchange name
    auto encoded_name = Utils::encode_dns_name(exchange_);
    data.insert(data.end(), encoded_name.begin(), encoded_name.end());
    
    return data;
}

std::unique_ptr<DnsRData> MXRecord::clone() const {
    return std::make_unique<MXRecord>(preference_, exchange_);
}

std::string MXRecord::to_string() const {
    return std::to_string(preference_) + " " + exchange_;
}

Result<std::unique_ptr<MXRecord>> MXRecord::deserialize(const uint8_t* data, size_t size, size_t& offset) {
    if (offset + 2 > size) {
        return make_error(NetworkError::INVALID_DATA, "MX record data too short for preference");
    }
    
    // Read preference
    uint16_t net_pref = Utils::ntohs_portable(*reinterpret_cast<const uint16_t*>(data + offset));
    offset += 2;
    
    // Read exchange name
    auto name_result = Utils::decode_dns_name(data, size, offset);
    if (!name_result.has_value()) {
        return make_error(name_result.error());
    }
    
    return std::make_unique<MXRecord>(net_pref, name_result.value());
}

// TXTRecord implementation
TXTRecord::TXTRecord(const std::string& text) {
    text_strings_.push_back(text);
}

TXTRecord::TXTRecord(const std::vector<std::string>& text_strings) 
    : text_strings_(text_strings) {}

std::vector<uint8_t> TXTRecord::serialize() const {
    std::vector<uint8_t> data;
    
    for (const auto& text : text_strings_) {
        if (text.length() > 255) {
            // Split long strings into multiple parts
            size_t pos = 0;
            while (pos < text.length()) {
                size_t chunk_size = std::min(static_cast<size_t>(255), text.length() - pos);
                data.push_back(static_cast<uint8_t>(chunk_size));
                for (size_t i = 0; i < chunk_size; ++i) {
                    data.push_back(static_cast<uint8_t>(text[pos + i]));
                }
                pos += chunk_size;
            }
        } else {
            data.push_back(static_cast<uint8_t>(text.length()));
            for (char c : text) {
                data.push_back(static_cast<uint8_t>(c));
            }
        }
    }
    
    return data;
}

std::unique_ptr<DnsRData> TXTRecord::clone() const {
    return std::make_unique<TXTRecord>(text_strings_);
}

std::string TXTRecord::to_string() const {
    std::ostringstream oss;
    bool first = true;
    for (const auto& text : text_strings_) {
        if (!first) oss << " ";
        oss << "\"" << text << "\"";
        first = false;
    }
    return oss.str();
}

Result<std::unique_ptr<TXTRecord>> TXTRecord::deserialize(const uint8_t* data, size_t size) {
    std::vector<std::string> text_strings;
    size_t offset = 0;
    
    while (offset < size) {
        if (offset >= size) break;
        
        uint8_t length = data[offset++];
        if (offset + length > size) {
            return make_error(NetworkError::INVALID_DATA, "TXT record string extends beyond data");
        }
        
        std::string text(reinterpret_cast<const char*>(data + offset), length);
        text_strings.push_back(text);
        offset += length;
    }
    
    return std::make_unique<TXTRecord>(text_strings);
}

// SOARecord implementation
SOARecord::SOARecord(const std::string& primary_ns, const std::string& admin_email,
                     uint32_t serial, uint32_t refresh, uint32_t retry,
                     uint32_t expire, uint32_t minimum)
    : primary_ns_(primary_ns), admin_email_(admin_email), serial_(serial),
      refresh_(refresh), retry_(retry), expire_(expire), minimum_(minimum) {}

std::vector<uint8_t> SOARecord::serialize() const {
    std::vector<uint8_t> data;
    
    // Primary NS name
    auto encoded_ns = Utils::encode_dns_name(primary_ns_);
    data.insert(data.end(), encoded_ns.begin(), encoded_ns.end());
    
    // Admin email
    auto encoded_email = Utils::encode_dns_name(admin_email_);
    data.insert(data.end(), encoded_email.begin(), encoded_email.end());
    
    // Add 32-bit values
    auto add_uint32 = [&data](uint32_t value) {
        uint32_t net_value = Utils::htonl_portable(value);
        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&net_value);
        data.insert(data.end(), bytes, bytes + 4);
    };
    
    add_uint32(serial_);
    add_uint32(refresh_);
    add_uint32(retry_);
    add_uint32(expire_);
    add_uint32(minimum_);
    
    return data;
}

std::unique_ptr<DnsRData> SOARecord::clone() const {
    return std::make_unique<SOARecord>(primary_ns_, admin_email_, serial_,
                                       refresh_, retry_, expire_, minimum_);
}

std::string SOARecord::to_string() const {
    std::ostringstream oss;
    oss << primary_ns_ << " " << admin_email_ << " " 
        << serial_ << " " << refresh_ << " " << retry_ << " " 
        << expire_ << " " << minimum_;
    return oss.str();
}

Result<std::unique_ptr<SOARecord>> SOARecord::deserialize(const uint8_t* data, size_t size, size_t& offset) {
    // Read primary NS name
    auto ns_result = Utils::decode_dns_name(data, size, offset);
    if (!ns_result.has_value()) {
        return make_error(ns_result.error());
    }
    
    // Read admin email
    auto email_result = Utils::decode_dns_name(data, size, offset);
    if (!email_result.has_value()) {
        return make_error(email_result.error());
    }
    
    // Check remaining data for 5 32-bit values
    if (offset + 20 > size) {
        return make_error(NetworkError::INVALID_DATA, "SOA record data too short for numeric fields");
    }
    
    auto read_uint32 = [&data, &offset]() {
        uint32_t net_value = *reinterpret_cast<const uint32_t*>(data + offset);
        offset += 4;
        return Utils::ntohl_portable(net_value);
    };
    
    uint32_t serial = read_uint32();
    uint32_t refresh = read_uint32();
    uint32_t retry = read_uint32();
    uint32_t expire = read_uint32();
    uint32_t minimum = read_uint32();
    
    return std::make_unique<SOARecord>(ns_result.value(), email_result.value(),
                                       serial, refresh, retry, expire, minimum);
}

// DnsResourceRecord implementation
DnsResourceRecord::DnsResourceRecord(const std::string& name, DnsType type, DnsClass rclass,
                                     uint32_t ttl, std::unique_ptr<DnsRData> rdata)
    : name_(name), type_(type), class_(rclass), ttl_(ttl), rdata_(std::move(rdata)) {}

DnsResourceRecord::DnsResourceRecord(const DnsResourceRecord& other)
    : name_(other.name_), type_(other.type_), class_(other.class_), ttl_(other.ttl_) {
    if (other.rdata_) {
        rdata_ = other.rdata_->clone();
    }
}

DnsResourceRecord& DnsResourceRecord::operator=(const DnsResourceRecord& other) {
    if (this != &other) {
        name_ = other.name_;
        type_ = other.type_;
        class_ = other.class_;
        ttl_ = other.ttl_;
        rdata_ = other.rdata_ ? other.rdata_->clone() : nullptr;
    }
    return *this;
}

std::vector<uint8_t> DnsResourceRecord::serialize() const {
    std::vector<uint8_t> data;
    
    // Name
    auto encoded_name = Utils::encode_dns_name(name_);
    data.insert(data.end(), encoded_name.begin(), encoded_name.end());
    
    // Type (2 bytes)
    uint16_t net_type = Utils::htons_portable(static_cast<uint16_t>(type_));
    data.push_back((net_type >> 8) & 0xFF);
    data.push_back(net_type & 0xFF);
    
    // Class (2 bytes)
    uint16_t net_class = Utils::htons_portable(static_cast<uint16_t>(class_));
    data.push_back((net_class >> 8) & 0xFF);
    data.push_back(net_class & 0xFF);
    
    // TTL (4 bytes)
    uint32_t net_ttl = Utils::htonl_portable(ttl_);
    const uint8_t* ttl_bytes = reinterpret_cast<const uint8_t*>(&net_ttl);
    data.insert(data.end(), ttl_bytes, ttl_bytes + 4);
    
    // RDATA
    std::vector<uint8_t> rdata_bytes;
    if (rdata_) {
        rdata_bytes = rdata_->serialize();
    }
    
    // RDLENGTH (2 bytes)
    uint16_t rdlength = static_cast<uint16_t>(rdata_bytes.size());
    uint16_t net_rdlength = Utils::htons_portable(rdlength);
    data.push_back((net_rdlength >> 8) & 0xFF);
    data.push_back(net_rdlength & 0xFF);
    
    // RDATA
    data.insert(data.end(), rdata_bytes.begin(), rdata_bytes.end());
    
    return data;
}

Result<DnsResourceRecord> DnsResourceRecord::deserialize(const uint8_t* data, size_t size, size_t& offset) {
    DnsResourceRecord rr;
    
    // Decode name
    auto name_result = Utils::decode_dns_name(data, size, offset);
    if (!name_result.has_value()) {
        return make_error(name_result.error());
    }
    rr.name_ = name_result.value();
    
    // Check remaining data for type, class, ttl, rdlength
    if (offset + 10 > size) {
        return make_error(NetworkError::INVALID_DATA, "Insufficient data for RR header");
    }
    
    // Type
    uint16_t net_type = Utils::ntohs_portable(*reinterpret_cast<const uint16_t*>(data + offset));
    rr.type_ = static_cast<DnsType>(net_type);
    offset += 2;
    
    // Class
    uint16_t net_class = Utils::ntohs_portable(*reinterpret_cast<const uint16_t*>(data + offset));
    rr.class_ = static_cast<DnsClass>(net_class);
    offset += 2;
    
    // TTL
    uint32_t net_ttl = Utils::ntohl_portable(*reinterpret_cast<const uint32_t*>(data + offset));
    rr.ttl_ = net_ttl;
    offset += 4;
    
    // RDLENGTH
    uint16_t rdlength = Utils::ntohs_portable(*reinterpret_cast<const uint16_t*>(data + offset));
    offset += 2;
    
    // Check RDATA size
    if (offset + rdlength > size) {
        return make_error(NetworkError::INVALID_DATA, "RDATA extends beyond message");
    }
    
    // Parse RDATA based on type
    size_t rdata_start = offset;
    switch (rr.type_) {
        case DnsType::A: {
            auto a_result = ARecord::deserialize(data + offset, rdlength);
            if (!a_result.has_value()) {
                return make_error(a_result.error());
            }
            rr.rdata_ = std::move(a_result.value());
            break;
        }
        case DnsType::AAAA: {
            auto aaaa_result = AAAARecord::deserialize(data + offset, rdlength);
            if (!aaaa_result.has_value()) {
                return make_error(aaaa_result.error());
            }
            rr.rdata_ = std::move(aaaa_result.value());
            break;
        }
        case DnsType::NS: {
            auto ns_result = NSRecord::deserialize(data, size, offset);
            if (!ns_result.has_value()) {
                return make_error(ns_result.error());
            }
            rr.rdata_ = std::move(ns_result.value());
            break;
        }
        case DnsType::CNAME: {
            auto cname_result = CNAMERecord::deserialize(data, size, offset);
            if (!cname_result.has_value()) {
                return make_error(cname_result.error());
            }
            rr.rdata_ = std::move(cname_result.value());
            break;
        }
        case DnsType::PTR: {
            auto ptr_result = PTRRecord::deserialize(data, size, offset);
            if (!ptr_result.has_value()) {
                return make_error(ptr_result.error());
            }
            rr.rdata_ = std::move(ptr_result.value());
            break;
        }
        case DnsType::MX: {
            auto mx_result = MXRecord::deserialize(data, size, offset);
            if (!mx_result.has_value()) {
                return make_error(mx_result.error());
            }
            rr.rdata_ = std::move(mx_result.value());
            break;
        }
        case DnsType::TXT: {
            auto txt_result = TXTRecord::deserialize(data + offset, rdlength);
            if (!txt_result.has_value()) {
                return make_error(txt_result.error());
            }
            rr.rdata_ = std::move(txt_result.value());
            break;
        }
        case DnsType::SOA: {
            auto soa_result = SOARecord::deserialize(data, size, offset);
            if (!soa_result.has_value()) {
                return make_error(soa_result.error());
            }
            rr.rdata_ = std::move(soa_result.value());
            break;
        }
        default:
            // For unknown types, just skip the RDATA
            break;
    }
    
    // Ensure we advance by exactly rdlength bytes
    offset = rdata_start + rdlength;
    
    return rr;
}

std::string DnsResourceRecord::to_string() const {
    std::ostringstream oss;
    oss << name_ << " " << ttl_ << " " 
        << Utils::dns_class_to_string(class_) << " "
        << Utils::dns_type_to_string(type_);
    
    if (rdata_) {
        oss << " " << rdata_->to_string();
    }
    
    return oss.str();
}

} // namespace NetworkQuests::Dns