#include "networkquests/snmp.hpp"
#include <algorithm>

namespace networkquests::snmp {

// SnmpMessage implementation

SnmpMessage::SnmpMessage(SnmpVersion version, std::string_view community) 
    : version_(version), community_(community) {}

SnmpVersion SnmpMessage::version() const {
    return version_;
}

const std::string& SnmpMessage::community() const {
    return community_;
}

const SnmpPdu* SnmpMessage::pdu() const {
    return pdu_.get();
}

void SnmpMessage::set_version(SnmpVersion version) {
    version_ = version;
}

void SnmpMessage::set_community(std::string_view community) {
    community_ = community;
}

void SnmpMessage::set_pdu(std::unique_ptr<SnmpPdu> pdu) {
    pdu_ = std::move(pdu);
}

std::vector<uint8_t> SnmpMessage::encode() const {
    if (!pdu_) {
        return {};
    }
    
    std::vector<uint8_t> content;
    
    // Encode version
    auto version_bytes = snmp_utils::encode_integer(static_cast<int32_t>(version_));
    content.insert(content.end(), version_bytes.begin(), version_bytes.end());
    
    // Encode community string
    auto community_bytes = snmp_utils::encode_octet_string(community_);
    content.insert(content.end(), community_bytes.begin(), community_bytes.end());
    
    // Encode PDU
    auto pdu_bytes = pdu_->encode();
    content.insert(content.end(), pdu_bytes.begin(), pdu_bytes.end());
    
    // Wrap entire message in a sequence
    return snmp_utils::encode_sequence(content);
}

Result<SnmpMessage> SnmpMessage::decode(const std::vector<uint8_t>& data) {
    if (data.empty()) {
        return make_error("Empty SNMP message data");
    }
    
    size_t offset = 0;
    
    // Check sequence tag
    if (data[offset] != static_cast<uint8_t>(DataType::SEQUENCE)) {
        return make_error("Invalid SNMP message sequence tag");
    }
    offset++;
    
    // Decode sequence length
    auto length_result = snmp_utils::decode_length(data, offset);
    if (!length_result) {
        return make_error("Failed to decode SNMP message length");
    }
    
    size_t message_end = offset + length_result.value();
    if (message_end > data.size()) {
        return make_error("SNMP message extends beyond data");
    }
    
    // Decode version
    auto version_result = snmp_utils::decode_integer(data, offset);
    if (!version_result) {
        return make_error("Failed to decode SNMP version");
    }
    
    SnmpVersion version = static_cast<SnmpVersion>(version_result.value());
    if (version != SnmpVersion::V1 && version != SnmpVersion::V2C && version != SnmpVersion::V3) {
        return make_error("Unsupported SNMP version");
    }
    
    // Decode community string
    auto community_result = snmp_utils::decode_octet_string(data, offset);
    if (!community_result) {
        return make_error("Failed to decode community string");
    }
    
    // Create message
    SnmpMessage message(version, community_result.value());
    
    // Decode PDU
    auto pdu_result = SnmpPdu::decode(data, offset);
    if (!pdu_result) {
        return make_error("Failed to decode PDU: " + pdu_result.error());
    }
    
    message.set_pdu(std::move(pdu_result.value()));
    
    return message;
}

bool SnmpMessage::is_valid() const {
    return pdu_ && pdu_->is_valid() && !community_.empty();
}

size_t SnmpMessage::estimated_size() const {
    if (!pdu_) {
        return 0;
    }
    
    // Rough estimation: version (5 bytes) + community (2 + length) + PDU
    size_t size = 5; // Version overhead
    size += 2 + community_.size(); // Community overhead + length
    size += pdu_->variable_bindings().size() * 50; // Rough estimate per VB
    size += 50; // PDU overhead
    
    return size;
}

// MibObject implementation

MibObject::MibObject(const ObjectIdentifier& oid, DataType type, bool writable)
    : oid_(oid), type_(type), writable_(writable), has_static_value_(false) {}

const ObjectIdentifier& MibObject::oid() const {
    return oid_;
}

DataType MibObject::type() const {
    return type_;
}

bool MibObject::is_writable() const {
    return writable_;
}

Result<SnmpValue> MibObject::get_value() {
    if (get_handler_) {
        return get_handler_();
    } else if (has_static_value_) {
        return static_value_;
    } else {
        return make_error("No value available for MIB object");
    }
}

Result<void> MibObject::set_value(const SnmpValue& value) {
    if (!writable_) {
        return make_error("MIB object is not writable");
    }
    
    if (!snmp_utils::is_compatible_type(value, type_)) {
        return make_error("Value type incompatible with MIB object type");
    }
    
    if (set_handler_) {
        return set_handler_(value);
    } else if (has_static_value_) {
        static_value_ = value;
        return make_success();
    } else {
        return make_error("No set handler available for MIB object");
    }
}

void MibObject::set_get_handler(GetHandler handler) {
    get_handler_ = std::move(handler);
}

void MibObject::set_set_handler(SetHandler handler) {
    set_handler_ = std::move(handler);
}

void MibObject::set_static_value(const SnmpValue& value) {
    if (snmp_utils::is_compatible_type(value, type_)) {
        static_value_ = value;
        has_static_value_ = true;
    }
}

} // namespace networkquests::snmp