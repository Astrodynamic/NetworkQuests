#include "networkquests/dns.hpp"
#include "networkquests/udp.hpp"
#include "networkquests/tcp.hpp"
#include <algorithm>
#include <sstream>

namespace NetworkQuests::Dns {

// DnsZone implementation
DnsZone::DnsZone(const std::string& origin) : origin_(Utils::normalize_domain_name(origin)) {}

void DnsZone::add_record(const DnsResourceRecord& record) {
    std::unique_lock<std::shared_mutex> lock(records_mutex_);
    records_.push_back(record);
}

void DnsZone::remove_record(const std::string& name, DnsType type) {
    std::unique_lock<std::shared_mutex> lock(records_mutex_);
    auto normalized_name = normalize_name(name);
    
    records_.erase(
        std::remove_if(records_.begin(), records_.end(),
            [&normalized_name, type](const DnsResourceRecord& rr) {
                return normalize_name(rr.get_name()) == normalized_name && rr.get_type() == type;
            }),
        records_.end());
}

std::vector<DnsResourceRecord> DnsZone::find_records(const std::string& name, DnsType type, DnsClass qclass) const {
    std::shared_lock<std::shared_mutex> lock(records_mutex_);
    std::vector<DnsResourceRecord> matches;
    auto normalized_name = normalize_name(name);
    
    for (const auto& rr : records_) {
        if (normalize_name(rr.get_name()) == normalized_name && 
            (type == DnsType::ANY || rr.get_type() == type) &&
            (qclass == DnsClass::ANY || rr.get_class() == qclass)) {
            matches.push_back(rr);
        }
    }
    
    return matches;
}

std::vector<DnsResourceRecord> DnsZone::find_ns_records() const {
    return find_records(origin_, DnsType::NS, DnsClass::IN);
}

std::vector<DnsResourceRecord> DnsZone::find_soa_records() const {
    return find_records(origin_, DnsType::SOA, DnsClass::IN);
}

std::string DnsZone::normalize_name(const std::string& name) const {
    return Utils::normalize_domain_name(name);
}

Result<void> DnsZone::load_from_string(const std::string& zone_data) {
    // Simplified zone file parsing
    std::istringstream iss(zone_data);
    std::string line;
    
    while (std::getline(iss, line)) {
        // Skip comments and empty lines
        if (line.empty() || line[0] == ';') continue;
        
        // Simple parsing - this is very basic
        std::istringstream line_stream(line);
        std::string name, ttl_str, class_str, type_str, rdata_str;
        
        if (!(line_stream >> name >> ttl_str >> class_str >> type_str)) {
            continue;  // Skip malformed lines
        }
        
        // Read rest of line as rdata
        std::getline(line_stream, rdata_str);
        if (!rdata_str.empty() && rdata_str[0] == ' ') {
            rdata_str = rdata_str.substr(1);  // Remove leading space
        }
        
        try {
            uint32_t ttl = std::stoul(ttl_str);
            auto type_result = Utils::string_to_dns_type(type_str);
            auto class_result = Utils::string_to_dns_class(class_str);
            
            if (!type_result.has_value() || !class_result.has_value()) {
                continue;  // Skip unknown types/classes
            }
            
            // Create appropriate record based on type
            std::unique_ptr<DnsRData> rdata;
            switch (type_result.value()) {
                case DnsType::A:
                    if (Utils::is_valid_ipv4(rdata_str)) {
                        rdata = std::make_unique<ARecord>(rdata_str);
                    }
                    break;
                case DnsType::AAAA:
                    if (Utils::is_valid_ipv6(rdata_str)) {
                        rdata = std::make_unique<AAAARecord>(rdata_str);
                    }
                    break;
                case DnsType::NS:
                    rdata = std::make_unique<NSRecord>(rdata_str);
                    break;
                case DnsType::CNAME:
                    rdata = std::make_unique<CNAMERecord>(rdata_str);
                    break;
                case DnsType::PTR:
                    rdata = std::make_unique<PTRRecord>(rdata_str);
                    break;
                case DnsType::TXT:
                    rdata = std::make_unique<TXTRecord>(rdata_str);
                    break;
                default:
                    // Skip unsupported types
                    continue;
            }
            
            if (rdata) {
                DnsResourceRecord record(name, type_result.value(), class_result.value(), ttl, std::move(rdata));
                add_record(record);
            }
        } catch (const std::exception&) {
            // Skip malformed records
            continue;
        }
    }
    
    return {};
}

// DnsServer implementation
DnsServer::DnsServer(uint16_t port)
    : port_(port)
    , running_(false)
    , should_stop_(false)
    , recursion_enabled_(false)
    , upstream_server_("8.8.8.8")
    , upstream_port_(DNS_PORT) {}

DnsServer::~DnsServer() {
    stop();
}

Result<void> DnsServer::start() {
    if (running_.load()) {
        return make_error(NetworkError::ALREADY_CONNECTED, "DNS server already running");
    }
    
    // Create UDP socket
    auto udp_result = UdpSocket::create();
    if (!udp_result.has_value()) {
        return make_error(udp_result.error());
    }
    udp_socket_ = std::move(udp_result.value());
    
    // Bind UDP socket
    auto bind_result = udp_socket_->bind(port_);
    if (!bind_result.has_value()) {
        return make_error(bind_result.error());
    }
    
    // Create TCP server
    auto tcp_result = TcpServer::create(port_);
    if (!tcp_result.has_value()) {
        return make_error(tcp_result.error());
    }
    tcp_server_ = std::move(tcp_result.value());
    
    // Start TCP server listening
    auto listen_result = tcp_server_->start_listening();
    if (!listen_result.has_value()) {
        return make_error(listen_result.error());
    }
    
    // Create upstream client if recursion is enabled
    if (recursion_enabled_) {
        upstream_client_ = std::make_unique<DnsClient>(upstream_server_, upstream_port_);
    }
    
    should_stop_.store(false);
    running_.store(true);
    
    // Start worker threads
    udp_thread_ = std::thread(&DnsServer::handle_udp_requests, this);
    tcp_thread_ = std::thread(&DnsServer::handle_tcp_requests, this);
    
    Logger::log(LogLevel::INFO, "DNS server started on port " + std::to_string(port_));
    return {};
}

void DnsServer::stop() {
    if (!running_.load()) {
        return;
    }
    
    should_stop_.store(true);
    running_.store(false);
    
    // Close sockets to wake up threads
    if (udp_socket_) {
        udp_socket_.reset();
    }
    if (tcp_server_) {
        tcp_server_.reset();
    }
    
    // Wait for threads to finish
    if (udp_thread_.joinable()) {
        udp_thread_.join();
    }
    if (tcp_thread_.joinable()) {
        tcp_thread_.join();
    }
    
    Logger::log(LogLevel::INFO, "DNS server stopped");
}

void DnsServer::add_zone(std::shared_ptr<DnsZone> zone) {
    std::unique_lock<std::shared_mutex> lock(zones_mutex_);
    zones_[zone->get_origin()] = zone;
}

void DnsServer::remove_zone(const std::string& origin) {
    std::unique_lock<std::shared_mutex> lock(zones_mutex_);
    auto normalized_origin = Utils::normalize_domain_name(origin);
    zones_.erase(normalized_origin);
}

void DnsServer::set_upstream_server(const std::string& server, uint16_t port) {
    upstream_server_ = server;
    upstream_port_ = port;
    if (recursion_enabled_) {
        upstream_client_ = std::make_unique<DnsClient>(server, port);
    }
}

void DnsServer::handle_udp_requests() {
    std::vector<uint8_t> buffer(DNS_MAX_UDP_SIZE);
    
    while (!should_stop_.load()) {
        try {
            SocketAddress client_addr;
            auto recv_result = udp_socket_->receive_from(buffer, client_addr);
            if (!recv_result.has_value()) {
                if (should_stop_.load()) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            
            size_t bytes_received = recv_result.value();
            std::vector<uint8_t> query_data(buffer.begin(), buffer.begin() + bytes_received);
            
            // Parse query
            auto query_result = DnsMessage::deserialize(query_data);
            if (!query_result.has_value()) {
                Logger::log(LogLevel::WARNING, "Failed to parse DNS query from " + client_addr.get_ip());
                continue;
            }
            
            auto query = query_result.value();
            auto response = process_query(query);
            
            // Serialize response
            auto response_data = response.serialize();
            
            // Check if response fits in UDP
            if (response_data.size() > DNS_MAX_UDP_SIZE) {
                // Set truncated flag
                response.get_header().flags.truncated = true;
                response_data = response.serialize();
                response_data.resize(DNS_MAX_UDP_SIZE);
            }
            
            // Send response
            udp_socket_->send_to(response_data, client_addr);
            
        } catch (const std::exception& e) {
            Logger::log(LogLevel::ERROR, "UDP handler exception: " + std::string(e.what()));
        }
    }
}

void DnsServer::handle_tcp_requests() {
    while (!should_stop_.load()) {
        try {
            auto connection_result = tcp_server_->accept_connection();
            if (!connection_result.has_value()) {
                if (should_stop_.load()) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
            
            auto connection = std::move(connection_result.value());
            
            // Handle connection in a separate thread (simplified - in production, use thread pool)
            std::thread([this, connection = std::move(connection)]() mutable {
                try {
                    // Read length prefix
                    std::vector<uint8_t> length_bytes(2);
                    auto length_result = connection->receive_exact(length_bytes);
                    if (!length_result.has_value()) return;
                    
                    uint16_t query_length = Utils::ntohs_portable(*reinterpret_cast<const uint16_t*>(length_bytes.data()));
                    
                    // Read query
                    std::vector<uint8_t> query_data(query_length);
                    auto query_result = connection->receive_exact(query_data);
                    if (!query_result.has_value()) return;
                    
                    // Parse query
                    auto message_result = DnsMessage::deserialize(query_data);
                    if (!message_result.has_value()) return;
                    
                    auto query = message_result.value();
                    auto response = process_query(query);
                    
                    // Serialize response
                    auto response_data = response.serialize();
                    
                    // Send length prefix
                    uint16_t response_length = Utils::htons_portable(static_cast<uint16_t>(response_data.size()));
                    std::vector<uint8_t> tcp_response;
                    tcp_response.push_back((response_length >> 8) & 0xFF);
                    tcp_response.push_back(response_length & 0xFF);
                    tcp_response.insert(tcp_response.end(), response_data.begin(), response_data.end());
                    
                    // Send response
                    connection->send(tcp_response);
                    
                } catch (const std::exception& e) {
                    Logger::log(LogLevel::ERROR, "TCP connection handler exception: " + std::string(e.what()));
                }
            }).detach();
            
        } catch (const std::exception& e) {
            Logger::log(LogLevel::ERROR, "TCP handler exception: " + std::string(e.what()));
        }
    }
}

DnsMessage DnsServer::process_query(const DnsMessage& query) {
    auto response = query.create_response();
    
    // Process each question
    for (const auto& question : query.get_questions()) {
        auto records = find_records(question.get_name(), question.get_type(), question.get_class());
        
        if (!records.empty()) {
            // Found records in our zones
            response.get_header().flags.authoritative_answer = true;
            for (const auto& record : records) {
                response.add_answer(record);
            }
        } else if (recursion_enabled_ && query.get_header().flags.recursion_desired && upstream_client_) {
            // Forward to upstream server
            auto upstream_result = forward_query(query);
            if (upstream_result.has_value()) {
                auto upstream_response = upstream_result.value();
                response.get_header().flags.recursion_available = true;
                
                // Copy answers from upstream
                for (const auto& rr : upstream_response.get_answers()) {
                    response.add_answer(rr);
                }
                for (const auto& rr : upstream_response.get_authorities()) {
                    response.add_authority(rr);
                }
                for (const auto& rr : upstream_response.get_additionals()) {
                    response.add_additional(rr);
                }
                
                // Copy response code
                response.get_header().flags.response_code = upstream_response.get_header().flags.response_code;
            } else {
                response.get_header().flags.response_code = DnsResponseCode::SERVER_FAILURE;
            }
        } else {
            // No records found and no recursion
            response.get_header().flags.response_code = DnsResponseCode::NAME_ERROR;
        }
    }
    
    return response;
}

std::vector<DnsResourceRecord> DnsServer::find_records(const std::string& name, DnsType type, DnsClass qclass) {
    std::shared_lock<std::shared_mutex> lock(zones_mutex_);
    
    auto normalized_name = Utils::normalize_domain_name(name);
    
    // Find the most specific zone that contains this name
    std::shared_ptr<DnsZone> best_zone;
    size_t best_match_length = 0;
    
    for (const auto& [origin, zone] : zones_) {
        if (normalized_name.ends_with(origin)) {
            if (origin.length() > best_match_length) {
                best_zone = zone;
                best_match_length = origin.length();
            }
        }
    }
    
    if (best_zone) {
        return best_zone->find_records(name, type, qclass);
    }
    
    return {};
}

Result<DnsMessage> DnsServer::forward_query(const DnsMessage& query) {
    if (!upstream_client_) {
        return make_error(NetworkError::NOT_CONNECTED, "No upstream server configured");
    }
    
    try {
        if (!query.get_questions().empty()) {
            const auto& question = query.get_questions()[0];
            return upstream_client_->query(question.get_name(), question.get_type(), question.get_class());
        }
    } catch (const std::exception& e) {
        Logger::log(LogLevel::ERROR, "Upstream query failed: " + std::string(e.what()));
    }
    
    return make_error(NetworkError::CONNECTION_FAILED, "Upstream query failed");
}

DnsMessage DnsServer::create_error_response(const DnsMessage& query, DnsResponseCode code) {
    auto response = query.create_response();
    response.get_header().flags.response_code = code;
    return response;
}

DnsMessage DnsServer::create_nxdomain_response(const DnsMessage& query) {
    return create_error_response(query, DnsResponseCode::NAME_ERROR);
}

} // namespace NetworkQuests::Dns