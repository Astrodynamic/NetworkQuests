#include "networkquests/websocket.hpp"

namespace networkquests::websocket {

// WebSocketConnection implementation

WebSocketConnection::WebSocketConnection(tcp::TcpConnection tcp_conn) 
    : tcp_conn_(std::move(tcp_conn)) {}

SocketAddress WebSocketConnection::local_address() const {
    return tcp_conn_.local_address();
}

SocketAddress WebSocketConnection::remote_address() const {
    return tcp_conn_.remote_address();
}

Result<void> WebSocketConnection::send(const WebSocketMessage& message) {
    switch (message.type()) {
        case WebSocketFrameType::Text:
            return send_text(message.as_string());
        case WebSocketFrameType::Binary:
            return send_binary(message.data());
        case WebSocketFrameType::Close: {
            auto close_info_result = websocket_utils::parse_close_frame(
                WebSocketFrame(WebSocketOpcode::Close, message.data()));
            if (close_info_result) {
                auto close_info = close_info_result.value();
                return close(close_info.code, close_info.reason);
            } else {
                return close();
            }
        }
        case WebSocketFrameType::Ping:
            return send_ping(message.as_string());
        case WebSocketFrameType::Pong:
            return send_pong(message.as_string());
        default:
            return make_error("Invalid message type");
    }
}

Result<void> WebSocketConnection::send_text(std::string_view text) {
    if (state_ != WebSocketState::Open) {
        return make_error("Connection not open");
    }
    
    WebSocketFrame frame(WebSocketOpcode::Text, text);
    
    // Client-to-server frames must be masked
    if (is_client_) {
        frame.generate_masking_key();
    }
    
    return send_frame(frame);
}

Result<void> WebSocketConnection::send_binary(const std::vector<uint8_t>& data) {
    if (state_ != WebSocketState::Open) {
        return make_error("Connection not open");
    }
    
    WebSocketFrame frame(WebSocketOpcode::Binary, data);
    
    // Client-to-server frames must be masked
    if (is_client_) {
        frame.generate_masking_key();
    }
    
    return send_frame(frame);
}

Result<void> WebSocketConnection::send_ping(std::string_view data) {
    if (state_ != WebSocketState::Open) {
        return make_error("Connection not open");
    }
    
    if (data.size() > websocket_utils::constants::MAX_CONTROL_FRAME_SIZE) {
        return make_error("Ping data too large");
    }
    
    WebSocketFrame frame(WebSocketOpcode::Ping, data);
    
    // Client-to-server frames must be masked
    if (is_client_) {
        frame.generate_masking_key();
    }
    
    return send_frame(frame);
}

Result<void> WebSocketConnection::send_pong(std::string_view data) {
    if (state_ != WebSocketState::Open) {
        return make_error("Connection not open");
    }
    
    if (data.size() > websocket_utils::constants::MAX_CONTROL_FRAME_SIZE) {
        return make_error("Pong data too large");
    }
    
    WebSocketFrame frame(WebSocketOpcode::Pong, data);
    
    // Client-to-server frames must be masked
    if (is_client_) {
        frame.generate_masking_key();
    }
    
    return send_frame(frame);
}

Result<void> WebSocketConnection::close(WebSocketCloseCode code, std::string_view reason) {
    if (state_ == WebSocketState::Closed) {
        return make_success();
    }
    
    if (state_ == WebSocketState::Open) {
        state_ = WebSocketState::Closing;
        
        // Send close frame
        auto close_frame = websocket_utils::create_close_frame(code, reason);
        
        // Client-to-server frames must be masked
        if (is_client_) {
            close_frame.generate_masking_key();
        }
        
        auto send_result = send_frame(close_frame);
        if (!send_result) {
            state_ = WebSocketState::Closed;
            return send_result;
        }
    }
    
    // Stop async receive if running
    stop_async_receive();
    
    state_ = WebSocketState::Closed;
    
    // Call close handler
    if (close_handler_) {
        close_handler_(code, reason);
    }
    
    return make_success();
}

Result<WebSocketMessage> WebSocketConnection::receive_message(std::chrono::milliseconds timeout) {
    if (state_ != WebSocketState::Open) {
        return make_error("Connection not open");
    }
    
    auto start_time = std::chrono::steady_clock::now();
    
    while (true) {
        // Check timeout
        auto elapsed = std::chrono::steady_clock::now() - start_time;
        if (elapsed >= timeout) {
            return make_error("Timeout waiting for message");
        }
        
        auto remaining_timeout = timeout - std::chrono::duration_cast<std::chrono::milliseconds>(elapsed);
        
        // Receive frame
        auto frame_result = receive_frame();
        if (!frame_result) {
            return frame_result.error();
        }
        
        auto frame = frame_result.value();
        
        // Handle control frames immediately
        if (frame.is_control_frame()) {
            handle_control_frame(frame);
            continue;
        }
        
        // Handle data frames
        WebSocketFrameType frame_type = frame.frame_type();
        
        // Find or create partial message
        auto& partial_message = partial_messages_[frame_type];
        
        if (partial_message.data().empty()) {
            // Start new message
            if (frame_type == WebSocketFrameType::Continuation) {
                return make_error("Unexpected continuation frame");
            }
            partial_message = WebSocketMessage(frame_type);
        } else {
            // Continue existing message
            if (frame_type != WebSocketFrameType::Continuation) {
                return make_error("Expected continuation frame");
            }
        }
        
        partial_message.append_frame(frame);
        
        // Check if message is complete
        if (partial_message.is_complete()) {
            auto complete_message = std::move(partial_message);
            partial_messages_.erase(frame_type);
            
            // Validate text message UTF-8
            if (complete_message.type() == WebSocketFrameType::Text) {
                if (!websocket_utils::is_valid_utf8(complete_message.data())) {
                    return make_error("Invalid UTF-8 in text message");
                }
            }
            
            return complete_message;
        }
    }
}

void WebSocketConnection::start_async_receive() {
    if (receiving_.load()) {
        return; // Already receiving
    }
    
    receiving_.store(true);
    receive_thread_ = std::thread(&WebSocketConnection::async_receive_loop, this);
}

void WebSocketConnection::stop_async_receive() {
    if (!receiving_.load()) {
        return; // Not receiving
    }
    
    receiving_.store(false);
    
    if (receive_thread_.joinable()) {
        receive_thread_.join();
    }
}

Result<WebSocketFrame> WebSocketConnection::receive_frame() {
    // Read frame header (minimum 2 bytes)
    auto header_result = tcp_conn_.receive(2, std::chrono::seconds(30));
    if (!header_result) {
        return header_result.error();
    }
    
    auto header_data = header_result.value();
    if (header_data.size() < 2) {
        return make_error("Insufficient frame header data");
    }
    
    // Parse initial frame info
    uint8_t second_byte = header_data[1];
    bool masked = (second_byte & 0x80) != 0;
    uint64_t payload_length = second_byte & 0x7F;
    
    size_t additional_bytes = 0;
    
    // Calculate additional bytes needed
    if (payload_length == 126) {
        additional_bytes += 2; // 16-bit extended length
    } else if (payload_length == 127) {
        additional_bytes += 8; // 64-bit extended length
    }
    
    if (masked) {
        additional_bytes += 4; // Masking key
    }
    
    // Read additional header bytes if needed
    if (additional_bytes > 0) {
        auto additional_result = tcp_conn_.receive(additional_bytes, std::chrono::seconds(30));
        if (!additional_result) {
            return additional_result.error();
        }
        
        auto additional_data = additional_result.value();
        header_data.insert(header_data.end(), additional_data.begin(), additional_data.end());
    }
    
    // Parse frame header
    auto frame_result = WebSocketFrame::deserialize(header_data.data(), header_data.size());
    if (!frame_result) {
        return frame_result.error();
    }
    
    auto frame = frame_result.value();
    
    // Read payload if present
    if (frame.payload_length() > 0) {
        auto payload_result = tcp_conn_.receive(frame.payload_length(), std::chrono::seconds(30));
        if (!payload_result) {
            return payload_result.error();
        }
        
        auto payload_data = payload_result.value();
        
        // Apply unmasking if needed
        if (frame.masked()) {
            websocket_utils::apply_masking(payload_data, frame.masking_key());
        }
        
        frame.set_payload(payload_data);
    }
    
    // Validate frame
    auto validate_result = websocket_utils::validate_frame(frame);
    if (!validate_result) {
        return validate_result.error();
    }
    
    return frame;
}

Result<void> WebSocketConnection::send_frame(const WebSocketFrame& frame) {
    auto frame_data = frame.serialize();
    return tcp_conn_.send(std::string(frame_data.begin(), frame_data.end()));
}

void WebSocketConnection::process_frame(const WebSocketFrame& frame) {
    if (frame.is_control_frame()) {
        handle_control_frame(frame);
    } else {
        handle_data_frame(frame);
    }
}

void WebSocketConnection::handle_control_frame(const WebSocketFrame& frame) {
    switch (frame.opcode()) {
        case WebSocketOpcode::Close: {
            auto close_info_result = websocket_utils::parse_close_frame(frame);
            WebSocketCloseCode code = WebSocketCloseCode::Normal;
            std::string reason;
            
            if (close_info_result) {
                auto close_info = close_info_result.value();
                code = close_info.code;
                reason = close_info.reason;
            }
            
            if (state_ == WebSocketState::Open) {
                // Send close response
                state_ = WebSocketState::Closing;
                auto close_frame = websocket_utils::create_close_frame(code, "");
                if (is_client_) {
                    close_frame.generate_masking_key();
                }
                send_frame(close_frame);
            }
            
            close(code, reason);
            break;
        }
        
        case WebSocketOpcode::Ping: {
            // Send pong response with same payload
            if (auto_pong_) {
                send_pong(frame.payload_as_string());
            }
            break;
        }
        
        case WebSocketOpcode::Pong: {
            // Handle pong frame (usually for keepalive)
            break;
        }
        
        default:
            // Unknown control frame
            break;
    }
}

void WebSocketConnection::handle_data_frame(const WebSocketFrame& frame) {
    WebSocketFrameType frame_type = frame.frame_type();
    
    // Find or create partial message
    auto& partial_message = partial_messages_[frame_type];
    
    if (partial_message.data().empty()) {
        // Start new message
        if (frame_type == WebSocketFrameType::Continuation) {
            if (error_handler_) {
                error_handler_("Unexpected continuation frame");
            }
            return;
        }
        partial_message = WebSocketMessage(frame_type);
    } else {
        // Continue existing message
        if (frame_type != WebSocketFrameType::Continuation) {
            if (error_handler_) {
                error_handler_("Expected continuation frame");
            }
            return;
        }
    }
    
    partial_message.append_frame(frame);
    
    // Check if message is complete
    if (partial_message.is_complete()) {
        auto complete_message = std::move(partial_message);
        partial_messages_.erase(frame_type);
        
        // Validate text message UTF-8
        if (complete_message.type() == WebSocketFrameType::Text) {
            if (!websocket_utils::is_valid_utf8(complete_message.data())) {
                if (error_handler_) {
                    error_handler_("Invalid UTF-8 in text message");
                }
                return;
            }
        }
        
        // Call message handler
        if (message_handler_) {
            message_handler_(complete_message);
        }
    }
}

void WebSocketConnection::async_receive_loop() {
    while (receiving_.load() && state_ == WebSocketState::Open) {
        try {
            auto frame_result = receive_frame();
            if (!frame_result) {
                if (error_handler_) {
                    error_handler_("Failed to receive frame: " + frame_result.error().message);
                }
                break;
            }
            
            auto frame = frame_result.value();
            process_frame(frame);
            
        } catch (const std::exception& e) {
            if (error_handler_) {
                error_handler_("Exception in receive loop: " + std::string(e.what()));
            }
            break;
        }
    }
    
    receiving_.store(false);
}

} // namespace networkquests::websocket