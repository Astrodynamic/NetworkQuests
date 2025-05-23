#include "networkquests/snmp.hpp"
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <cstring>

namespace networkquests::snmp {

// ObjectIdentifier implementation

ObjectIdentifier::ObjectIdentifier() = default;

ObjectIdentifier::ObjectIdentifier(std::string_view oid_string) {
    auto result = from_string(oid_string);
    if (result) {
        *this = result.value();
    }
}

ObjectIdentifier::ObjectIdentifier(const std::vector<uint32_t>& components) 
    : components_(components) {}

void ObjectIdentifier::append(uint32_t component) {
    components_.push_back(component);
}

void ObjectIdentifier::prepend(uint32_t component) {
    components_.insert(components_.begin(), component);
}

std::vector<uint32_t> ObjectIdentifier::components() const {
    return components_;
}

size_t ObjectIdentifier::length() const {
    return components_.size();
}

uint32_t ObjectIdentifier::operator[](size_t index) const {
    if (index >= components_.size()) {
        throw std::out_of_range("OID component index out of range");
    }
    return components_[index];
}

std::string ObjectIdentifier::to_string() const {
    if (components_.empty()) {
        return "";
    }
    
    std::ostringstream oss;
    for (size_t i = 0; i < components_.size(); ++i) {
        if (i > 0) {
            oss << ".";
        }
        oss << components_[i];
    }
    return oss.str();
}

Result<ObjectIdentifier> ObjectIdentifier::from_string(std::string_view oid_str) {
    if (!snmp_utils::is_valid_oid_string(oid_str)) {
        return make_error("Invalid OID string format");
    }
    
    auto string_components = snmp_utils::split_oid_string(oid_str);
    std::vector<uint32_t> components;
    components.reserve(string_components.size());
    
    try {
        for (const auto& comp_str : string_components) {
            uint64_t value = std::stoull(comp_str);
            if (value > UINT32_MAX) {
                return make_error("OID component value too large");
            }
            components.push_back(static_cast<uint32_t>(value));
        }
    } catch (const std::exception& e) {
        return make_error("Failed to parse OID component: " + std::string(e.what()));
    }
    
    return ObjectIdentifier(components);
}

bool ObjectIdentifier::operator==(const ObjectIdentifier& other) const {
    return components_ == other.components_;
}

bool ObjectIdentifier::operator<(const ObjectIdentifier& other) const {
    return std::lexicographical_compare(
        components_.begin(), components_.end(),
        other.components_.begin(), other.components_.end()
    );
}

bool ObjectIdentifier::operator<=(const ObjectIdentifier& other) const {
    return *this < other || *this == other;
}

bool ObjectIdentifier::operator>(const ObjectIdentifier& other) const {
    return !(*this <= other);
}

bool ObjectIdentifier::operator>=(const ObjectIdentifier& other) const {
    return !(*this < other);
}

bool ObjectIdentifier::is_child_of(const ObjectIdentifier& parent) const {
    if (parent.components_.size() >= components_.size()) {
        return false;
    }
    
    return std::equal(parent.components_.begin(), parent.components_.end(), 
                     components_.begin());
}

bool ObjectIdentifier::is_parent_of(const ObjectIdentifier& child) const {
    return child.is_child_of(*this);
}

ObjectIdentifier ObjectIdentifier::get_parent() const {
    if (components_.empty()) {
        return ObjectIdentifier();
    }
    
    std::vector<uint32_t> parent_components(components_.begin(), components_.end() - 1);
    return ObjectIdentifier(parent_components);
}

ObjectIdentifier ObjectIdentifier::get_next() const {
    if (components_.empty()) {
        return ObjectIdentifier({0});
    }
    
    std::vector<uint32_t> next_components = components_;
    
    // Increment the last component
    if (next_components.back() < UINT32_MAX) {
        next_components.back()++;
    } else {
        // Handle overflow by adding a new component
        next_components.push_back(0);
    }
    
    return ObjectIdentifier(next_components);
}

bool ObjectIdentifier::is_valid() const {
    if (components_.size() < 2) {
        return false;
    }
    
    // First component must be 0, 1, or 2
    if (components_[0] > 2) {
        return false;
    }
    
    // If first component is 0 or 1, second must be < 40
    if (components_[0] < 2 && components_[1] >= 40) {
        return false;
    }
    
    return true;
}

// VariableBinding implementation

VariableBinding::VariableBinding() : type_(DataType::NULL_VALUE) {}

VariableBinding::VariableBinding(const ObjectIdentifier& oid, const SnmpValue& value, DataType type)
    : oid_(oid), value_(value), type_(type) {}

const ObjectIdentifier& VariableBinding::oid() const {
    return oid_;
}

const SnmpValue& VariableBinding::value() const {
    return value_;
}

DataType VariableBinding::type() const {
    return type_;
}

void VariableBinding::set_oid(const ObjectIdentifier& oid) {
    oid_ = oid;
}

void VariableBinding::set_value(const SnmpValue& value, DataType type) {
    value_ = value;
    type_ = type;
}

void VariableBinding::set_exception(DataType exception_type) {
    if (exception_type == DataType::NO_SUCH_OBJECT ||
        exception_type == DataType::NO_SUCH_INSTANCE ||
        exception_type == DataType::END_OF_MIB_VIEW) {
        value_ = std::monostate{};
        type_ = exception_type;
    }
}

bool VariableBinding::is_valid() const {
    return oid_.is_valid() && snmp_utils::is_compatible_type(value_, type_);
}

bool VariableBinding::is_exception() const {
    return type_ == DataType::NO_SUCH_OBJECT ||
           type_ == DataType::NO_SUCH_INSTANCE ||
           type_ == DataType::END_OF_MIB_VIEW;
}

std::vector<uint8_t> VariableBinding::encode() const {
    std::vector<uint8_t> result;
    
    // Encode OID
    auto oid_bytes = snmp_utils::encode_object_identifier(oid_);
    result.insert(result.end(), oid_bytes.begin(), oid_bytes.end());
    
    // Encode value based on type
    std::vector<uint8_t> value_bytes;
    
    switch (type_) {
        case DataType::INTEGER:
            if (std::holds_alternative<int32_t>(value_)) {
                value_bytes = snmp_utils::encode_integer(std::get<int32_t>(value_));
            }
            break;
            
        case DataType::OCTET_STRING:
            if (std::holds_alternative<std::string>(value_)) {
                value_bytes = snmp_utils::encode_octet_string(std::get<std::string>(value_));
            }
            break;
            
        case DataType::OBJECT_IDENTIFIER:
            if (std::holds_alternative<ObjectIdentifier>(value_)) {
                value_bytes = snmp_utils::encode_object_identifier(std::get<ObjectIdentifier>(value_));
            }
            break;
            
        case DataType::COUNTER32:
        case DataType::GAUGE32:
        case DataType::TIME_TICKS:
        case DataType::UNSIGNED32:
            if (std::holds_alternative<uint32_t>(value_)) {
                value_bytes = snmp_utils::encode_unsigned32(std::get<uint32_t>(value_));
                value_bytes[0] = static_cast<uint8_t>(type_); // Set correct tag
            }
            break;
            
        case DataType::COUNTER64:
            if (std::holds_alternative<uint64_t>(value_)) {
                value_bytes = snmp_utils::encode_counter64(std::get<uint64_t>(value_));
            }
            break;
            
        case DataType::IP_ADDRESS:
        case DataType::OPAQUE:
            if (std::holds_alternative<std::vector<uint8_t>>(value_)) {
                value_bytes = snmp_utils::encode_octet_string(std::get<std::vector<uint8_t>>(value_));
                value_bytes[0] = static_cast<uint8_t>(type_); // Set correct tag
            }
            break;
            
        case DataType::NULL_VALUE:
        case DataType::NO_SUCH_OBJECT:
        case DataType::NO_SUCH_INSTANCE:
        case DataType::END_OF_MIB_VIEW:
            value_bytes.push_back(static_cast<uint8_t>(type_));
            value_bytes.push_back(0x00); // Length 0
            break;
            
        default:
            // Unknown type, encode as null
            value_bytes = snmp_utils::encode_null();
            break;
    }
    
    result.insert(result.end(), value_bytes.begin(), value_bytes.end());
    
    // Wrap in sequence
    return snmp_utils::encode_sequence(result);
}

Result<VariableBinding> VariableBinding::decode(const std::vector<uint8_t>& data, size_t& offset) {
    size_t start_offset = offset;
    
    // Check sequence tag
    if (offset >= data.size() || data[offset] != static_cast<uint8_t>(DataType::SEQUENCE)) {
        return make_error("Invalid variable binding sequence tag");
    }
    offset++;
    
    // Decode sequence length
    auto length_result = snmp_utils::decode_length(data, offset);
    if (!length_result) {
        return make_error("Failed to decode variable binding length");
    }
    
    size_t sequence_end = offset + length_result.value();
    if (sequence_end > data.size()) {
        return make_error("Variable binding sequence extends beyond data");
    }
    
    // Decode OID
    auto oid_result = snmp_utils::decode_object_identifier(data, offset);
    if (!oid_result) {
        return make_error("Failed to decode variable binding OID");
    }
    
    // Decode value
    if (offset >= sequence_end) {
        return make_error("Missing value in variable binding");
    }
    
    uint8_t value_tag = data[offset];
    DataType value_type = static_cast<DataType>(value_tag);
    SnmpValue value;
    
    switch (value_type) {
        case DataType::INTEGER: {
            auto int_result = snmp_utils::decode_integer(data, offset);
            if (!int_result) {
                return make_error("Failed to decode integer value");
            }
            value = int_result.value();
            break;
        }
        
        case DataType::OCTET_STRING: {
            auto str_result = snmp_utils::decode_octet_string(data, offset);
            if (!str_result) {
                return make_error("Failed to decode octet string value");
            }
            value = str_result.value();
            break;
        }
        
        case DataType::OBJECT_IDENTIFIER: {
            auto oid_val_result = snmp_utils::decode_object_identifier(data, offset);
            if (!oid_val_result) {
                return make_error("Failed to decode OID value");
            }
            value = oid_val_result.value();
            break;
        }
        
        case DataType::COUNTER32:
        case DataType::GAUGE32:
        case DataType::TIME_TICKS:
        case DataType::UNSIGNED32: {
            auto uint_result = snmp_utils::decode_unsigned32(data, offset);
            if (!uint_result) {
                return make_error("Failed to decode unsigned32 value");
            }
            value = uint_result.value();
            break;
        }
        
        case DataType::COUNTER64: {
            auto counter64_result = snmp_utils::decode_counter64(data, offset);
            if (!counter64_result) {
                return make_error("Failed to decode counter64 value");
            }
            value = counter64_result.value();
            break;
        }
        
        case DataType::IP_ADDRESS:
        case DataType::OPAQUE: {
            auto bytes_result = snmp_utils::decode_octet_string_bytes(data, offset);
            if (!bytes_result) {
                return make_error("Failed to decode byte string value");
            }
            value = bytes_result.value();
            break;
        }
        
        case DataType::NULL_VALUE:
        case DataType::NO_SUCH_OBJECT:
        case DataType::NO_SUCH_INSTANCE:
        case DataType::END_OF_MIB_VIEW:
            // Skip tag and length
            offset++;
            if (offset < data.size() && data[offset] == 0x00) {
                offset++; // Skip length byte
            }
            value = std::monostate{};
            break;
            
        default:
            return make_error("Unsupported value type in variable binding");
    }
    
    if (offset > sequence_end) {
        return make_error("Variable binding decoding exceeded sequence boundary");
    }
    
    return VariableBinding(oid_result.value(), value, value_type);
}

// SnmpPdu implementation

SnmpPdu::SnmpPdu(PduType type) 
    : type_(type), request_id_(0), error_status_(ErrorStatus::NO_ERROR), error_index_(0) {}

PduType SnmpPdu::type() const {
    return type_;
}

int32_t SnmpPdu::request_id() const {
    return request_id_;
}

ErrorStatus SnmpPdu::error_status() const {
    return error_status_;
}

int32_t SnmpPdu::error_index() const {
    return error_index_;
}

void SnmpPdu::set_request_id(int32_t request_id) {
    request_id_ = request_id;
}

void SnmpPdu::set_error_status(ErrorStatus status) {
    error_status_ = status;
}

void SnmpPdu::set_error_index(int32_t index) {
    error_index_ = index;
}

void SnmpPdu::add_variable_binding(const VariableBinding& vb) {
    variable_bindings_.push_back(vb);
}

void SnmpPdu::add_variable_binding(const ObjectIdentifier& oid, const SnmpValue& value, DataType type) {
    variable_bindings_.emplace_back(oid, value, type);
}

const std::vector<VariableBinding>& SnmpPdu::variable_bindings() const {
    return variable_bindings_;
}

void SnmpPdu::clear_variable_bindings() {
    variable_bindings_.clear();
}

std::vector<uint8_t> SnmpPdu::encode() const {
    std::vector<uint8_t> content;
    
    // Encode request ID
    auto request_id_bytes = snmp_utils::encode_integer(request_id_);
    content.insert(content.end(), request_id_bytes.begin(), request_id_bytes.end());
    
    // Encode error status
    auto error_status_bytes = snmp_utils::encode_integer(static_cast<int32_t>(error_status_));
    content.insert(content.end(), error_status_bytes.begin(), error_status_bytes.end());
    
    // Encode error index
    auto error_index_bytes = snmp_utils::encode_integer(error_index_);
    content.insert(content.end(), error_index_bytes.begin(), error_index_bytes.end());
    
    // Encode variable bindings list
    std::vector<uint8_t> vb_list_content;
    for (const auto& vb : variable_bindings_) {
        auto vb_bytes = vb.encode();
        vb_list_content.insert(vb_list_content.end(), vb_bytes.begin(), vb_bytes.end());
    }
    
    auto vb_list_bytes = snmp_utils::encode_sequence(vb_list_content);
    content.insert(content.end(), vb_list_bytes.begin(), vb_list_bytes.end());
    
    // Wrap with PDU type tag
    std::vector<uint8_t> result;
    result.push_back(static_cast<uint8_t>(type_));
    
    auto length_bytes = snmp_utils::encode_length(content.size());
    result.insert(result.end(), length_bytes.begin(), length_bytes.end());
    result.insert(result.end(), content.begin(), content.end());
    
    return result;
}

Result<std::unique_ptr<SnmpPdu>> SnmpPdu::decode(const std::vector<uint8_t>& data, size_t& offset) {
    if (offset >= data.size()) {
        return make_error("Insufficient data for PDU tag");
    }
    
    uint8_t pdu_tag = data[offset++];
    PduType pdu_type = static_cast<PduType>(pdu_tag);
    
    // Decode PDU length
    auto length_result = snmp_utils::decode_length(data, offset);
    if (!length_result) {
        return make_error("Failed to decode PDU length");
    }
    
    size_t pdu_end = offset + length_result.value();
    if (pdu_end > data.size()) {
        return make_error("PDU extends beyond data");
    }
    
    // Create appropriate PDU type
    std::unique_ptr<SnmpPdu> pdu;
    
    if (pdu_type == PduType::GET_BULK_REQUEST) {
        pdu = std::make_unique<GetBulkPdu>();
    } else if (pdu_type == PduType::TRAP_V1) {
        pdu = std::make_unique<TrapV1Pdu>();
    } else {
        pdu = std::make_unique<SnmpPdu>(pdu_type);
    }
    
    // Decode request ID
    auto request_id_result = snmp_utils::decode_integer(data, offset);
    if (!request_id_result) {
        return make_error("Failed to decode request ID");
    }
    pdu->set_request_id(request_id_result.value());
    
    // For GetBulk PDU, handle non-repeaters and max-repetitions
    if (pdu_type == PduType::GET_BULK_REQUEST) {
        auto bulk_pdu = static_cast<GetBulkPdu*>(pdu.get());
        
        auto non_repeaters_result = snmp_utils::decode_integer(data, offset);
        if (!non_repeaters_result) {
            return make_error("Failed to decode non-repeaters");
        }
        bulk_pdu->set_non_repeaters(non_repeaters_result.value());
        
        auto max_reps_result = snmp_utils::decode_integer(data, offset);
        if (!max_reps_result) {
            return make_error("Failed to decode max-repetitions");
        }
        bulk_pdu->set_max_repetitions(max_reps_result.value());
    } else if (pdu_type == PduType::TRAP_V1) {
        // Handle Trap V1 specific fields
        auto trap_pdu = static_cast<TrapV1Pdu*>(pdu.get());
        
        // Enterprise OID
        auto enterprise_result = snmp_utils::decode_object_identifier(data, offset);
        if (!enterprise_result) {
            return make_error("Failed to decode trap enterprise OID");
        }
        trap_pdu->set_enterprise(enterprise_result.value());
        
        // Agent address
        auto agent_addr_result = snmp_utils::decode_octet_string_bytes(data, offset);
        if (!agent_addr_result) {
            return make_error("Failed to decode trap agent address");
        }
        trap_pdu->set_agent_addr(agent_addr_result.value());
        
        // Generic trap
        auto generic_trap_result = snmp_utils::decode_integer(data, offset);
        if (!generic_trap_result) {
            return make_error("Failed to decode generic trap");
        }
        trap_pdu->set_generic_trap(static_cast<GenericTrap>(generic_trap_result.value()));
        
        // Specific trap
        auto specific_trap_result = snmp_utils::decode_integer(data, offset);
        if (!specific_trap_result) {
            return make_error("Failed to decode specific trap");
        }
        trap_pdu->set_specific_trap(specific_trap_result.value());
        
        // Timestamp
        auto timestamp_result = snmp_utils::decode_unsigned32(data, offset);
        if (!timestamp_result) {
            return make_error("Failed to decode trap timestamp");
        }
        trap_pdu->set_timestamp(timestamp_result.value());
    } else {
        // Standard PDU: decode error status and error index
        auto error_status_result = snmp_utils::decode_integer(data, offset);
        if (!error_status_result) {
            return make_error("Failed to decode error status");
        }
        pdu->set_error_status(static_cast<ErrorStatus>(error_status_result.value()));
        
        auto error_index_result = snmp_utils::decode_integer(data, offset);
        if (!error_index_result) {
            return make_error("Failed to decode error index");
        }
        pdu->set_error_index(error_index_result.value());
    }
    
    // Decode variable bindings sequence
    if (offset >= pdu_end) {
        return make_error("Missing variable bindings in PDU");
    }
    
    if (data[offset] != static_cast<uint8_t>(DataType::SEQUENCE)) {
        return make_error("Invalid variable bindings sequence tag");
    }
    offset++;
    
    auto vb_length_result = snmp_utils::decode_length(data, offset);
    if (!vb_length_result) {
        return make_error("Failed to decode variable bindings length");
    }
    
    size_t vb_end = offset + vb_length_result.value();
    if (vb_end > pdu_end) {
        return make_error("Variable bindings exceed PDU boundary");
    }
    
    // Decode individual variable bindings
    while (offset < vb_end) {
        auto vb_result = VariableBinding::decode(data, offset);
        if (!vb_result) {
            return make_error("Failed to decode variable binding");
        }
        pdu->add_variable_binding(vb_result.value());
    }
    
    return pdu;
}

bool SnmpPdu::is_valid() const {
    for (const auto& vb : variable_bindings_) {
        if (!vb.is_valid()) {
            return false;
        }
    }
    return true;
}

// GetBulkPdu implementation

GetBulkPdu::GetBulkPdu() 
    : SnmpPdu(PduType::GET_BULK_REQUEST), non_repeaters_(0), max_repetitions_(0) {}

int32_t GetBulkPdu::non_repeaters() const {
    return non_repeaters_;
}

int32_t GetBulkPdu::max_repetitions() const {
    return max_repetitions_;
}

void GetBulkPdu::set_non_repeaters(int32_t non_repeaters) {
    non_repeaters_ = non_repeaters;
}

void GetBulkPdu::set_max_repetitions(int32_t max_repetitions) {
    max_repetitions_ = max_repetitions;
}

std::vector<uint8_t> GetBulkPdu::encode() const {
    std::vector<uint8_t> content;
    
    // Encode request ID
    auto request_id_bytes = snmp_utils::encode_integer(request_id_);
    content.insert(content.end(), request_id_bytes.begin(), request_id_bytes.end());
    
    // Encode non-repeaters
    auto non_repeaters_bytes = snmp_utils::encode_integer(non_repeaters_);
    content.insert(content.end(), non_repeaters_bytes.begin(), non_repeaters_bytes.end());
    
    // Encode max-repetitions
    auto max_reps_bytes = snmp_utils::encode_integer(max_repetitions_);
    content.insert(content.end(), max_reps_bytes.begin(), max_reps_bytes.end());
    
    // Encode variable bindings list
    std::vector<uint8_t> vb_list_content;
    for (const auto& vb : variable_bindings_) {
        auto vb_bytes = vb.encode();
        vb_list_content.insert(vb_list_content.end(), vb_bytes.begin(), vb_bytes.end());
    }
    
    auto vb_list_bytes = snmp_utils::encode_sequence(vb_list_content);
    content.insert(content.end(), vb_list_bytes.begin(), vb_list_bytes.end());
    
    // Wrap with PDU type tag
    std::vector<uint8_t> result;
    result.push_back(static_cast<uint8_t>(type_));
    
    auto length_bytes = snmp_utils::encode_length(content.size());
    result.insert(result.end(), length_bytes.begin(), length_bytes.end());
    result.insert(result.end(), content.begin(), content.end());
    
    return result;
}

bool GetBulkPdu::is_valid() const {
    return SnmpPdu::is_valid() && non_repeaters_ >= 0 && max_repetitions_ >= 0;
}

// TrapV1Pdu implementation

TrapV1Pdu::TrapV1Pdu() 
    : SnmpPdu(PduType::TRAP_V1), 
      enterprise_("1.3.6.1.4.1.99999"), 
      agent_addr_(4, 0),
      generic_trap_(GenericTrap::ENTERPRISE_SPECIFIC),
      specific_trap_(0),
      timestamp_(0) {}

const ObjectIdentifier& TrapV1Pdu::enterprise() const {
    return enterprise_;
}

std::vector<uint8_t> TrapV1Pdu::agent_addr() const {
    return agent_addr_;
}

GenericTrap TrapV1Pdu::generic_trap() const {
    return generic_trap_;
}

int32_t TrapV1Pdu::specific_trap() const {
    return specific_trap_;
}

uint32_t TrapV1Pdu::timestamp() const {
    return timestamp_;
}

void TrapV1Pdu::set_enterprise(const ObjectIdentifier& enterprise) {
    enterprise_ = enterprise;
}

void TrapV1Pdu::set_agent_addr(const std::vector<uint8_t>& agent_addr) {
    if (agent_addr.size() == 4) {
        agent_addr_ = agent_addr;
    }
}

void TrapV1Pdu::set_generic_trap(GenericTrap generic_trap) {
    generic_trap_ = generic_trap;
}

void TrapV1Pdu::set_specific_trap(int32_t specific_trap) {
    specific_trap_ = specific_trap;
}

void TrapV1Pdu::set_timestamp(uint32_t timestamp) {
    timestamp_ = timestamp;
}

std::vector<uint8_t> TrapV1Pdu::encode() const {
    std::vector<uint8_t> content;
    
    // Encode enterprise OID
    auto enterprise_bytes = snmp_utils::encode_object_identifier(enterprise_);
    content.insert(content.end(), enterprise_bytes.begin(), enterprise_bytes.end());
    
    // Encode agent address
    auto agent_addr_bytes = snmp_utils::encode_octet_string(agent_addr_);
    agent_addr_bytes[0] = static_cast<uint8_t>(DataType::IP_ADDRESS); // Set correct tag
    content.insert(content.end(), agent_addr_bytes.begin(), agent_addr_bytes.end());
    
    // Encode generic trap
    auto generic_trap_bytes = snmp_utils::encode_integer(static_cast<int32_t>(generic_trap_));
    content.insert(content.end(), generic_trap_bytes.begin(), generic_trap_bytes.end());
    
    // Encode specific trap
    auto specific_trap_bytes = snmp_utils::encode_integer(specific_trap_);
    content.insert(content.end(), specific_trap_bytes.begin(), specific_trap_bytes.end());
    
    // Encode timestamp
    auto timestamp_bytes = snmp_utils::encode_unsigned32(timestamp_);
    timestamp_bytes[0] = static_cast<uint8_t>(DataType::TIME_TICKS); // Set correct tag
    content.insert(content.end(), timestamp_bytes.begin(), timestamp_bytes.end());
    
    // Encode variable bindings list
    std::vector<uint8_t> vb_list_content;
    for (const auto& vb : variable_bindings_) {
        auto vb_bytes = vb.encode();
        vb_list_content.insert(vb_list_content.end(), vb_bytes.begin(), vb_bytes.end());
    }
    
    auto vb_list_bytes = snmp_utils::encode_sequence(vb_list_content);
    content.insert(content.end(), vb_list_bytes.begin(), vb_list_bytes.end());
    
    // Wrap with PDU type tag
    std::vector<uint8_t> result;
    result.push_back(static_cast<uint8_t>(type_));
    
    auto length_bytes = snmp_utils::encode_length(content.size());
    result.insert(result.end(), length_bytes.begin(), length_bytes.end());
    result.insert(result.end(), content.begin(), content.end());
    
    return result;
}

bool TrapV1Pdu::is_valid() const {
    return SnmpPdu::is_valid() && 
           enterprise_.is_valid() && 
           agent_addr_.size() == 4;
}

} // namespace networkquests::snmp