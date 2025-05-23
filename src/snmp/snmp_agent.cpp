#include "networkquests/snmp.hpp"
#include <algorithm>
#include <random>

namespace networkquests::snmp {

// SnmpAgent implementation

SnmpAgent::SnmpAgent(Port port) 
    : bind_addr_(SocketAddress::any_ipv4(port)), running_(false), max_message_size_(65507), active_connections_(0) {
    initialize_system_mib();
}

SnmpAgent::SnmpAgent(const SocketAddress& bind_addr) 
    : bind_addr_(bind_addr), running_(false), max_message_size_(65507), active_connections_(0) {
    initialize_system_mib();
}

Result<void> SnmpAgent::start() {
    if (running_.load()) {
        return make_error("SNMP agent is already running");
    }
    
    // Create UDP socket
    auto socket_result = udp::UdpSocket::create();
    if (!socket_result) {
        return make_error("Failed to create UDP socket: " + socket_result.error());
    }
    
    socket_ = std::make_unique<udp::UdpSocket>(socket_result.value());
    
    // Bind to address
    auto bind_result = socket_->bind(bind_addr_);
    if (!bind_result) {
        socket_.reset();
        return make_error("Failed to bind to address: " + bind_result.error());
    }
    
    // Start server thread
    running_.store(true);
    start_time_ = std::chrono::steady_clock::now();
    server_thread_ = std::thread(&SnmpAgent::server_loop, this);
    
    return make_success();
}

void SnmpAgent::stop() {
    if (!running_.load()) {
        return;
    }
    
    running_.store(false);
    
    if (socket_) {
        socket_->close();
    }
    
    if (server_thread_.joinable()) {
        server_thread_.join();
    }
    
    socket_.reset();
}

bool SnmpAgent::is_running() const {
    return running_.load();
}

void SnmpAgent::add_mib_object(std::unique_ptr<MibObject> object) {
    if (!object) {
        return;
    }
    
    ObjectIdentifier oid = object->oid();
    mib_objects_[oid] = std::move(object);
    sort_mib_oids();
}

void SnmpAgent::add_static_object(const ObjectIdentifier& oid, const SnmpValue& value, DataType type, bool writable) {
    auto mib_obj = std::make_unique<MibObject>(oid, type, writable);
    mib_obj->set_static_value(value);
    add_mib_object(std::move(mib_obj));
}

void SnmpAgent::remove_mib_object(const ObjectIdentifier& oid) {
    auto it = mib_objects_.find(oid);
    if (it != mib_objects_.end()) {
        mib_objects_.erase(it);
        sort_mib_oids();
    }
}

void SnmpAgent::clear_mib() {
    mib_objects_.clear();
    sorted_oids_.clear();
    initialize_system_mib();
}

void SnmpAgent::add_read_community(std::string_view community) {
    read_communities_.insert(std::string(community));
}

void SnmpAgent::add_write_community(std::string_view community) {
    write_communities_.insert(std::string(community));
}

void SnmpAgent::remove_community(std::string_view community) {
    read_communities_.erase(std::string(community));
    write_communities_.erase(std::string(community));
}

bool SnmpAgent::is_valid_read_community(std::string_view community) const {
    return read_communities_.find(std::string(community)) != read_communities_.end();
}

bool SnmpAgent::is_valid_write_community(std::string_view community) const {
    return write_communities_.find(std::string(community)) != write_communities_.end();
}

Result<void> SnmpAgent::send_trap(const SocketAddress& manager_addr, const ObjectIdentifier& trap_oid,
                                 const std::vector<VariableBinding>& variables, std::string_view community) {
    if (!socket_) {
        return make_error("SNMP agent not started");
    }
    
    // Create trap PDU (SNMPv2 style)
    auto trap_pdu = std::make_unique<SnmpPdu>(PduType::TRAP_V2);
    trap_pdu->set_request_id(0); // Traps typically use request ID 0
    
    // Add system uptime
    trap_pdu->add_variable_binding(snmp_utils::oids::SYS_UP_TIME, 
                                  get_uptime(), 
                                  DataType::TIME_TICKS);
    
    // Add trap OID
    trap_pdu->add_variable_binding(ObjectIdentifier("1.3.6.1.6.3.1.1.4.1.0"), 
                                  trap_oid, 
                                  DataType::OBJECT_IDENTIFIER);
    
    // Add additional variables
    for (const auto& vb : variables) {
        trap_pdu->add_variable_binding(vb);
    }
    
    // Create trap message
    SnmpMessage trap_message(SnmpVersion::V2C, community);
    trap_message.set_pdu(std::move(trap_pdu));
    
    // Create temporary socket for sending trap
    auto trap_socket_result = udp::UdpSocket::create();
    if (!trap_socket_result) {
        return make_error("Failed to create trap socket: " + trap_socket_result.error());
    }
    
    auto trap_socket = trap_socket_result.value();
    
    // Send trap
    auto data = trap_message.encode();
    auto send_result = trap_socket.send_to(data, manager_addr);
    if (!send_result) {
        return make_error("Failed to send trap: " + send_result.error());
    }
    
    return make_success();
}

Result<void> SnmpAgent::send_v1_trap(const SocketAddress& manager_addr, const ObjectIdentifier& enterprise,
                                    GenericTrap generic_trap, int32_t specific_trap,
                                    const std::vector<VariableBinding>& variables, std::string_view community) {
    if (!socket_) {
        return make_error("SNMP agent not started");
    }
    
    // Create SNMPv1 trap PDU
    auto trap_pdu = std::make_unique<TrapV1Pdu>();
    trap_pdu->set_enterprise(enterprise);
    trap_pdu->set_generic_trap(generic_trap);
    trap_pdu->set_specific_trap(specific_trap);
    trap_pdu->set_timestamp(get_uptime());
    
    // Set agent address (use local IP)
    auto local_addr = socket_->local_address();
    if (local_addr.is_ipv4()) {
        auto agent_ip = snmp_utils::ip_address_to_bytes(local_addr.ip_string());
        if (!agent_ip.empty()) {
            trap_pdu->set_agent_addr(agent_ip);
        }
    }
    
    // Add variables
    for (const auto& vb : variables) {
        trap_pdu->add_variable_binding(vb);
    }
    
    // Create trap message
    SnmpMessage trap_message(SnmpVersion::V1, community);
    trap_message.set_pdu(std::move(trap_pdu));
    
    // Create temporary socket for sending trap
    auto trap_socket_result = udp::UdpSocket::create();
    if (!trap_socket_result) {
        return make_error("Failed to create trap socket: " + trap_socket_result.error());
    }
    
    auto trap_socket = trap_socket_result.value();
    
    // Send trap
    auto data = trap_message.encode();
    auto send_result = trap_socket.send_to(data, manager_addr);
    if (!send_result) {
        return make_error("Failed to send trap: " + send_result.error());
    }
    
    return make_success();
}

void SnmpAgent::set_trap_handler(TrapHandler handler) {
    trap_handler_ = std::move(handler);
}

void SnmpAgent::set_system_description(std::string_view description) {
    system_description_ = description;
}

void SnmpAgent::set_system_contact(std::string_view contact) {
    system_contact_ = contact;
}

void SnmpAgent::set_system_name(std::string_view name) {
    system_name_ = name;
}

void SnmpAgent::set_system_location(std::string_view location) {
    system_location_ = location;
}

void SnmpAgent::set_max_message_size(size_t size) {
    max_message_size_ = size;
}

SocketAddress SnmpAgent::local_address() const {
    return socket_ ? socket_->local_address() : SocketAddress{};
}

size_t SnmpAgent::active_connections() const {
    return active_connections_.load();
}

std::vector<ObjectIdentifier> SnmpAgent::get_mib_oids() const {
    return sorted_oids_;
}

void SnmpAgent::server_loop() {
    while (running_.load()) {
        if (!socket_) {
            break;
        }
        
        // Receive request
        auto receive_result = socket_->receive_from(std::chrono::milliseconds(1000));
        if (!receive_result) {
            continue; // Timeout or error, continue listening
        }
        
        const auto& [data, client_addr] = receive_result.value();
        
        // Handle request in separate context (could be threaded for performance)
        try {
            handle_request(data, client_addr);
        } catch (const std::exception& e) {
            // Log error but continue serving
            LOG_ERROR("SNMP Agent", "Error handling request: " + std::string(e.what()));
        }
    }
}

void SnmpAgent::handle_request(const std::vector<uint8_t>& data, const SocketAddress& client_addr) {
    active_connections_.fetch_add(1);
    
    // Parse SNMP message
    auto message_result = SnmpMessage::decode(data);
    if (!message_result) {
        active_connections_.fetch_sub(1);
        return; // Invalid message, ignore
    }
    
    const auto& request_msg = message_result.value();
    const auto* request_pdu = request_msg.pdu();
    
    if (!request_pdu) {
        active_connections_.fetch_sub(1);
        return; // No PDU, ignore
    }
    
    // Handle traps
    if (request_pdu->type() == PduType::TRAP_V1 || request_pdu->type() == PduType::TRAP_V2) {
        process_trap(request_msg, client_addr);
        active_connections_.fetch_sub(1);
        return;
    }
    
    // Process request and generate response
    std::unique_ptr<SnmpPdu> response_pdu;
    
    switch (request_pdu->type()) {
        case PduType::GET_REQUEST:
            response_pdu = process_get_request(request_pdu, request_msg.community());
            break;
            
        case PduType::GET_NEXT_REQUEST:
            response_pdu = process_get_next_request(request_pdu, request_msg.community());
            break;
            
        case PduType::GET_BULK_REQUEST:
            if (const auto* bulk_pdu = dynamic_cast<const GetBulkPdu*>(request_pdu)) {
                response_pdu = process_get_bulk_request(bulk_pdu, request_msg.community());
            }
            break;
            
        case PduType::SET_REQUEST:
            response_pdu = process_set_request(request_pdu, request_msg.community());
            break;
            
        case PduType::INFORM_REQUEST:
            // Send acknowledge response for INFORM
            response_pdu = std::make_unique<SnmpPdu>(PduType::GET_RESPONSE);
            response_pdu->set_request_id(request_pdu->request_id());
            response_pdu->set_error_status(ErrorStatus::NO_ERROR);
            response_pdu->set_error_index(0);
            // Copy variable bindings
            for (const auto& vb : request_pdu->variable_bindings()) {
                response_pdu->add_variable_binding(vb);
            }
            break;
            
        default:
            // Unsupported PDU type
            response_pdu = std::make_unique<SnmpPdu>(PduType::GET_RESPONSE);
            response_pdu->set_request_id(request_pdu->request_id());
            response_pdu->set_error_status(ErrorStatus::GEN_ERR);
            response_pdu->set_error_index(0);
            break;
    }
    
    // Send response
    if (response_pdu) {
        SnmpMessage response_msg(request_msg.version(), request_msg.community());
        response_msg.set_pdu(std::move(response_pdu));
        
        auto response_data = response_msg.encode();
        if (!response_data.empty()) {
            socket_->send_to(response_data, client_addr);
        }
    }
    
    active_connections_.fetch_sub(1);
}

std::unique_ptr<SnmpPdu> SnmpAgent::process_get_request(const SnmpPdu* request_pdu, std::string_view community) {
    auto response_pdu = std::make_unique<SnmpPdu>(PduType::GET_RESPONSE);
    response_pdu->set_request_id(request_pdu->request_id());
    
    // Check read community
    if (!is_valid_read_community(community)) {
        response_pdu->set_error_status(ErrorStatus::NO_ACCESS);
        response_pdu->set_error_index(0);
        return response_pdu;
    }
    
    const auto& request_vbs = request_pdu->variable_bindings();
    
    for (size_t i = 0; i < request_vbs.size(); ++i) {
        const auto& request_vb = request_vbs[i];
        auto* mib_obj = find_mib_object(request_vb.oid());
        
        if (!mib_obj) {
            // Object not found
            response_pdu->set_error_status(ErrorStatus::NO_SUCH_NAME);
            response_pdu->set_error_index(static_cast<int32_t>(i + 1));
            return response_pdu;
        }
        
        // Get value
        auto value_result = mib_obj->get_value();
        if (!value_result) {
            response_pdu->set_error_status(ErrorStatus::GEN_ERR);
            response_pdu->set_error_index(static_cast<int32_t>(i + 1));
            return response_pdu;
        }
        
        response_pdu->add_variable_binding(request_vb.oid(), value_result.value(), mib_obj->type());
    }
    
    response_pdu->set_error_status(ErrorStatus::NO_ERROR);
    response_pdu->set_error_index(0);
    return response_pdu;
}

std::unique_ptr<SnmpPdu> SnmpAgent::process_get_next_request(const SnmpPdu* request_pdu, std::string_view community) {
    auto response_pdu = std::make_unique<SnmpPdu>(PduType::GET_RESPONSE);
    response_pdu->set_request_id(request_pdu->request_id());
    
    // Check read community
    if (!is_valid_read_community(community)) {
        response_pdu->set_error_status(ErrorStatus::NO_ACCESS);
        response_pdu->set_error_index(0);
        return response_pdu;
    }
    
    const auto& request_vbs = request_pdu->variable_bindings();
    
    for (size_t i = 0; i < request_vbs.size(); ++i) {
        const auto& request_vb = request_vbs[i];
        ObjectIdentifier next_oid = find_next_oid(request_vb.oid());
        
        if (next_oid.components().empty()) {
            // End of MIB view
            VariableBinding end_vb;
            end_vb.set_oid(request_vb.oid());
            end_vb.set_exception(DataType::END_OF_MIB_VIEW);
            response_pdu->add_variable_binding(end_vb);
            continue;
        }
        
        auto* mib_obj = find_mib_object(next_oid);
        if (!mib_obj) {
            // Should not happen if find_next_oid works correctly
            response_pdu->set_error_status(ErrorStatus::GEN_ERR);
            response_pdu->set_error_index(static_cast<int32_t>(i + 1));
            return response_pdu;
        }
        
        // Get value
        auto value_result = mib_obj->get_value();
        if (!value_result) {
            response_pdu->set_error_status(ErrorStatus::GEN_ERR);
            response_pdu->set_error_index(static_cast<int32_t>(i + 1));
            return response_pdu;
        }
        
        response_pdu->add_variable_binding(next_oid, value_result.value(), mib_obj->type());
    }
    
    response_pdu->set_error_status(ErrorStatus::NO_ERROR);
    response_pdu->set_error_index(0);
    return response_pdu;
}

std::unique_ptr<SnmpPdu> SnmpAgent::process_get_bulk_request(const GetBulkPdu* request_pdu, std::string_view community) {
    auto response_pdu = std::make_unique<SnmpPdu>(PduType::GET_RESPONSE);
    response_pdu->set_request_id(request_pdu->request_id());
    
    // Check read community
    if (!is_valid_read_community(community)) {
        response_pdu->set_error_status(ErrorStatus::NO_ACCESS);
        response_pdu->set_error_index(0);
        return response_pdu;
    }
    
    const auto& request_vbs = request_pdu->variable_bindings();
    int32_t non_repeaters = request_pdu->non_repeaters();
    int32_t max_repetitions = request_pdu->max_repetitions();
    
    // Process non-repeating variables
    for (int32_t i = 0; i < non_repeaters && i < static_cast<int32_t>(request_vbs.size()); ++i) {
        const auto& request_vb = request_vbs[i];
        ObjectIdentifier next_oid = find_next_oid(request_vb.oid());
        
        if (next_oid.components().empty()) {
            VariableBinding end_vb;
            end_vb.set_oid(request_vb.oid());
            end_vb.set_exception(DataType::END_OF_MIB_VIEW);
            response_pdu->add_variable_binding(end_vb);
            continue;
        }
        
        auto* mib_obj = find_mib_object(next_oid);
        if (mib_obj) {
            auto value_result = mib_obj->get_value();
            if (value_result) {
                response_pdu->add_variable_binding(next_oid, value_result.value(), mib_obj->type());
            }
        }
    }
    
    // Process repeating variables
    std::vector<ObjectIdentifier> current_oids;
    for (int32_t i = non_repeaters; i < static_cast<int32_t>(request_vbs.size()); ++i) {
        current_oids.push_back(request_vbs[i].oid());
    }
    
    for (int32_t rep = 0; rep < max_repetitions && !current_oids.empty(); ++rep) {
        std::vector<ObjectIdentifier> next_oids;
        
        for (const auto& current_oid : current_oids) {
            ObjectIdentifier next_oid = find_next_oid(current_oid);
            
            if (next_oid.components().empty()) {
                VariableBinding end_vb;
                end_vb.set_oid(current_oid);
                end_vb.set_exception(DataType::END_OF_MIB_VIEW);
                response_pdu->add_variable_binding(end_vb);
                continue;
            }
            
            auto* mib_obj = find_mib_object(next_oid);
            if (mib_obj) {
                auto value_result = mib_obj->get_value();
                if (value_result) {
                    response_pdu->add_variable_binding(next_oid, value_result.value(), mib_obj->type());
                    next_oids.push_back(next_oid);
                }
            }
        }
        
        current_oids = std::move(next_oids);
    }
    
    response_pdu->set_error_status(ErrorStatus::NO_ERROR);
    response_pdu->set_error_index(0);
    return response_pdu;
}

std::unique_ptr<SnmpPdu> SnmpAgent::process_set_request(const SnmpPdu* request_pdu, std::string_view community) {
    auto response_pdu = std::make_unique<SnmpPdu>(PduType::GET_RESPONSE);
    response_pdu->set_request_id(request_pdu->request_id());
    
    // Check write community
    if (!is_valid_write_community(community)) {
        response_pdu->set_error_status(ErrorStatus::NO_ACCESS);
        response_pdu->set_error_index(0);
        return response_pdu;
    }
    
    const auto& request_vbs = request_pdu->variable_bindings();
    
    // First pass: validate all objects and values
    for (size_t i = 0; i < request_vbs.size(); ++i) {
        const auto& request_vb = request_vbs[i];
        auto* mib_obj = find_mib_object(request_vb.oid());
        
        if (!mib_obj) {
            response_pdu->set_error_status(ErrorStatus::NO_SUCH_NAME);
            response_pdu->set_error_index(static_cast<int32_t>(i + 1));
            return response_pdu;
        }
        
        if (!mib_obj->is_writable()) {
            response_pdu->set_error_status(ErrorStatus::read_ONLY);
            response_pdu->set_error_index(static_cast<int32_t>(i + 1));
            return response_pdu;
        }
        
        if (!snmp_utils::is_compatible_type(request_vb.value(), mib_obj->type())) {
            response_pdu->set_error_status(ErrorStatus::WRONG_TYPE);
            response_pdu->set_error_index(static_cast<int32_t>(i + 1));
            return response_pdu;
        }
    }
    
    // Second pass: perform all sets
    for (size_t i = 0; i < request_vbs.size(); ++i) {
        const auto& request_vb = request_vbs[i];
        auto* mib_obj = find_mib_object(request_vb.oid());
        
        auto set_result = mib_obj->set_value(request_vb.value());
        if (!set_result) {
            response_pdu->set_error_status(ErrorStatus::GEN_ERR);
            response_pdu->set_error_index(static_cast<int32_t>(i + 1));
            return response_pdu;
        }
        
        // Add to response with new value
        auto get_result = mib_obj->get_value();
        if (get_result) {
            response_pdu->add_variable_binding(request_vb.oid(), get_result.value(), mib_obj->type());
        } else {
            response_pdu->add_variable_binding(request_vb.oid(), request_vb.value(), request_vb.type());
        }
    }
    
    response_pdu->set_error_status(ErrorStatus::NO_ERROR);
    response_pdu->set_error_index(0);
    return response_pdu;
}

void SnmpAgent::process_trap(const SnmpMessage& message, const SocketAddress& sender_addr) {
    if (trap_handler_) {
        trap_handler_(message, sender_addr);
    }
}

MibObject* SnmpAgent::find_mib_object(const ObjectIdentifier& oid) {
    auto it = mib_objects_.find(oid);
    return it != mib_objects_.end() ? it->second.get() : nullptr;
}

ObjectIdentifier SnmpAgent::find_next_oid(const ObjectIdentifier& oid) {
    // Find the first OID that is lexicographically greater than the given OID
    auto it = std::upper_bound(sorted_oids_.begin(), sorted_oids_.end(), oid);
    return it != sorted_oids_.end() ? *it : ObjectIdentifier{};
}

void SnmpAgent::sort_mib_oids() {
    sorted_oids_.clear();
    sorted_oids_.reserve(mib_objects_.size());
    
    for (const auto& [oid, obj] : mib_objects_) {
        sorted_oids_.push_back(oid);
    }
    
    std::sort(sorted_oids_.begin(), sorted_oids_.end());
}

void SnmpAgent::initialize_system_mib() {
    // Add default read communities
    add_read_community("public");
    add_write_community("private");
    
    // Initialize basic system MIB objects
    system_description_ = "NetworkQuests SNMP Agent";
    system_contact_ = "admin@networkquests.org";
    system_name_ = "NetworkQuests-Agent";
    system_location_ = "Unknown";
    
    // Add system description
    auto sys_descr = std::make_unique<MibObject>(snmp_utils::oids::SYS_DESCR, DataType::OCTET_STRING, false);
    sys_descr->set_get_handler([this]() -> Result<SnmpValue> {
        return SnmpValue(system_description_);
    });
    add_mib_object(std::move(sys_descr));
    
    // Add system object ID
    auto sys_obj_id = std::make_unique<MibObject>(snmp_utils::oids::SYS_OBJECT_ID, DataType::OBJECT_IDENTIFIER, false);
    sys_obj_id->set_get_handler([this]() -> Result<SnmpValue> {
        return SnmpValue(snmp_utils::oids::NETWORKQUESTS);
    });
    add_mib_object(std::move(sys_obj_id));
    
    // Add system uptime
    auto sys_uptime = std::make_unique<MibObject>(snmp_utils::oids::SYS_UP_TIME, DataType::TIME_TICKS, false);
    sys_uptime->set_get_handler([this]() -> Result<SnmpValue> {
        return SnmpValue(get_uptime());
    });
    add_mib_object(std::move(sys_uptime));
    
    // Add system contact
    auto sys_contact = std::make_unique<MibObject>(snmp_utils::oids::SYS_CONTACT, DataType::OCTET_STRING, true);
    sys_contact->set_get_handler([this]() -> Result<SnmpValue> {
        return SnmpValue(system_contact_);
    });
    sys_contact->set_set_handler([this](const SnmpValue& value) -> Result<void> {
        if (std::holds_alternative<std::string>(value)) {
            system_contact_ = std::get<std::string>(value);
            return make_success();
        }
        return make_error("Invalid value type for system contact");
    });
    add_mib_object(std::move(sys_contact));
    
    // Add system name
    auto sys_name = std::make_unique<MibObject>(snmp_utils::oids::SYS_NAME, DataType::OCTET_STRING, true);
    sys_name->set_get_handler([this]() -> Result<SnmpValue> {
        return SnmpValue(system_name_);
    });
    sys_name->set_set_handler([this](const SnmpValue& value) -> Result<void> {
        if (std::holds_alternative<std::string>(value)) {
            system_name_ = std::get<std::string>(value);
            return make_success();
        }
        return make_error("Invalid value type for system name");
    });
    add_mib_object(std::move(sys_name));
    
    // Add system location
    auto sys_location = std::make_unique<MibObject>(snmp_utils::oids::SYS_LOCATION, DataType::OCTET_STRING, true);
    sys_location->set_get_handler([this]() -> Result<SnmpValue> {
        return SnmpValue(system_location_);
    });
    sys_location->set_set_handler([this](const SnmpValue& value) -> Result<void> {
        if (std::holds_alternative<std::string>(value)) {
            system_location_ = std::get<std::string>(value);
            return make_success();
        }
        return make_error("Invalid value type for system location");
    });
    add_mib_object(std::move(sys_location));
    
    // Add system services (indicating network management capabilities)
    auto sys_services = std::make_unique<MibObject>(snmp_utils::oids::SYS_SERVICES, DataType::INTEGER, false);
    sys_services->set_get_handler([this]() -> Result<SnmpValue> {
        return SnmpValue(static_cast<int32_t>(72)); // Layer 3 (4) + Layer 7 (64) + Application (8)
    });
    add_mib_object(std::move(sys_services));
}

uint32_t SnmpAgent::get_uptime() const {
    if (!running_.load()) {
        return 0;
    }
    
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time_);
    return static_cast<uint32_t>(duration.count() / 10); // Convert to centiseconds
}

} // namespace networkquests::snmp