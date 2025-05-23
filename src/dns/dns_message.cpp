#include "networkquests/dns.hpp"
#include <random>
#include <sstream>

namespace NetworkQuests::Dns {

// DnsFlags implementation
uint16_t DnsFlags::to_wire() const {
    uint16_t flags = 0;
    
    if (query_response) flags |= (1 << 15);
    flags |= ((opcode & 0x0F) << 11);
    if (authoritative_answer) flags |= (1 << 10);
    if (truncated) flags |= (1 << 9);
    if (recursion_desired) flags |= (1 << 8);
    if (recursion_available) flags |= (1 << 7);
    flags |= ((z & 0x07) << 4);
    flags |= (static_cast<uint8_t>(response_code) & 0x0F);
    
    return Utils::htons_portable(flags);
}

void DnsFlags::from_wire(uint16_t flags) {
    flags = Utils::ntohs_portable(flags);
    
    query_response = (flags & (1 << 15)) != 0;
    opcode = (flags >> 11) & 0x0F;
    authoritative_answer = (flags & (1 << 10)) != 0;
    truncated = (flags & (1 << 9)) != 0;
    recursion_desired = (flags & (1 << 8)) != 0;
    recursion_available = (flags & (1 << 7)) != 0;
    z = (flags >> 4) & 0x07;
    response_code = static_cast<DnsResponseCode>(flags & 0x0F);
}

// DnsHeader implementation
std::vector<uint8_t> DnsHeader::serialize() const {
    std::vector<uint8_t> data;
    data.reserve(12);  // DNS header is always 12 bytes
    
    // ID (2 bytes)
    uint16_t wire_id = Utils::htons_portable(id);
    data.push_back((wire_id >> 8) & 0xFF);
    data.push_back(wire_id & 0xFF);
    
    // Flags (2 bytes)
    uint16_t wire_flags = flags.to_wire();
    data.push_back((wire_flags >> 8) & 0xFF);
    data.push_back(wire_flags & 0xFF);
    
    // Counts (8 bytes total)
    auto add_count = [&data](uint16_t count) {
        uint16_t wire_count = Utils::htons_portable(count);
        data.push_back((wire_count >> 8) & 0xFF);
        data.push_back(wire_count & 0xFF);
    };
    
    add_count(question_count);
    add_count(answer_count);
    add_count(authority_count);
    add_count(additional_count);
    
    return data;
}

Result<DnsHeader> DnsHeader::deserialize(const uint8_t* data, size_t size, size_t& offset) {
    if (offset + 12 > size) {
        return make_error(NetworkError::INVALID_DATA, "Insufficient data for DNS header");
    }
    
    DnsHeader header;
    
    // ID
    header.id = Utils::ntohs_portable(*reinterpret_cast<const uint16_t*>(data + offset));
    offset += 2;
    
    // Flags
    uint16_t wire_flags = Utils::ntohs_portable(*reinterpret_cast<const uint16_t*>(data + offset));
    header.flags.from_wire(wire_flags);
    offset += 2;
    
    // Counts
    header.question_count = Utils::ntohs_portable(*reinterpret_cast<const uint16_t*>(data + offset));
    offset += 2;
    header.answer_count = Utils::ntohs_portable(*reinterpret_cast<const uint16_t*>(data + offset));
    offset += 2;
    header.authority_count = Utils::ntohs_portable(*reinterpret_cast<const uint16_t*>(data + offset));
    offset += 2;
    header.additional_count = Utils::ntohs_portable(*reinterpret_cast<const uint16_t*>(data + offset));
    offset += 2;
    
    return header;
}

// DnsQuestion implementation
DnsQuestion::DnsQuestion(const std::string& name, DnsType type, DnsClass qclass)
    : name_(name), type_(type), class_(qclass) {}

std::vector<uint8_t> DnsQuestion::serialize() const {
    std::vector<uint8_t> data;
    
    // Name
    auto encoded_name = Utils::encode_dns_name(name_);
    data.insert(data.end(), encoded_name.begin(), encoded_name.end());
    
    // Type (2 bytes)
    uint16_t wire_type = Utils::htons_portable(static_cast<uint16_t>(type_));
    data.push_back((wire_type >> 8) & 0xFF);
    data.push_back(wire_type & 0xFF);
    
    // Class (2 bytes)
    uint16_t wire_class = Utils::htons_portable(static_cast<uint16_t>(class_));
    data.push_back((wire_class >> 8) & 0xFF);
    data.push_back(wire_class & 0xFF);
    
    return data;
}

Result<DnsQuestion> DnsQuestion::deserialize(const uint8_t* data, size_t size, size_t& offset) {
    DnsQuestion question;
    
    // Decode name
    auto name_result = Utils::decode_dns_name(data, size, offset);
    if (!name_result.has_value()) {
        return make_error(name_result.error());
    }
    question.name_ = name_result.value();
    
    // Check remaining data
    if (offset + 4 > size) {
        return make_error(NetworkError::INVALID_DATA, "Insufficient data for question type and class");
    }
    
    // Type
    uint16_t wire_type = Utils::ntohs_portable(*reinterpret_cast<const uint16_t*>(data + offset));
    question.type_ = static_cast<DnsType>(wire_type);
    offset += 2;
    
    // Class
    uint16_t wire_class = Utils::ntohs_portable(*reinterpret_cast<const uint16_t*>(data + offset));
    question.class_ = static_cast<DnsClass>(wire_class);
    offset += 2;
    
    return question;
}

// DnsMessage implementation
DnsMessage::DnsMessage(uint16_t id) {
    header_.id = id;
}

void DnsMessage::add_question(const DnsQuestion& question) {
    questions_.push_back(question);
    update_counts();
}

void DnsMessage::add_answer(const DnsResourceRecord& rr) {
    answers_.push_back(rr);
    update_counts();
}

void DnsMessage::add_authority(const DnsResourceRecord& rr) {
    authorities_.push_back(rr);
    update_counts();
}

void DnsMessage::add_additional(const DnsResourceRecord& rr) {
    additionals_.push_back(rr);
    update_counts();
}

void DnsMessage::clear() {
    questions_.clear();
    answers_.clear();
    authorities_.clear();
    additionals_.clear();
    update_counts();
}

DnsMessage DnsMessage::create_query(const std::string& domain, DnsType type, DnsClass qclass, uint16_t id) {
    if (id == 0) {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<uint16_t> dis(1, 65535);
        id = dis(gen);
    }
    
    DnsMessage message(id);
    message.header_.flags.query_response = false;
    message.header_.flags.opcode = 0;  // Standard query
    message.header_.flags.recursion_desired = true;
    
    message.add_question(DnsQuestion(domain, type, qclass));
    
    return message;
}

DnsMessage DnsMessage::create_response() const {
    DnsMessage response(header_.id);
    response.header_.flags = header_.flags;
    response.header_.flags.query_response = true;
    response.header_.flags.recursion_available = true;
    
    // Copy questions to response
    for (const auto& question : questions_) {
        response.add_question(question);
    }
    
    return response;
}

std::vector<uint8_t> DnsMessage::serialize() const {
    std::vector<uint8_t> data;
    
    // Serialize header
    auto header_data = header_.serialize();
    data.insert(data.end(), header_data.begin(), header_data.end());
    
    // Serialize questions
    for (const auto& question : questions_) {
        auto question_data = question.serialize();
        data.insert(data.end(), question_data.begin(), question_data.end());
    }
    
    // Serialize answers
    for (const auto& rr : answers_) {
        auto rr_data = rr.serialize();
        data.insert(data.end(), rr_data.begin(), rr_data.end());
    }
    
    // Serialize authorities
    for (const auto& rr : authorities_) {
        auto rr_data = rr.serialize();
        data.insert(data.end(), rr_data.begin(), rr_data.end());
    }
    
    // Serialize additionals
    for (const auto& rr : additionals_) {
        auto rr_data = rr.serialize();
        data.insert(data.end(), rr_data.begin(), rr_data.end());
    }
    
    return data;
}

Result<DnsMessage> DnsMessage::deserialize(const std::vector<uint8_t>& data) {
    return deserialize(data.data(), data.size());
}

Result<DnsMessage> DnsMessage::deserialize(const uint8_t* data, size_t size) {
    if (size < 12) {
        return make_error(NetworkError::INVALID_DATA, "DNS message too small");
    }
    
    DnsMessage message;
    size_t offset = 0;
    
    // Deserialize header
    auto header_result = DnsHeader::deserialize(data, size, offset);
    if (!header_result.has_value()) {
        return make_error(header_result.error());
    }
    message.header_ = header_result.value();
    
    // Deserialize questions
    for (uint16_t i = 0; i < message.header_.question_count; ++i) {
        auto question_result = DnsQuestion::deserialize(data, size, offset);
        if (!question_result.has_value()) {
            return make_error(question_result.error());
        }
        message.questions_.push_back(question_result.value());
    }
    
    // Deserialize answers
    for (uint16_t i = 0; i < message.header_.answer_count; ++i) {
        auto rr_result = DnsResourceRecord::deserialize(data, size, offset);
        if (!rr_result.has_value()) {
            return make_error(rr_result.error());
        }
        message.answers_.push_back(rr_result.value());
    }
    
    // Deserialize authorities
    for (uint16_t i = 0; i < message.header_.authority_count; ++i) {
        auto rr_result = DnsResourceRecord::deserialize(data, size, offset);
        if (!rr_result.has_value()) {
            return make_error(rr_result.error());
        }
        message.authorities_.push_back(rr_result.value());
    }
    
    // Deserialize additionals
    for (uint16_t i = 0; i < message.header_.additional_count; ++i) {
        auto rr_result = DnsResourceRecord::deserialize(data, size, offset);
        if (!rr_result.has_value()) {
            return make_error(rr_result.error());
        }
        message.additionals_.push_back(rr_result.value());
    }
    
    return message;
}

std::string DnsMessage::to_string() const {
    std::ostringstream oss;
    
    oss << "DNS Message:\n";
    oss << "  ID: " << header_.id << "\n";
    oss << "  Flags: QR=" << (header_.flags.query_response ? 1 : 0)
        << " OPCODE=" << static_cast<int>(header_.flags.opcode)
        << " AA=" << (header_.flags.authoritative_answer ? 1 : 0)
        << " TC=" << (header_.flags.truncated ? 1 : 0)
        << " RD=" << (header_.flags.recursion_desired ? 1 : 0)
        << " RA=" << (header_.flags.recursion_available ? 1 : 0)
        << " RCODE=" << Utils::dns_response_code_to_string(header_.flags.response_code) << "\n";
    
    oss << "  Questions: " << questions_.size() << "\n";
    for (const auto& question : questions_) {
        oss << "    " << question.get_name() << " " 
            << Utils::dns_class_to_string(question.get_class()) << " "
            << Utils::dns_type_to_string(question.get_type()) << "\n";
    }
    
    oss << "  Answers: " << answers_.size() << "\n";
    for (const auto& rr : answers_) {
        oss << "    " << rr.to_string() << "\n";
    }
    
    oss << "  Authorities: " << authorities_.size() << "\n";
    for (const auto& rr : authorities_) {
        oss << "    " << rr.to_string() << "\n";
    }
    
    oss << "  Additionals: " << additionals_.size() << "\n";
    for (const auto& rr : additionals_) {
        oss << "    " << rr.to_string() << "\n";
    }
    
    return oss.str();
}

void DnsMessage::update_counts() {
    header_.question_count = static_cast<uint16_t>(questions_.size());
    header_.answer_count = static_cast<uint16_t>(answers_.size());
    header_.authority_count = static_cast<uint16_t>(authorities_.size());
    header_.additional_count = static_cast<uint16_t>(additionals_.size());
}

} // namespace NetworkQuests::Dns