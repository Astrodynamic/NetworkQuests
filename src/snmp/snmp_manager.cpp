#include "networkquests/snmp.hpp"
#include <random>

namespace networkquests::snmp {

// SnmpConnection implementation

SnmpConnection::SnmpConnection(udp::UdpSocket socket) 
    : socket_(std::move(socket)) {}

SocketAddress SnmpConnection::local_address() const {
    return socket_.local_address();
}

SocketAddress SnmpConnection::remote_address() const {
    return socket_.remote_address();
}

bool SnmpConnection::is_connected() const {
    return socket_.is_connected();
}

Result<void> SnmpConnection::send_message(const SnmpMessage& message) {
    if (!message.is_valid()) {
        return make_error("Invalid SNMP message");
    }
    
    auto data = message.encode();
    if (data.empty()) {
        return make_error("Failed to encode SNMP message");
    }
    
    auto result = socket_.send(data);
    if (!result) {
        return make_error("Failed to send SNMP message: " + result.error());
    }
    
    return make_success();
}

Result<SnmpMessage> SnmpConnection::receive_message(std::chrono::milliseconds timeout) {
    auto data_result = socket_.receive(timeout);
    if (!data_result) {
        return make_error("Failed to receive SNMP message: " + data_result.error());
    }
    
    auto message_result = SnmpMessage::decode(data_result.value());
    if (!message_result) {
        return make_error("Failed to decode SNMP message: " + message_result.error());
    }
    
    return message_result.value();
}

void SnmpConnection::close() {
    socket_.close();
}

// SnmpManager implementation

SnmpManager::SnmpManager() 
    : timeout_(std::chrono::milliseconds(5000)), retries_(3), version_(SnmpVersion::V2C), next_request_id_(1) {}

SnmpManager::SnmpManager(std::chrono::milliseconds timeout) 
    : timeout_(timeout), retries_(3), version_(SnmpVersion::V2C), next_request_id_(1) {}

Result<void> SnmpManager::connect(const SocketAddress& agent_addr) {
    if (connection_ && connection_->is_connected()) {
        connection_->close();
    }
    
    // Create UDP socket for SNMP communication
    auto socket_result = udp::UdpSocket::create();
    if (!socket_result) {
        return make_error("Failed to create UDP socket: " + socket_result.error());
    }
    
    auto socket = socket_result.value();
    
    // Connect to agent
    auto connect_result = socket.connect(agent_addr);
    if (!connect_result) {
        return make_error("Failed to connect to SNMP agent: " + connect_result.error());
    }
    
    connection_ = std::make_unique<SnmpConnection>(std::move(socket));
    return make_success();
}

void SnmpManager::disconnect() {
    if (connection_) {
        connection_->close();
        connection_.reset();
    }
}

bool SnmpManager::is_connected() const {
    return connection_ && connection_->is_connected();
}

Result<SnmpValue> SnmpManager::get(const ObjectIdentifier& oid, std::string_view community) {
    if (!is_connected()) {
        return make_error("Not connected to SNMP agent");
    }
    
    // Create GET request
    auto request_pdu = std::make_unique<SnmpPdu>(PduType::GET_REQUEST);
    request_pdu->set_request_id(generate_request_id());
    request_pdu->add_variable_binding(oid, std::monostate{}, DataType::NULL_VALUE);
    
    SnmpMessage request(version_, community);
    request.set_pdu(std::move(request_pdu));
    
    // Send request and wait for response
    auto response_result = send_request_and_wait_response(request);
    if (!response_result) {
        return make_error("Failed to get SNMP response: " + response_result.error());
    }
    
    const auto& response = response_result.value();
    const auto* response_pdu = response.pdu();
    
    if (!response_pdu || response_pdu->error_status() != ErrorStatus::NO_ERROR) {
        return make_error("SNMP error: " + std::string(snmp_utils::error_status_to_string(
            response_pdu ? response_pdu->error_status() : ErrorStatus::GEN_ERR)));
    }
    
    const auto& vb_list = response_pdu->variable_bindings();
    if (vb_list.empty()) {
        return make_error("Empty variable binding list in response");
    }
    
    const auto& vb = vb_list[0];
    if (vb.is_exception()) {
        return make_error("Exception in response: " + std::string(snmp_utils::data_type_to_string(vb.type())));
    }
    
    if (vb.oid() != oid) {
        return make_error("Response OID does not match request OID");
    }
    
    return vb.value();
}

Result<std::vector<VariableBinding>> SnmpManager::get_multiple(const std::vector<ObjectIdentifier>& oids, 
                                                               std::string_view community) {
    if (!is_connected()) {
        return make_error("Not connected to SNMP agent");
    }
    
    if (oids.empty()) {
        return std::vector<VariableBinding>{};
    }
    
    // Create GET request with multiple OIDs
    auto request_pdu = std::make_unique<SnmpPdu>(PduType::GET_REQUEST);
    request_pdu->set_request_id(generate_request_id());
    
    for (const auto& oid : oids) {
        request_pdu->add_variable_binding(oid, std::monostate{}, DataType::NULL_VALUE);
    }
    
    SnmpMessage request(version_, community);
    request.set_pdu(std::move(request_pdu));
    
    // Send request and wait for response
    auto response_result = send_request_and_wait_response(request);
    if (!response_result) {
        return make_error("Failed to get SNMP response: " + response_result.error());
    }
    
    const auto& response = response_result.value();
    const auto* response_pdu = response.pdu();
    
    if (!response_pdu || response_pdu->error_status() != ErrorStatus::NO_ERROR) {
        return make_error("SNMP error: " + std::string(snmp_utils::error_status_to_string(
            response_pdu ? response_pdu->error_status() : ErrorStatus::GEN_ERR)));
    }
    
    return response_pdu->variable_bindings();
}

Result<VariableBinding> SnmpManager::get_next(const ObjectIdentifier& oid, std::string_view community) {
    if (!is_connected()) {
        return make_error("Not connected to SNMP agent");
    }
    
    // Create GETNEXT request
    auto request_pdu = std::make_unique<SnmpPdu>(PduType::GET_NEXT_REQUEST);
    request_pdu->set_request_id(generate_request_id());
    request_pdu->add_variable_binding(oid, std::monostate{}, DataType::NULL_VALUE);
    
    SnmpMessage request(version_, community);
    request.set_pdu(std::move(request_pdu));
    
    // Send request and wait for response
    auto response_result = send_request_and_wait_response(request);
    if (!response_result) {
        return make_error("Failed to get SNMP response: " + response_result.error());
    }
    
    const auto& response = response_result.value();
    const auto* response_pdu = response.pdu();
    
    if (!response_pdu || response_pdu->error_status() != ErrorStatus::NO_ERROR) {
        return make_error("SNMP error: " + std::string(snmp_utils::error_status_to_string(
            response_pdu ? response_pdu->error_status() : ErrorStatus::GEN_ERR)));
    }
    
    const auto& vb_list = response_pdu->variable_bindings();
    if (vb_list.empty()) {
        return make_error("Empty variable binding list in response");
    }
    
    return vb_list[0];
}

Result<void> SnmpManager::set(const ObjectIdentifier& oid, const SnmpValue& value, DataType type,
                             std::string_view community) {
    if (!is_connected()) {
        return make_error("Not connected to SNMP agent");
    }
    
    if (!snmp_utils::is_compatible_type(value, type)) {
        return make_error("Value type incompatible with specified data type");
    }
    
    // Create SET request
    auto request_pdu = std::make_unique<SnmpPdu>(PduType::SET_REQUEST);
    request_pdu->set_request_id(generate_request_id());
    request_pdu->add_variable_binding(oid, value, type);
    
    SnmpMessage request(version_, community);
    request.set_pdu(std::move(request_pdu));
    
    // Send request and wait for response
    auto response_result = send_request_and_wait_response(request);
    if (!response_result) {
        return make_error("Failed to get SNMP response: " + response_result.error());
    }
    
    const auto& response = response_result.value();
    const auto* response_pdu = response.pdu();
    
    if (!response_pdu || response_pdu->error_status() != ErrorStatus::NO_ERROR) {
        return make_error("SNMP error: " + std::string(snmp_utils::error_status_to_string(
            response_pdu ? response_pdu->error_status() : ErrorStatus::GEN_ERR)));
    }
    
    return make_success();
}

Result<std::vector<VariableBinding>> SnmpManager::get_bulk(const std::vector<ObjectIdentifier>& oids,
                                                           int32_t non_repeaters, int32_t max_repetitions,
                                                           std::string_view community) {
    if (!is_connected()) {
        return make_error("Not connected to SNMP agent");
    }
    
    if (version_ == SnmpVersion::V1) {
        return make_error("GETBULK not supported in SNMPv1");
    }
    
    if (oids.empty()) {
        return std::vector<VariableBinding>{};
    }
    
    // Create GETBULK request
    auto request_pdu = std::make_unique<GetBulkPdu>();
    request_pdu->set_request_id(generate_request_id());
    request_pdu->set_non_repeaters(non_repeaters);
    request_pdu->set_max_repetitions(max_repetitions);
    
    for (const auto& oid : oids) {
        request_pdu->add_variable_binding(oid, std::monostate{}, DataType::NULL_VALUE);
    }
    
    SnmpMessage request(version_, community);
    request.set_pdu(std::move(request_pdu));
    
    // Send request and wait for response
    auto response_result = send_request_and_wait_response(request);
    if (!response_result) {
        return make_error("Failed to get SNMP response: " + response_result.error());
    }
    
    const auto& response = response_result.value();
    const auto* response_pdu = response.pdu();
    
    if (!response_pdu || response_pdu->error_status() != ErrorStatus::NO_ERROR) {
        return make_error("SNMP error: " + std::string(snmp_utils::error_status_to_string(
            response_pdu ? response_pdu->error_status() : ErrorStatus::GEN_ERR)));
    }
    
    return response_pdu->variable_bindings();
}

Result<void> SnmpManager::inform(const ObjectIdentifier& oid, const SnmpValue& value, DataType type,
                                std::string_view community) {
    if (!is_connected()) {
        return make_error("Not connected to SNMP agent");
    }
    
    if (version_ == SnmpVersion::V1) {
        return make_error("INFORM not supported in SNMPv1");
    }
    
    // Create INFORM request
    auto request_pdu = std::make_unique<SnmpPdu>(PduType::INFORM_REQUEST);
    request_pdu->set_request_id(generate_request_id());
    
    // Add system uptime
    request_pdu->add_variable_binding(snmp_utils::oids::SYS_UP_TIME, 
                                     snmp_utils::get_system_uptime_ticks(), 
                                     DataType::TIME_TICKS);
    
    // Add trap OID
    request_pdu->add_variable_binding(ObjectIdentifier("1.3.6.1.6.3.1.1.4.1.0"), 
                                     oid, 
                                     DataType::OBJECT_IDENTIFIER);
    
    // Add user data
    if (!std::holds_alternative<std::monostate>(value)) {
        request_pdu->add_variable_binding(oid, value, type);
    }
    
    SnmpMessage request(version_, community);
    request.set_pdu(std::move(request_pdu));
    
    // Send request and wait for response
    auto response_result = send_request_and_wait_response(request);
    if (!response_result) {
        return make_error("Failed to get SNMP response: " + response_result.error());
    }
    
    const auto& response = response_result.value();
    const auto* response_pdu = response.pdu();
    
    if (!response_pdu || response_pdu->error_status() != ErrorStatus::NO_ERROR) {
        return make_error("SNMP error: " + std::string(snmp_utils::error_status_to_string(
            response_pdu ? response_pdu->error_status() : ErrorStatus::GEN_ERR)));
    }
    
    return make_success();
}

Result<std::vector<VariableBinding>> SnmpManager::walk_table(const ObjectIdentifier& table_oid,
                                                             std::string_view community) {
    if (!is_connected()) {
        return make_error("Not connected to SNMP agent");
    }
    
    std::vector<VariableBinding> results;
    ObjectIdentifier current_oid = table_oid;
    
    // Walk the table using GETNEXT operations
    while (true) {
        auto next_result = get_next(current_oid, community);
        if (!next_result) {
            break; // End of table or error
        }
        
        const auto& vb = next_result.value();
        
        // Check if we're still in the table
        if (!vb.oid().is_child_of(table_oid)) {
            break; // Left the table
        }
        
        // Check for exception values (end of MIB view)
        if (vb.is_exception()) {
            break;
        }
        
        results.push_back(vb);
        current_oid = vb.oid();
        
        // Prevent infinite loops
        if (results.size() > 10000) {
            return make_error("Table walk exceeded maximum entries (possible loop)");
        }
    }
    
    return results;
}

void SnmpManager::set_timeout(std::chrono::milliseconds timeout) {
    timeout_ = timeout;
}

void SnmpManager::set_retries(int retries) {
    retries_ = std::max(0, retries);
}

void SnmpManager::set_version(SnmpVersion version) {
    version_ = version;
}

SocketAddress SnmpManager::local_address() const {
    return connection_ ? connection_->local_address() : SocketAddress{};
}

SocketAddress SnmpManager::remote_address() const {
    return connection_ ? connection_->remote_address() : SocketAddress{};
}

int32_t SnmpManager::generate_request_id() {
    return next_request_id_.fetch_add(1);
}

Result<SnmpMessage> SnmpManager::send_request_and_wait_response(const SnmpMessage& request) {
    if (!connection_) {
        return make_error("No connection available");
    }
    
    const auto* request_pdu = request.pdu();
    if (!request_pdu) {
        return make_error("Invalid request PDU");
    }
    
    int32_t request_id = request_pdu->request_id();
    
    // Retry loop
    for (int attempt = 0; attempt <= retries_; ++attempt) {
        // Send request
        auto send_result = connection_->send_message(request);
        if (!send_result) {
            if (attempt == retries_) {
                return make_error("Failed to send request after " + std::to_string(retries_ + 1) + " attempts");
            }
            continue;
        }
        
        // Wait for response
        auto response_result = connection_->receive_message(timeout_);
        if (!response_result) {
            if (attempt == retries_) {
                return make_error("Failed to receive response after " + std::to_string(retries_ + 1) + " attempts");
            }
            continue;
        }
        
        const auto& response = response_result.value();
        const auto* response_pdu = response.pdu();
        
        // Verify response matches request
        if (response_pdu && response_pdu->request_id() == request_id) {
            return response;
        }
        
        // Response doesn't match, try again if retries left
        if (attempt == retries_) {
            return make_error("Response request ID does not match after " + std::to_string(retries_ + 1) + " attempts");
        }
    }
    
    return make_error("Unexpected error in request/response loop");
}

} // namespace networkquests::snmp