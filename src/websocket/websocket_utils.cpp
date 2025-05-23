#include "networkquests/websocket.hpp"

#include <sstream>
#include <iomanip>
#include <algorithm>
#include <random>
#include <regex>
#include <array>

namespace networkquests::websocket::websocket_utils {

std::string_view opcode_to_string(WebSocketOpcode opcode) {
    switch (opcode) {
        case WebSocketOpcode::Continuation: return "Continuation";
        case WebSocketOpcode::Text: return "Text";
        case WebSocketOpcode::Binary: return "Binary";
        case WebSocketOpcode::Close: return "Close";
        case WebSocketOpcode::Ping: return "Ping";
        case WebSocketOpcode::Pong: return "Pong";
        default: return "Unknown";
    }
}

std::string_view close_code_to_string(WebSocketCloseCode code) {
    switch (code) {
        case WebSocketCloseCode::Normal: return "Normal Closure";
        case WebSocketCloseCode::GoingAway: return "Going Away";
        case WebSocketCloseCode::ProtocolError: return "Protocol Error";
        case WebSocketCloseCode::UnsupportedData: return "Unsupported Data";
        case WebSocketCloseCode::NoStatusReceived: return "No Status Received";
        case WebSocketCloseCode::AbnormalClosure: return "Abnormal Closure";
        case WebSocketCloseCode::InvalidFramePayloadData: return "Invalid Frame Payload Data";
        case WebSocketCloseCode::PolicyViolation: return "Policy Violation";
        case WebSocketCloseCode::MessageTooBig: return "Message Too Big";
        case WebSocketCloseCode::MandatoryExtension: return "Mandatory Extension";
        case WebSocketCloseCode::InternalError: return "Internal Error";
        case WebSocketCloseCode::ServiceRestart: return "Service Restart";
        case WebSocketCloseCode::TryAgainLater: return "Try Again Later";
        case WebSocketCloseCode::BadGateway: return "Bad Gateway";
        case WebSocketCloseCode::TlsHandshake: return "TLS Handshake";
        default: return "Unknown";
    }
}

std::string_view state_to_string(WebSocketState state) {
    switch (state) {
        case WebSocketState::Connecting: return "Connecting";
        case WebSocketState::Open: return "Open";
        case WebSocketState::Closing: return "Closing";
        case WebSocketState::Closed: return "Closed";
        default: return "Unknown";
    }
}

bool is_valid_utf8(const std::vector<uint8_t>& data) {
    size_t i = 0;
    while (i < data.size()) {
        uint8_t byte = data[i];
        
        if (byte <= 0x7F) {
            // ASCII character
            i++;
        } else if ((byte >> 5) == 0x06) {
            // 110xxxxx - 2-byte sequence
            if (i + 1 >= data.size()) return false;
            if ((data[i + 1] >> 6) != 0x02) return false;
            i += 2;
        } else if ((byte >> 4) == 0x0E) {
            // 1110xxxx - 3-byte sequence
            if (i + 2 >= data.size()) return false;
            if ((data[i + 1] >> 6) != 0x02 || (data[i + 2] >> 6) != 0x02) return false;
            // Check for overlong encoding and surrogates
            uint32_t codepoint = ((byte & 0x0F) << 12) | 
                               ((data[i + 1] & 0x3F) << 6) | 
                               (data[i + 2] & 0x3F);
            if (codepoint < 0x800 || (codepoint >= 0xD800 && codepoint <= 0xDFFF)) return false;
            i += 3;
        } else if ((byte >> 3) == 0x1E) {
            // 11110xxx - 4-byte sequence
            if (i + 3 >= data.size()) return false;
            if ((data[i + 1] >> 6) != 0x02 || (data[i + 2] >> 6) != 0x02 || (data[i + 3] >> 6) != 0x02) return false;
            // Check for overlong encoding and too large codepoints
            uint32_t codepoint = ((byte & 0x07) << 18) | 
                               ((data[i + 1] & 0x3F) << 12) |
                               ((data[i + 2] & 0x3F) << 6) | 
                               (data[i + 3] & 0x3F);
            if (codepoint < 0x10000 || codepoint > 0x10FFFF) return false;
            i += 4;
        } else {
            return false;
        }
    }
    return true;
}

uint32_t generate_masking_key() {
    static thread_local std::random_device rd;
    static thread_local std::mt19937 gen(rd());
    static thread_local std::uniform_int_distribution<uint32_t> dis;
    return dis(gen);
}

void apply_masking(std::vector<uint8_t>& payload, uint32_t masking_key) {
    uint8_t key_bytes[4] = {
        static_cast<uint8_t>((masking_key >> 24) & 0xFF),
        static_cast<uint8_t>((masking_key >> 16) & 0xFF),
        static_cast<uint8_t>((masking_key >> 8) & 0xFF),
        static_cast<uint8_t>(masking_key & 0xFF)
    };
    
    for (size_t i = 0; i < payload.size(); ++i) {
        payload[i] ^= key_bytes[i % 4];
    }
}

// Simple SHA-1 implementation for WebSocket handshake
std::string sha1_hash(std::string_view input) {
    // Constants for SHA-1
    const uint32_t h0 = 0x67452301;
    const uint32_t h1 = 0xEFCDAB89;
    const uint32_t h2 = 0x98BADCFE;
    const uint32_t h3 = 0x10325476;
    const uint32_t h4 = 0xC3D2E1F0;
    
    // Preprocessing
    std::vector<uint8_t> message(input.begin(), input.end());
    uint64_t original_length = message.size() * 8;
    
    // Append bit '1'
    message.push_back(0x80);
    
    // Append zeros
    while ((message.size() % 64) != 56) {
        message.push_back(0x00);
    }
    
    // Append original length as 64-bit big-endian
    for (int i = 7; i >= 0; --i) {
        message.push_back(static_cast<uint8_t>((original_length >> (i * 8)) & 0xFF));
    }
    
    // Process message in 512-bit chunks
    uint32_t hash[5] = {h0, h1, h2, h3, h4};
    
    for (size_t chunk_start = 0; chunk_start < message.size(); chunk_start += 64) {
        uint32_t w[80];
        
        // Break chunk into sixteen 32-bit words
        for (int i = 0; i < 16; ++i) {
            w[i] = (static_cast<uint32_t>(message[chunk_start + i * 4]) << 24) |
                   (static_cast<uint32_t>(message[chunk_start + i * 4 + 1]) << 16) |
                   (static_cast<uint32_t>(message[chunk_start + i * 4 + 2]) << 8) |
                   static_cast<uint32_t>(message[chunk_start + i * 4 + 3]);
        }
        
        // Extend the sixteen 32-bit words into eighty 32-bit words
        for (int i = 16; i < 80; ++i) {
            uint32_t temp = w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16];
            w[i] = (temp << 1) | (temp >> 31);
        }
        
        // Initialize hash value for this chunk
        uint32_t a = hash[0], b = hash[1], c = hash[2], d = hash[3], e = hash[4];
        
        // Main loop
        for (int i = 0; i < 80; ++i) {
            uint32_t f, k;
            if (i < 20) {
                f = (b & c) | (~b & d);
                k = 0x5A827999;
            } else if (i < 40) {
                f = b ^ c ^ d;
                k = 0x6ED9EBA1;
            } else if (i < 60) {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8F1BBCDC;
            } else {
                f = b ^ c ^ d;
                k = 0xCA62C1D6;
            }
            
            uint32_t temp = ((a << 5) | (a >> 27)) + f + e + k + w[i];
            e = d;
            d = c;
            c = (b << 30) | (b >> 2);
            b = a;
            a = temp;
        }
        
        // Add this chunk's hash to result so far
        hash[0] += a;
        hash[1] += b;
        hash[2] += c;
        hash[3] += d;
        hash[4] += e;
    }
    
    // Produce the final hash value as a 160-bit number (20 bytes)
    std::vector<uint8_t> result(20);
    for (int i = 0; i < 5; ++i) {
        result[i * 4] = (hash[i] >> 24) & 0xFF;
        result[i * 4 + 1] = (hash[i] >> 16) & 0xFF;
        result[i * 4 + 2] = (hash[i] >> 8) & 0xFF;
        result[i * 4 + 3] = hash[i] & 0xFF;
    }
    
    return std::string(result.begin(), result.end());
}

std::string base64_encode(const std::vector<uint8_t>& input) {
    return base64_encode(std::string_view(reinterpret_cast<const char*>(input.data()), input.size()));
}

std::string base64_encode(std::string_view input) {
    static const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    
    std::string result;
    int val = 0, valb = -6;
    
    for (unsigned char c : input) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            result.push_back(chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    
    if (valb > -6) {
        result.push_back(chars[((val << 8) >> (valb + 8)) & 0x3F]);
    }
    
    while (result.size() % 4) {
        result.push_back('=');
    }
    
    return result;
}

Result<std::vector<uint8_t>> base64_decode(std::string_view input) {
    static const std::array<int, 128> decode_table = []() {
        std::array<int, 128> table{};
        std::fill(table.begin(), table.end(), -1);
        
        for (int i = 0; i < 26; ++i) {
            table['A' + i] = i;
            table['a' + i] = i + 26;
        }
        for (int i = 0; i < 10; ++i) {
            table['0' + i] = i + 52;
        }
        table['+'] = 62;
        table['/'] = 63;
        table['='] = 0;
        
        return table;
    }();
    
    std::vector<uint8_t> result;
    int val = 0, valb = -8;
    
    for (unsigned char c : input) {
        if (c > 127 || decode_table[c] == -1) {
            if (c != '=') {
                return make_error("Invalid base64 character");
            }
            break;
        }
        
        val = (val << 6) + decode_table[c];
        valb += 6;
        if (valb >= 0) {
            result.push_back(static_cast<uint8_t>((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    
    return result;
}

std::vector<std::string> parse_extension_list(std::string_view extensions) {
    std::vector<std::string> result;
    std::istringstream stream(std::string(extensions));
    std::string extension;
    
    while (std::getline(stream, extension, ',')) {
        // Trim whitespace
        extension.erase(0, extension.find_first_not_of(" \t"));
        extension.erase(extension.find_last_not_of(" \t") + 1);
        
        if (!extension.empty()) {
            result.push_back(extension);
        }
    }
    
    return result;
}

std::vector<std::string> parse_subprotocol_list(std::string_view protocols) {
    return parse_extension_list(protocols); // Same parsing logic
}

std::string build_extension_header(const std::vector<WebSocketExtension>& extensions) {
    std::string result;
    
    for (size_t i = 0; i < extensions.size(); ++i) {
        if (i > 0) result += ", ";
        
        switch (extensions[i]) {
            case WebSocketExtension::PerMessageDeflate:
                result += "permessage-deflate";
                break;
            case WebSocketExtension::None:
            default:
                break;
        }
    }
    
    return result;
}

std::string build_subprotocol_header(const std::vector<std::string>& protocols) {
    std::string result;
    
    for (size_t i = 0; i < protocols.size(); ++i) {
        if (i > 0) result += ", ";
        result += protocols[i];
    }
    
    return result;
}

Result<void> validate_frame(const WebSocketFrame& frame) {
    // Check reserved bits
    if (frame.rsv1() || frame.rsv2() || frame.rsv3()) {
        // RSV bits must be 0 unless extension negotiated
        return make_error("Reserved bits must be 0");
    }
    
    // Check control frame constraints
    if (frame.is_control_frame()) {
        if (!frame.fin()) {
            return make_error("Control frames must not be fragmented");
        }
        if (frame.payload_length() > constants::MAX_CONTROL_FRAME_SIZE) {
            return make_error("Control frame payload too large");
        }
    }
    
    // Check text frame UTF-8 encoding
    if (frame.opcode() == WebSocketOpcode::Text && !is_valid_utf8(frame.payload())) {
        return make_error("Text frame contains invalid UTF-8");
    }
    
    return make_success();
}

WebSocketFrame create_close_frame(WebSocketCloseCode code, std::string_view reason) {
    std::vector<uint8_t> payload;
    
    // Add close code (2 bytes, big-endian)
    uint16_t code_value = static_cast<uint16_t>(code);
    payload.push_back(static_cast<uint8_t>((code_value >> 8) & 0xFF));
    payload.push_back(static_cast<uint8_t>(code_value & 0xFF));
    
    // Add reason (UTF-8 encoded)
    if (!reason.empty()) {
        payload.insert(payload.end(), reason.begin(), reason.end());
    }
    
    return WebSocketFrame(WebSocketOpcode::Close, payload);
}

Result<CloseInfo> parse_close_frame(const WebSocketFrame& frame) {
    if (frame.opcode() != WebSocketOpcode::Close) {
        return make_error("Not a close frame");
    }
    
    const auto& payload = frame.payload();
    CloseInfo info;
    
    if (payload.empty()) {
        info.code = WebSocketCloseCode::NoStatusReceived;
        return info;
    }
    
    if (payload.size() < 2) {
        return make_error("Invalid close frame payload");
    }
    
    // Parse close code (2 bytes, big-endian)
    uint16_t code_value = (static_cast<uint16_t>(payload[0]) << 8) | payload[1];
    info.code = static_cast<WebSocketCloseCode>(code_value);
    
    // Parse reason (remaining bytes as UTF-8)
    if (payload.size() > 2) {
        std::vector<uint8_t> reason_bytes(payload.begin() + 2, payload.end());
        if (!is_valid_utf8(reason_bytes)) {
            return make_error("Close frame reason contains invalid UTF-8");
        }
        info.reason = std::string(reason_bytes.begin(), reason_bytes.end());
    }
    
    return info;
}

Result<std::string> simple_websocket_request(std::string_view url, std::string_view message,
                                           std::chrono::milliseconds timeout) {
    WebSocketClient client;
    client.set_timeout(timeout);
    
    auto connection_result = client.connect(url);
    if (!connection_result) {
        return connection_result.error();
    }
    
    auto connection = std::move(connection_result.value());
    
    // Send message
    auto send_result = connection->send_text(message);
    if (!send_result) {
        return send_result.error();
    }
    
    // Receive response
    auto receive_result = connection->receive_message(timeout);
    if (!receive_result) {
        return receive_result.error();
    }
    
    auto response = receive_result.value();
    if (response.type() != WebSocketFrameType::Text) {
        return make_error("Expected text response");
    }
    
    // Close connection
    connection->close();
    
    return response.as_string();
}

} // namespace networkquests::websocket::websocket_utils