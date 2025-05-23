#include "networkquests/websocket.hpp"

#include <cstring>
#include <algorithm>

namespace networkquests::websocket {

// WebSocketFrame implementation

WebSocketFrame::WebSocketFrame(WebSocketOpcode opcode, std::vector<uint8_t> payload, bool fin)
    : fin_(fin), opcode_(opcode), payload_(std::move(payload)) {
    payload_length_ = payload_.size();
}

WebSocketFrame::WebSocketFrame(WebSocketOpcode opcode, std::string_view payload, bool fin)
    : fin_(fin), opcode_(opcode) {
    payload_.assign(payload.begin(), payload.end());
    payload_length_ = payload_.size();
}

void WebSocketFrame::set_payload(const std::vector<uint8_t>& payload) {
    payload_ = payload;
    payload_length_ = payload_.size();
}

void WebSocketFrame::set_payload(std::string_view payload) {
    payload_.assign(payload.begin(), payload.end());
    payload_length_ = payload_.size();
}

bool WebSocketFrame::is_control_frame() const {
    return static_cast<uint8_t>(opcode_) >= 0x8;
}

bool WebSocketFrame::is_data_frame() const {
    return !is_control_frame();
}

WebSocketFrameType WebSocketFrame::frame_type() const {
    switch (opcode_) {
        case WebSocketOpcode::Text: return WebSocketFrameType::Text;
        case WebSocketOpcode::Binary: return WebSocketFrameType::Binary;
        case WebSocketOpcode::Close: return WebSocketFrameType::Close;
        case WebSocketOpcode::Ping: return WebSocketFrameType::Ping;
        case WebSocketOpcode::Pong: return WebSocketFrameType::Pong;
        case WebSocketOpcode::Continuation: return WebSocketFrameType::Continuation;
        default: return WebSocketFrameType::Text;
    }
}

std::vector<uint8_t> WebSocketFrame::serialize() const {
    std::vector<uint8_t> result;
    
    // First byte: FIN (1 bit) + RSV1-3 (3 bits) + Opcode (4 bits)
    uint8_t first_byte = (fin_ ? 0x80 : 0x00) |
                        (rsv1_ ? 0x40 : 0x00) |
                        (rsv2_ ? 0x20 : 0x00) |
                        (rsv3_ ? 0x10 : 0x00) |
                        static_cast<uint8_t>(opcode_);
    result.push_back(first_byte);
    
    // Second byte: MASK (1 bit) + Payload length (7 bits)
    uint8_t second_byte = masked_ ? 0x80 : 0x00;
    
    if (payload_length_ < 126) {
        second_byte |= static_cast<uint8_t>(payload_length_);
        result.push_back(second_byte);
    } else if (payload_length_ < 65536) {
        second_byte |= 126;
        result.push_back(second_byte);
        // Extended payload length (16 bits)
        result.push_back(static_cast<uint8_t>((payload_length_ >> 8) & 0xFF));
        result.push_back(static_cast<uint8_t>(payload_length_ & 0xFF));
    } else {
        second_byte |= 127;
        result.push_back(second_byte);
        // Extended payload length (64 bits)
        for (int i = 7; i >= 0; --i) {
            result.push_back(static_cast<uint8_t>((payload_length_ >> (i * 8)) & 0xFF));
        }
    }
    
    // Masking key (if present)
    if (masked_) {
        result.push_back(static_cast<uint8_t>((masking_key_ >> 24) & 0xFF));
        result.push_back(static_cast<uint8_t>((masking_key_ >> 16) & 0xFF));
        result.push_back(static_cast<uint8_t>((masking_key_ >> 8) & 0xFF));
        result.push_back(static_cast<uint8_t>(masking_key_ & 0xFF));
    }
    
    // Payload data
    std::vector<uint8_t> payload_copy = payload_;
    if (masked_) {
        websocket_utils::apply_masking(payload_copy, masking_key_);
    }
    result.insert(result.end(), payload_copy.begin(), payload_copy.end());
    
    return result;
}

Result<WebSocketFrame> WebSocketFrame::deserialize(const std::vector<uint8_t>& data) {
    return deserialize(data.data(), data.size());
}

Result<WebSocketFrame> WebSocketFrame::deserialize(const uint8_t* data, size_t length) {
    if (length < 2) {
        return make_error("Insufficient data for WebSocket frame header");
    }
    
    WebSocketFrame frame;
    size_t offset = 0;
    
    // Parse first byte
    uint8_t first_byte = data[offset++];
    frame.fin_ = (first_byte & 0x80) != 0;
    frame.rsv1_ = (first_byte & 0x40) != 0;
    frame.rsv2_ = (first_byte & 0x20) != 0;
    frame.rsv3_ = (first_byte & 0x10) != 0;
    frame.opcode_ = static_cast<WebSocketOpcode>(first_byte & 0x0F);
    
    // Parse second byte
    uint8_t second_byte = data[offset++];
    frame.masked_ = (second_byte & 0x80) != 0;
    uint64_t payload_length = second_byte & 0x7F;
    
    // Parse extended payload length
    if (payload_length == 126) {
        if (length < offset + 2) {
            return make_error("Insufficient data for 16-bit payload length");
        }
        payload_length = (static_cast<uint64_t>(data[offset]) << 8) |
                        static_cast<uint64_t>(data[offset + 1]);
        offset += 2;
    } else if (payload_length == 127) {
        if (length < offset + 8) {
            return make_error("Insufficient data for 64-bit payload length");
        }
        payload_length = 0;
        for (int i = 0; i < 8; ++i) {
            payload_length = (payload_length << 8) | static_cast<uint64_t>(data[offset + i]);
        }
        offset += 8;
        
        // Check for most significant bit set (not allowed per RFC)
        if (payload_length & (1ULL << 63)) {
            return make_error("Invalid 64-bit payload length");
        }
    }
    
    frame.payload_length_ = payload_length;
    
    // Parse masking key
    if (frame.masked_) {
        if (length < offset + 4) {
            return make_error("Insufficient data for masking key");
        }
        frame.masking_key_ = (static_cast<uint32_t>(data[offset]) << 24) |
                            (static_cast<uint32_t>(data[offset + 1]) << 16) |
                            (static_cast<uint32_t>(data[offset + 2]) << 8) |
                            static_cast<uint32_t>(data[offset + 3]);
        offset += 4;
    }
    
    // Parse payload
    if (length < offset + payload_length) {
        return make_error("Insufficient data for payload");
    }
    
    frame.payload_.assign(data + offset, data + offset + payload_length);
    
    // Unmask payload if needed
    if (frame.masked_) {
        websocket_utils::apply_masking(frame.payload_, frame.masking_key_);
    }
    
    return frame;
}

std::string WebSocketFrame::payload_as_string() const {
    return std::string(payload_.begin(), payload_.end());
}

void WebSocketFrame::generate_masking_key() {
    masking_key_ = websocket_utils::generate_masking_key();
    masked_ = true;
}

void WebSocketFrame::apply_mask() {
    if (masked_) {
        websocket_utils::apply_masking(payload_, masking_key_);
    }
}

void WebSocketFrame::remove_mask() {
    if (masked_) {
        websocket_utils::apply_masking(payload_, masking_key_);
        masked_ = false;
        masking_key_ = 0;
    }
}

// WebSocketMessage implementation

WebSocketMessage::WebSocketMessage(WebSocketFrameType type) : type_(type) {}

WebSocketMessage::WebSocketMessage(WebSocketFrameType type, std::string_view data) 
    : type_(type), complete_(true) {
    data_.assign(data.begin(), data.end());
}

WebSocketMessage::WebSocketMessage(WebSocketFrameType type, const std::vector<uint8_t>& data)
    : type_(type), data_(data), complete_(true) {}

void WebSocketMessage::append_frame(const WebSocketFrame& frame) {
    // Validate frame type consistency
    WebSocketFrameType frame_type = frame.frame_type();
    
    if (data_.empty()) {
        // First frame determines message type
        if (frame_type == WebSocketFrameType::Continuation) {
            // Cannot start with continuation frame
            return;
        }
        type_ = frame_type;
    } else {
        // Subsequent frames must be continuation frames
        if (frame_type != WebSocketFrameType::Continuation) {
            // Unexpected frame type
            return;
        }
    }
    
    // Append frame payload
    const auto& payload = frame.payload();
    data_.insert(data_.end(), payload.begin(), payload.end());
    
    // Check if message is complete
    if (frame.fin()) {
        complete_ = true;
    }
}

std::string WebSocketMessage::as_string() const {
    return std::string(data_.begin(), data_.end());
}

void WebSocketMessage::clear() {
    data_.clear();
    complete_ = false;
}

WebSocketMessage WebSocketMessage::text(std::string_view content) {
    return WebSocketMessage(WebSocketFrameType::Text, content);
}

WebSocketMessage WebSocketMessage::binary(const std::vector<uint8_t>& content) {
    return WebSocketMessage(WebSocketFrameType::Binary, content);
}

WebSocketMessage WebSocketMessage::close(WebSocketCloseCode code, std::string_view reason) {
    std::vector<uint8_t> payload;
    
    // Add close code (2 bytes, big-endian)
    uint16_t code_value = static_cast<uint16_t>(code);
    payload.push_back(static_cast<uint8_t>((code_value >> 8) & 0xFF));
    payload.push_back(static_cast<uint8_t>(code_value & 0xFF));
    
    // Add reason (UTF-8 encoded)
    if (!reason.empty()) {
        payload.insert(payload.end(), reason.begin(), reason.end());
    }
    
    return WebSocketMessage(WebSocketFrameType::Close, payload);
}

WebSocketMessage WebSocketMessage::ping(std::string_view data) {
    return WebSocketMessage(WebSocketFrameType::Ping, data);
}

WebSocketMessage WebSocketMessage::pong(std::string_view data) {
    return WebSocketMessage(WebSocketFrameType::Pong, data);
}

} // namespace networkquests::websocket