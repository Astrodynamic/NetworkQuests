#pragma once

#include "common.hpp"
#include "socket.hpp"
#include "logger.hpp"
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <chrono>

namespace NetworkQuests::Dns {

// Forward declarations
class DnsMessage;
class DnsResourceRecord;
class DnsQuestion;
class DnsClient;
class DnsServer;

// DNS Constants
constexpr uint16_t DNS_PORT = 53;
constexpr size_t DNS_MAX_UDP_SIZE = 512;
constexpr size_t DNS_MAX_LABEL_SIZE = 63;
constexpr size_t DNS_MAX_NAME_SIZE = 255;

// DNS Types (Resource Record Types)
enum class DnsType : uint16_t {
    A = 1,          // IPv4 address
    NS = 2,         // Name server
    MD = 3,         // Mail destination (obsolete)
    MF = 4,         // Mail forwarder (obsolete)
    CNAME = 5,      // Canonical name
    SOA = 6,        // Start of authority
    MB = 7,         // Mailbox (experimental)
    MG = 8,         // Mail group (experimental)
    MR = 9,         // Mail rename (experimental)
    NULL_RR = 10,   // Null record (experimental)
    WKS = 11,       // Well known services
    PTR = 12,       // Pointer record
    HINFO = 13,     // Host information
    MINFO = 14,     // Mailbox information (experimental)
    MX = 15,        // Mail exchange
    TXT = 16,       // Text record
    AAAA = 28,      // IPv6 address (RFC 3596)
    AXFR = 252,     // Zone transfer (query type)
    MAILB = 253,    // Mailbox-related records (query type)
    MAILA = 254,    // Mail agent records (query type, obsolete)
    ANY = 255       // All records (query type)
};

// DNS Classes
enum class DnsClass : uint16_t {
    IN = 1,         // Internet
    CS = 2,         // CSNET (obsolete)
    CH = 3,         // CHAOS
    HS = 4,         // Hesiod
    ANY = 255       // Any class (query class)
};

// DNS Response Codes
enum class DnsResponseCode : uint8_t {
    NO_ERROR = 0,           // No error
    FORMAT_ERROR = 1,       // Format error
    SERVER_FAILURE = 2,     // Server failure
    NAME_ERROR = 3,         // Name error (NXDOMAIN)
    NOT_IMPLEMENTED = 4,    // Not implemented
    REFUSED = 5,            // Refused
    // 6-15 reserved for future use
};

// DNS Header Flags
struct DnsFlags {
    bool query_response = false;        // QR: 0=query, 1=response
    uint8_t opcode = 0;                // OPCODE: 0=standard query, 1=inverse query, 2=status
    bool authoritative_answer = false;  // AA: Authoritative answer
    bool truncated = false;            // TC: Truncated message
    bool recursion_desired = false;    // RD: Recursion desired
    bool recursion_available = false;  // RA: Recursion available
    uint8_t z = 0;                     // Z: Reserved (must be zero)
    DnsResponseCode response_code = DnsResponseCode::NO_ERROR; // RCODE
    
    // Convert to/from wire format
    uint16_t to_wire() const;
    void from_wire(uint16_t flags);
};

// DNS Header Structure
struct DnsHeader {
    uint16_t id = 0;                    // Identifier
    DnsFlags flags;                     // Flags
    uint16_t question_count = 0;        // Number of questions
    uint16_t answer_count = 0;          // Number of answer RRs
    uint16_t authority_count = 0;       // Number of authority RRs
    uint16_t additional_count = 0;      // Number of additional RRs
    
    // Serialization
    std::vector<uint8_t> serialize() const;
    static Result<DnsHeader> deserialize(const uint8_t* data, size_t size, size_t& offset);
};

// DNS Question
class DnsQuestion {
public:
    DnsQuestion() = default;
    DnsQuestion(const std::string& name, DnsType type, DnsClass qclass = DnsClass::IN);
    
    // Getters
    const std::string& get_name() const { return name_; }
    DnsType get_type() const { return type_; }
    DnsClass get_class() const { return class_; }
    
    // Setters
    void set_name(const std::string& name) { name_ = name; }
    void set_type(DnsType type) { type_ = type; }
    void set_class(DnsClass qclass) { class_ = qclass; }
    
    // Serialization
    std::vector<uint8_t> serialize() const;
    static Result<DnsQuestion> deserialize(const uint8_t* data, size_t size, size_t& offset);
    
private:
    std::string name_;
    DnsType type_ = DnsType::A;
    DnsClass class_ = DnsClass::IN;
};

// DNS Resource Record Data (base class)
class DnsRData {
public:
    virtual ~DnsRData() = default;
    virtual std::vector<uint8_t> serialize() const = 0;
    virtual std::unique_ptr<DnsRData> clone() const = 0;
    virtual std::string to_string() const = 0;
};

// A Record (IPv4 address)
class ARecord : public DnsRData {
public:
    explicit ARecord(const std::string& address);
    explicit ARecord(uint32_t address);
    
    uint32_t get_address() const { return address_; }
    std::string get_address_string() const;
    
    std::vector<uint8_t> serialize() const override;
    std::unique_ptr<DnsRData> clone() const override;
    std::string to_string() const override;
    
    static Result<std::unique_ptr<ARecord>> deserialize(const uint8_t* data, size_t size);
    
private:
    uint32_t address_;
};

// AAAA Record (IPv6 address)
class AAAARecord : public DnsRData {
public:
    explicit AAAARecord(const std::string& address);
    explicit AAAARecord(const uint8_t address[16]);
    
    const uint8_t* get_address() const { return address_; }
    std::string get_address_string() const;
    
    std::vector<uint8_t> serialize() const override;
    std::unique_ptr<DnsRData> clone() const override;
    std::string to_string() const override;
    
    static Result<std::unique_ptr<AAAARecord>> deserialize(const uint8_t* data, size_t size);
    
private:
    uint8_t address_[16];
};

// NS Record (Name Server)
class NSRecord : public DnsRData {
public:
    explicit NSRecord(const std::string& name_server);
    
    const std::string& get_name_server() const { return name_server_; }
    
    std::vector<uint8_t> serialize() const override;
    std::unique_ptr<DnsRData> clone() const override;
    std::string to_string() const override;
    
    static Result<std::unique_ptr<NSRecord>> deserialize(const uint8_t* data, size_t size, size_t& offset);
    
private:
    std::string name_server_;
};

// CNAME Record (Canonical Name)
class CNAMERecord : public DnsRData {
public:
    explicit CNAMERecord(const std::string& canonical_name);
    
    const std::string& get_canonical_name() const { return canonical_name_; }
    
    std::vector<uint8_t> serialize() const override;
    std::unique_ptr<DnsRData> clone() const override;
    std::string to_string() const override;
    
    static Result<std::unique_ptr<CNAMERecord>> deserialize(const uint8_t* data, size_t size, size_t& offset);
    
private:
    std::string canonical_name_;
};

// PTR Record (Pointer)
class PTRRecord : public DnsRData {
public:
    explicit PTRRecord(const std::string& pointer_name);
    
    const std::string& get_pointer_name() const { return pointer_name_; }
    
    std::vector<uint8_t> serialize() const override;
    std::unique_ptr<DnsRData> clone() const override;
    std::string to_string() const override;
    
    static Result<std::unique_ptr<PTRRecord>> deserialize(const uint8_t* data, size_t size, size_t& offset);
    
private:
    std::string pointer_name_;
};

// MX Record (Mail Exchange)
class MXRecord : public DnsRData {
public:
    MXRecord(uint16_t preference, const std::string& exchange);
    
    uint16_t get_preference() const { return preference_; }
    const std::string& get_exchange() const { return exchange_; }
    
    std::vector<uint8_t> serialize() const override;
    std::unique_ptr<DnsRData> clone() const override;
    std::string to_string() const override;
    
    static Result<std::unique_ptr<MXRecord>> deserialize(const uint8_t* data, size_t size, size_t& offset);
    
private:
    uint16_t preference_;
    std::string exchange_;
};

// TXT Record (Text)
class TXTRecord : public DnsRData {
public:
    explicit TXTRecord(const std::string& text);
    explicit TXTRecord(const std::vector<std::string>& text_strings);
    
    const std::vector<std::string>& get_text_strings() const { return text_strings_; }
    
    std::vector<uint8_t> serialize() const override;
    std::unique_ptr<DnsRData> clone() const override;
    std::string to_string() const override;
    
    static Result<std::unique_ptr<TXTRecord>> deserialize(const uint8_t* data, size_t size);
    
private:
    std::vector<std::string> text_strings_;
};

// SOA Record (Start of Authority)
class SOARecord : public DnsRData {
public:
    SOARecord(const std::string& primary_ns, const std::string& admin_email,
              uint32_t serial, uint32_t refresh, uint32_t retry,
              uint32_t expire, uint32_t minimum);
    
    const std::string& get_primary_ns() const { return primary_ns_; }
    const std::string& get_admin_email() const { return admin_email_; }
    uint32_t get_serial() const { return serial_; }
    uint32_t get_refresh() const { return refresh_; }
    uint32_t get_retry() const { return retry_; }
    uint32_t get_expire() const { return expire_; }
    uint32_t get_minimum() const { return minimum_; }
    
    std::vector<uint8_t> serialize() const override;
    std::unique_ptr<DnsRData> clone() const override;
    std::string to_string() const override;
    
    static Result<std::unique_ptr<SOARecord>> deserialize(const uint8_t* data, size_t size, size_t& offset);
    
private:
    std::string primary_ns_;
    std::string admin_email_;
    uint32_t serial_;
    uint32_t refresh_;
    uint32_t retry_;
    uint32_t expire_;
    uint32_t minimum_;
};

// DNS Resource Record
class DnsResourceRecord {
public:
    DnsResourceRecord() = default;
    DnsResourceRecord(const std::string& name, DnsType type, DnsClass rclass,
                     uint32_t ttl, std::unique_ptr<DnsRData> rdata);
    
    // Getters
    const std::string& get_name() const { return name_; }
    DnsType get_type() const { return type_; }
    DnsClass get_class() const { return class_; }
    uint32_t get_ttl() const { return ttl_; }
    const DnsRData* get_rdata() const { return rdata_.get(); }
    
    // Setters
    void set_name(const std::string& name) { name_ = name; }
    void set_type(DnsType type) { type_ = type; }
    void set_class(DnsClass rclass) { class_ = rclass; }
    void set_ttl(uint32_t ttl) { ttl_ = ttl; }
    void set_rdata(std::unique_ptr<DnsRData> rdata) { rdata_ = std::move(rdata); }
    
    // Copy and move
    DnsResourceRecord(const DnsResourceRecord& other);
    DnsResourceRecord& operator=(const DnsResourceRecord& other);
    DnsResourceRecord(DnsResourceRecord&&) = default;
    DnsResourceRecord& operator=(DnsResourceRecord&&) = default;
    
    // Serialization
    std::vector<uint8_t> serialize() const;
    static Result<DnsResourceRecord> deserialize(const uint8_t* data, size_t size, size_t& offset);
    
    // String representation
    std::string to_string() const;
    
private:
    std::string name_;
    DnsType type_ = DnsType::A;
    DnsClass class_ = DnsClass::IN;
    uint32_t ttl_ = 0;
    std::unique_ptr<DnsRData> rdata_;
};

// DNS Message
class DnsMessage {
public:
    DnsMessage() = default;
    explicit DnsMessage(uint16_t id);
    
    // Header access
    DnsHeader& get_header() { return header_; }
    const DnsHeader& get_header() const { return header_; }
    
    // Questions
    void add_question(const DnsQuestion& question);
    const std::vector<DnsQuestion>& get_questions() const { return questions_; }
    std::vector<DnsQuestion>& get_questions() { return questions_; }
    
    // Answers
    void add_answer(const DnsResourceRecord& rr);
    const std::vector<DnsResourceRecord>& get_answers() const { return answers_; }
    std::vector<DnsResourceRecord>& get_answers() { return answers_; }
    
    // Authority records
    void add_authority(const DnsResourceRecord& rr);
    const std::vector<DnsResourceRecord>& get_authorities() const { return authorities_; }
    std::vector<DnsResourceRecord>& get_authorities() { return authorities_; }
    
    // Additional records
    void add_additional(const DnsResourceRecord& rr);
    const std::vector<DnsResourceRecord>& get_additionals() const { return additionals_; }
    std::vector<DnsResourceRecord>& get_additionals() { return additionals_; }
    
    // Utility methods
    void clear();
    bool is_query() const { return !header_.flags.query_response; }
    bool is_response() const { return header_.flags.query_response; }
    
    // Create standard query
    static DnsMessage create_query(const std::string& domain, DnsType type = DnsType::A, 
                                  DnsClass qclass = DnsClass::IN, uint16_t id = 0);
    
    // Create response
    DnsMessage create_response() const;
    
    // Serialization
    std::vector<uint8_t> serialize() const;
    static Result<DnsMessage> deserialize(const std::vector<uint8_t>& data);
    static Result<DnsMessage> deserialize(const uint8_t* data, size_t size);
    
    // String representation
    std::string to_string() const;
    
private:
    DnsHeader header_;
    std::vector<DnsQuestion> questions_;
    std::vector<DnsResourceRecord> answers_;
    std::vector<DnsResourceRecord> authorities_;
    std::vector<DnsResourceRecord> additionals_;
    
    void update_counts();
};

// DNS Cache Entry
struct DnsCacheEntry {
    DnsResourceRecord record;
    std::chrono::steady_clock::time_point expiry_time;
    
    DnsCacheEntry(const DnsResourceRecord& rr)
        : record(rr)
        , expiry_time(std::chrono::steady_clock::now() + std::chrono::seconds(rr.get_ttl())) {}
    
    bool is_expired() const {
        return std::chrono::steady_clock::now() >= expiry_time;
    }
};

// DNS Client
class DnsClient {
public:
    explicit DnsClient(const std::string& server = "8.8.8.8", uint16_t port = DNS_PORT);
    ~DnsClient() = default;
    
    // Configuration
    void set_server(const std::string& server, uint16_t port = DNS_PORT);
    void set_timeout(std::chrono::milliseconds timeout) { timeout_ = timeout; }
    void set_retries(int retries) { retries_ = retries; }
    
    // DNS operations
    Result<DnsMessage> query(const std::string& domain, DnsType type = DnsType::A, 
                            DnsClass qclass = DnsClass::IN);
    Result<std::vector<std::string>> resolve_a(const std::string& domain);
    Result<std::vector<std::string>> resolve_aaaa(const std::string& domain);
    Result<std::vector<std::string>> resolve_ns(const std::string& domain);
    Result<std::vector<std::string>> resolve_mx(const std::string& domain);
    Result<std::vector<std::string>> resolve_txt(const std::string& domain);
    Result<std::string> resolve_ptr(const std::string& ip_address);
    
    // Reverse DNS lookup helpers
    static std::string ip_to_arpa(const std::string& ip_address);
    static Result<std::string> arpa_to_ip(const std::string& arpa_name);
    
    // Cache management
    void enable_cache(bool enable = true) { cache_enabled_ = enable; }
    void clear_cache();
    size_t get_cache_size() const { return cache_.size(); }
    
private:
    std::string server_;
    uint16_t port_;
    std::chrono::milliseconds timeout_;
    int retries_;
    uint16_t next_id_;
    
    // Caching
    bool cache_enabled_;
    mutable std::unordered_map<std::string, std::vector<DnsCacheEntry>> cache_;
    mutable std::mutex cache_mutex_;
    
    // Internal methods
    Result<DnsMessage> send_query(const DnsMessage& query);
    Result<DnsMessage> send_udp_query(const DnsMessage& query);
    Result<DnsMessage> send_tcp_query(const DnsMessage& query);
    std::string make_cache_key(const std::string& name, DnsType type, DnsClass qclass) const;
    std::vector<DnsResourceRecord> get_cached_records(const std::string& key) const;
    void cache_records(const std::string& key, const std::vector<DnsResourceRecord>& records) const;
    void cleanup_cache() const;
};

// DNS Zone (for server implementation)
class DnsZone {
public:
    explicit DnsZone(const std::string& origin);
    
    // Zone management
    const std::string& get_origin() const { return origin_; }
    void add_record(const DnsResourceRecord& record);
    void remove_record(const std::string& name, DnsType type);
    
    // Query processing
    std::vector<DnsResourceRecord> find_records(const std::string& name, DnsType type, DnsClass qclass) const;
    std::vector<DnsResourceRecord> find_ns_records() const;
    std::vector<DnsResourceRecord> find_soa_records() const;
    
    // Zone file loading (simplified)
    Result<void> load_from_string(const std::string& zone_data);
    
private:
    std::string origin_;
    std::vector<DnsResourceRecord> records_;
    mutable std::shared_mutex records_mutex_;
    
    std::string normalize_name(const std::string& name) const;
};

// DNS Server
class DnsServer {
public:
    explicit DnsServer(uint16_t port = DNS_PORT);
    ~DnsServer();
    
    // Server control
    Result<void> start();
    void stop();
    bool is_running() const { return running_; }
    
    // Zone management
    void add_zone(std::shared_ptr<DnsZone> zone);
    void remove_zone(const std::string& origin);
    
    // Configuration
    void set_recursion_enabled(bool enabled) { recursion_enabled_ = enabled; }
    void set_upstream_server(const std::string& server, uint16_t port = DNS_PORT);
    
private:
    uint16_t port_;
    std::atomic<bool> running_;
    std::atomic<bool> should_stop_;
    
    // Network components
    std::unique_ptr<UdpSocket> udp_socket_;
    std::unique_ptr<TcpServer> tcp_server_;
    std::thread udp_thread_;
    std::thread tcp_thread_;
    
    // Zone storage
    std::unordered_map<std::string, std::shared_ptr<DnsZone>> zones_;
    mutable std::shared_mutex zones_mutex_;
    
    // Configuration
    bool recursion_enabled_;
    std::string upstream_server_;
    uint16_t upstream_port_;
    std::unique_ptr<DnsClient> upstream_client_;
    
    // Request processing
    void handle_udp_requests();
    void handle_tcp_requests();
    DnsMessage process_query(const DnsMessage& query);
    std::vector<DnsResourceRecord> find_records(const std::string& name, DnsType type, DnsClass qclass);
    Result<DnsMessage> forward_query(const DnsMessage& query);
    
    // Response helpers
    DnsMessage create_error_response(const DnsMessage& query, DnsResponseCode code);
    DnsMessage create_nxdomain_response(const DnsMessage& query);
};

// Utility functions
namespace Utils {
    // DNS name encoding/decoding
    std::vector<uint8_t> encode_dns_name(const std::string& name);
    Result<std::string> decode_dns_name(const uint8_t* data, size_t size, size_t& offset);
    
    // String conversion helpers
    std::string dns_type_to_string(DnsType type);
    std::string dns_class_to_string(DnsClass qclass);
    std::string dns_response_code_to_string(DnsResponseCode code);
    
    Result<DnsType> string_to_dns_type(const std::string& type_str);
    Result<DnsClass> string_to_dns_class(const std::string& class_str);
    
    // IP address utilities
    bool is_valid_ipv4(const std::string& ip);
    bool is_valid_ipv6(const std::string& ip);
    Result<uint32_t> ipv4_string_to_uint32(const std::string& ip);
    std::string ipv4_uint32_to_string(uint32_t ip);
    Result<std::array<uint8_t, 16>> ipv6_string_to_bytes(const std::string& ip);
    std::string ipv6_bytes_to_string(const uint8_t bytes[16]);
    
    // Domain name validation
    bool is_valid_domain_name(const std::string& name);
    std::string normalize_domain_name(const std::string& name);
    
    // Network byte order conversion
    uint16_t htons_portable(uint16_t value);
    uint32_t htonl_portable(uint32_t value);
    uint16_t ntohs_portable(uint16_t value);
    uint32_t ntohl_portable(uint32_t value);
}

} // namespace NetworkQuests::Dns