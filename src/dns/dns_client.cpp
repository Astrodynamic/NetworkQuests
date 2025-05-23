#include "networkquests/dns.hpp"
#include "networkquests/udp.hpp"
#include "networkquests/tcp.hpp"
#include <thread>
#include <algorithm>
#include <sstream>

namespace NetworkQuests::Dns {

// DnsClient implementation
DnsClient::DnsClient(const std::string& server, uint16_t port)
    : server_(server)
    , port_(port)
    , timeout_(std::chrono::milliseconds(5000))
    , retries_(3)
    , next_id_(1)
    , cache_enabled_(true) {}

void DnsClient::set_server(const std::string& server, uint16_t port) {
    server_ = server;
    port_ = port;
}

Result<DnsMessage> DnsClient::query(const std::string& domain, DnsType type, DnsClass qclass) {
    // Check cache first
    if (cache_enabled_) {
        auto cache_key = make_cache_key(domain, type, qclass);
        auto cached_records = get_cached_records(cache_key);
        if (!cached_records.empty()) {
            // Create response from cached records
            DnsMessage response;
            response.get_header().id = next_id_++;
            response.get_header().flags.query_response = true;
            response.get_header().flags.recursion_available = true;
            response.add_question(DnsQuestion(domain, type, qclass));
            
            for (const auto& record : cached_records) {
                response.add_answer(record);
            }
            
            Logger::log(LogLevel::DEBUG, "DNS cache hit for " + domain);
            return response;
        }
    }
    
    // Create query
    auto query = DnsMessage::create_query(domain, type, qclass, next_id_++);
    
    // Send query
    auto result = send_query(query);
    if (!result.has_value()) {
        return result;
    }
    
    auto response = result.value();
    
    // Cache the response
    if (cache_enabled_ && !response.get_answers().empty()) {
        auto cache_key = make_cache_key(domain, type, qclass);
        cache_records(cache_key, response.get_answers());
    }
    
    return response;
}

Result<std::vector<std::string>> DnsClient::resolve_a(const std::string& domain) {
    auto result = query(domain, DnsType::A, DnsClass::IN);
    if (!result.has_value()) {
        return make_error(result.error());
    }
    
    std::vector<std::string> addresses;
    for (const auto& rr : result.value().get_answers()) {
        if (rr.get_type() == DnsType::A) {
            auto a_record = dynamic_cast<const ARecord*>(rr.get_rdata());
            if (a_record) {
                addresses.push_back(a_record->get_address_string());
            }
        }
    }
    
    return addresses;
}

Result<std::vector<std::string>> DnsClient::resolve_aaaa(const std::string& domain) {
    auto result = query(domain, DnsType::AAAA, DnsClass::IN);
    if (!result.has_value()) {
        return make_error(result.error());
    }
    
    std::vector<std::string> addresses;
    for (const auto& rr : result.value().get_answers()) {
        if (rr.get_type() == DnsType::AAAA) {
            auto aaaa_record = dynamic_cast<const AAAARecord*>(rr.get_rdata());
            if (aaaa_record) {
                addresses.push_back(aaaa_record->get_address_string());
            }
        }
    }
    
    return addresses;
}

Result<std::vector<std::string>> DnsClient::resolve_ns(const std::string& domain) {
    auto result = query(domain, DnsType::NS, DnsClass::IN);
    if (!result.has_value()) {
        return make_error(result.error());
    }
    
    std::vector<std::string> nameservers;
    for (const auto& rr : result.value().get_answers()) {
        if (rr.get_type() == DnsType::NS) {
            auto ns_record = dynamic_cast<const NSRecord*>(rr.get_rdata());
            if (ns_record) {
                nameservers.push_back(ns_record->get_name_server());
            }
        }
    }
    
    return nameservers;
}

Result<std::vector<std::string>> DnsClient::resolve_mx(const std::string& domain) {
    auto result = query(domain, DnsType::MX, DnsClass::IN);
    if (!result.has_value()) {
        return make_error(result.error());
    }
    
    std::vector<std::pair<uint16_t, std::string>> mx_records;
    for (const auto& rr : result.value().get_answers()) {
        if (rr.get_type() == DnsType::MX) {
            auto mx_record = dynamic_cast<const MXRecord*>(rr.get_rdata());
            if (mx_record) {
                mx_records.emplace_back(mx_record->get_preference(), mx_record->get_exchange());
            }
        }
    }
    
    // Sort by preference
    std::sort(mx_records.begin(), mx_records.end());
    
    std::vector<std::string> exchanges;
    for (const auto& mx : mx_records) {
        exchanges.push_back(std::to_string(mx.first) + " " + mx.second);
    }
    
    return exchanges;
}

Result<std::vector<std::string>> DnsClient::resolve_txt(const std::string& domain) {
    auto result = query(domain, DnsType::TXT, DnsClass::IN);
    if (!result.has_value()) {
        return make_error(result.error());
    }
    
    std::vector<std::string> texts;
    for (const auto& rr : result.value().get_answers()) {
        if (rr.get_type() == DnsType::TXT) {
            auto txt_record = dynamic_cast<const TXTRecord*>(rr.get_rdata());
            if (txt_record) {
                for (const auto& text : txt_record->get_text_strings()) {
                    texts.push_back(text);
                }
            }
        }
    }
    
    return texts;
}

Result<std::string> DnsClient::resolve_ptr(const std::string& ip_address) {
    auto arpa_name = ip_to_arpa(ip_address);
    
    auto result = query(arpa_name, DnsType::PTR, DnsClass::IN);
    if (!result.has_value()) {
        return make_error(result.error());
    }
    
    for (const auto& rr : result.value().get_answers()) {
        if (rr.get_type() == DnsType::PTR) {
            auto ptr_record = dynamic_cast<const PTRRecord*>(rr.get_rdata());
            if (ptr_record) {
                return ptr_record->get_pointer_name();
            }
        }
    }
    
    return make_error(NetworkError::NOT_FOUND, "No PTR record found for " + ip_address);
}

std::string DnsClient::ip_to_arpa(const std::string& ip_address) {
    if (Utils::is_valid_ipv4(ip_address)) {
        // IPv4 reverse lookup
        std::istringstream iss(ip_address);
        std::vector<std::string> octets;
        std::string octet;
        
        while (std::getline(iss, octet, '.')) {
            octets.push_back(octet);
        }
        
        if (octets.size() == 4) {
            std::ostringstream oss;
            oss << octets[3] << "." << octets[2] << "." << octets[1] << "." << octets[0] << ".in-addr.arpa";
            return oss.str();
        }
    } else if (Utils::is_valid_ipv6(ip_address)) {
        // IPv6 reverse lookup (simplified)
        return ip_address + ".ip6.arpa";  // This is a simplified implementation
    }
    
    return "";
}

Result<std::string> DnsClient::arpa_to_ip(const std::string& arpa_name) {
    if (arpa_name.find(".in-addr.arpa") != std::string::npos) {
        // IPv4 reverse
        std::string prefix = arpa_name.substr(0, arpa_name.find(".in-addr.arpa"));
        std::istringstream iss(prefix);
        std::vector<std::string> octets;
        std::string octet;
        
        while (std::getline(iss, octet, '.')) {
            octets.push_back(octet);
        }
        
        if (octets.size() == 4) {
            return octets[3] + "." + octets[2] + "." + octets[1] + "." + octets[0];
        }
    } else if (arpa_name.find(".ip6.arpa") != std::string::npos) {
        // IPv6 reverse (simplified)
        return arpa_name.substr(0, arpa_name.find(".ip6.arpa"));
    }
    
    return make_error(NetworkError::INVALID_DATA, "Invalid ARPA name: " + arpa_name);
}

void DnsClient::clear_cache() {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    cache_.clear();
}

Result<DnsMessage> DnsClient::send_query(const DnsMessage& query) {
    auto serialized = query.serialize();
    
    // Try UDP first
    if (serialized.size() <= DNS_MAX_UDP_SIZE) {
        auto udp_result = send_udp_query(query);
        if (udp_result.has_value()) {
            auto response = udp_result.value();
            // Check if response is truncated
            if (!response.get_header().flags.truncated) {
                return response;
            }
            Logger::log(LogLevel::DEBUG, "DNS response truncated, falling back to TCP");
        } else {
            Logger::log(LogLevel::DEBUG, "UDP query failed: " + udp_result.error().message);
        }
    }
    
    // Fall back to TCP
    return send_tcp_query(query);
}

Result<DnsMessage> DnsClient::send_udp_query(const DnsMessage& query) {
    auto udp_socket = UdpSocket::create();
    if (!udp_socket.has_value()) {
        return make_error(udp_socket.error());
    }
    
    auto socket = std::move(udp_socket.value());
    auto serialized = query.serialize();
    
    // Resolve server address
    auto server_addr = SocketAddress::from_ipv4(server_, port_);
    if (!server_addr.has_value()) {
        return make_error(server_addr.error());
    }
    
    for (int attempt = 0; attempt <= retries_; ++attempt) {
        // Send query
        auto send_result = socket->send_to(serialized, server_addr.value());
        if (!send_result.has_value()) {
            Logger::log(LogLevel::WARNING, "UDP send failed: " + send_result.error().message);
            continue;
        }
        
        // Receive response with timeout
        auto start_time = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - start_time < timeout_) {
            std::vector<uint8_t> response_data(DNS_MAX_UDP_SIZE);
            SocketAddress sender;
            
            auto recv_result = socket->receive_from(response_data, sender);
            if (recv_result.has_value()) {
                response_data.resize(recv_result.value());
                
                // Verify sender
                if (sender.get_ip() == server_addr.value().get_ip() && 
                    sender.get_port() == server_addr.value().get_port()) {
                    
                    // Parse response
                    auto message_result = DnsMessage::deserialize(response_data);
                    if (message_result.has_value()) {
                        auto response = message_result.value();
                        
                        // Verify ID matches
                        if (response.get_header().id == query.get_header().id) {
                            return response;
                        }
                    }
                }
            }
            
            // Brief sleep to avoid busy waiting
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        Logger::log(LogLevel::DEBUG, "DNS UDP attempt " + std::to_string(attempt + 1) + " timed out");
    }
    
    return make_error(NetworkError::TIMEOUT, "DNS UDP query timed out after " + std::to_string(retries_ + 1) + " attempts");
}

Result<DnsMessage> DnsClient::send_tcp_query(const DnsMessage& query) {
    auto tcp_client = TcpClient::create();
    if (!tcp_client.has_value()) {
        return make_error(tcp_client.error());
    }
    
    auto client = std::move(tcp_client.value());
    
    // Connect to server
    auto connect_result = client->connect(server_, port_);
    if (!connect_result.has_value()) {
        return make_error(connect_result.error());
    }
    
    auto serialized = query.serialize();
    
    // TCP messages are prefixed with 2-byte length
    std::vector<uint8_t> tcp_message;
    uint16_t length = Utils::htons_portable(static_cast<uint16_t>(serialized.size()));
    tcp_message.push_back((length >> 8) & 0xFF);
    tcp_message.push_back(length & 0xFF);
    tcp_message.insert(tcp_message.end(), serialized.begin(), serialized.end());
    
    // Send query
    auto send_result = client->send(tcp_message);
    if (!send_result.has_value()) {
        return make_error(send_result.error());
    }
    
    // Receive length prefix
    std::vector<uint8_t> length_bytes(2);
    auto recv_length_result = client->receive_exact(length_bytes);
    if (!recv_length_result.has_value()) {
        return make_error(recv_length_result.error());
    }
    
    uint16_t response_length = Utils::ntohs_portable(*reinterpret_cast<const uint16_t*>(length_bytes.data()));
    
    // Receive response
    std::vector<uint8_t> response_data(response_length);
    auto recv_result = client->receive_exact(response_data);
    if (!recv_result.has_value()) {
        return make_error(recv_result.error());
    }
    
    // Parse response
    auto message_result = DnsMessage::deserialize(response_data);
    if (!message_result.has_value()) {
        return make_error(message_result.error());
    }
    
    auto response = message_result.value();
    
    // Verify ID matches
    if (response.get_header().id != query.get_header().id) {
        return make_error(NetworkError::INVALID_DATA, "Response ID mismatch");
    }
    
    return response;
}

std::string DnsClient::make_cache_key(const std::string& name, DnsType type, DnsClass qclass) const {
    return Utils::normalize_domain_name(name) + ":" + 
           std::to_string(static_cast<uint16_t>(type)) + ":" +
           std::to_string(static_cast<uint16_t>(qclass));
}

std::vector<DnsResourceRecord> DnsClient::get_cached_records(const std::string& key) const {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    
    auto it = cache_.find(key);
    if (it == cache_.end()) {
        return {};
    }
    
    std::vector<DnsResourceRecord> valid_records;
    auto& entries = it->second;
    
    // Remove expired entries and collect valid ones
    auto now = std::chrono::steady_clock::now();
    auto new_end = std::remove_if(entries.begin(), entries.end(),
        [now](const DnsCacheEntry& entry) {
            return entry.is_expired();
        });
    entries.erase(new_end, entries.end());
    
    // If no valid entries remain, remove the key
    if (entries.empty()) {
        cache_.erase(it);
        return {};
    }
    
    // Collect valid records
    for (const auto& entry : entries) {
        valid_records.push_back(entry.record);
    }
    
    return valid_records;
}

void DnsClient::cache_records(const std::string& key, const std::vector<DnsResourceRecord>& records) const {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    
    auto& entries = cache_[key];
    for (const auto& record : records) {
        entries.emplace_back(record);
    }
    
    // Cleanup old entries periodically
    cleanup_cache();
}

void DnsClient::cleanup_cache() const {
    // This is called with cache_mutex_ already locked
    
    static auto last_cleanup = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    
    // Only cleanup every 5 minutes
    if (now - last_cleanup < std::chrono::minutes(5)) {
        return;
    }
    
    auto it = cache_.begin();
    while (it != cache_.end()) {
        auto& entries = it->second;
        
        // Remove expired entries
        auto new_end = std::remove_if(entries.begin(), entries.end(),
            [now](const DnsCacheEntry& entry) {
                return entry.is_expired();
            });
        entries.erase(new_end, entries.end());
        
        // Remove empty buckets
        if (entries.empty()) {
            it = cache_.erase(it);
        } else {
            ++it;
        }
    }
    
    last_cleanup = now;
}

} // namespace NetworkQuests::Dns