#include "networkquests/websocket.hpp"

#include <algorithm>

namespace networkquests::websocket {

// WebSocketServer implementation

WebSocketServer::WebSocketServer(Port port) 
    : bind_addr_(SocketAddress::any_address(port)) {}

WebSocketServer::WebSocketServer(const SocketAddress& bind_addr) 
    : bind_addr_(bind_addr) {}

void WebSocketServer::add_supported_subprotocol(std::string_view protocol) {
    supported_subprotocols_.emplace_back(protocol);
}

void WebSocketServer::add_supported_extension(WebSocketExtension extension) {
    supported_extensions_.push_back(extension);
}

SocketAddress WebSocketServer::local_address() const {
    if (tcp_server_) {
        return tcp_server_->local_address();
    }
    return bind_addr_;
}

Result<void> WebSocketServer::start() {
    if (running_.load()) {
        return make_error("Server already running");
    }
    
    // Create TCP server
    tcp_server_ = std::make_unique<tcp::TcpServer>(bind_addr_);
    
    // Start TCP server
    auto start_result = tcp_server_->start();
    if (!start_result) {
        return start_result;
    }
    
    running_.store(true);
    
    // Set connection handler
    tcp_server_->set_connection_handler([this](tcp::TcpConnection connection) {
        handle_client(std::move(connection));
    });
    
    return make_success();
}

void WebSocketServer::stop() {
    if (!running_.load()) {
        return;
    }
    
    running_.store(false);
    
    if (tcp_server_) {
        tcp_server_->stop();
        tcp_server_.reset();
    }
}

void WebSocketServer::handle_client(tcp::TcpConnection connection) {
    if (!running_.load()) {
        return;
    }
    
    // Check connection limit
    if (static_cast<int>(active_connections_.load()) >= max_connections_) {
        return; // Reject connection
    }
    
    active_connections_.fetch_add(1);
    
    try {
        // Receive HTTP request
        auto request_data_result = connection.receive(4096, std::chrono::seconds(30));
        if (!request_data_result) {
            active_connections_.fetch_sub(1);
            return;
        }
        
        auto request_data = request_data_result.value();
        auto request_result = http::HttpRequest::from_string(request_data);
        if (!request_result) {
            active_connections_.fetch_sub(1);
            return;
        }
        
        auto request = request_result.value();
        
        // Validate WebSocket request
        if (!validate_websocket_request(request)) {
            // Send 400 Bad Request
            http::HttpResponse response(http::HttpStatusCode::BadRequest);
            response.set_body("Bad Request: Invalid WebSocket handshake");
            auto response_str = response.to_string();
            connection.send(response_str);
            active_connections_.fetch_sub(1);
            return;
        }
        
        // Check with handshake validator
        if (handshake_validator_ && !handshake_validator_(request)) {
            // Send 403 Forbidden
            http::HttpResponse response(http::HttpStatusCode::Forbidden);
            response.set_body("Forbidden: Handshake validation failed");
            auto response_str = response.to_string();
            connection.send(response_str);
            active_connections_.fetch_sub(1);
            return;
        }
        
        // Process handshake
        auto response_result = process_handshake(request);
        if (!response_result) {
            // Send 400 Bad Request
            http::HttpResponse response(http::HttpStatusCode::BadRequest);
            response.set_body("Bad Request: " + response_result.error().message);
            auto response_str = response.to_string();
            connection.send(response_str);
            active_connections_.fetch_sub(1);
            return;
        }
        
        auto response = response_result.value();
        
        // Send handshake response
        auto response_str = response.to_string();
        auto send_result = connection.send(response_str);
        if (!send_result) {
            active_connections_.fetch_sub(1);
            return;
        }
        
        // Create WebSocket connection
        auto ws_connection = std::make_unique<WebSocketConnection>(std::move(connection));
        ws_connection->state_ = WebSocketState::Open;
        ws_connection->is_client_ = false;
        
        // Set close handler to decrement connection count
        ws_connection->set_close_handler([this](WebSocketCloseCode, std::string_view) {
            active_connections_.fetch_sub(1);
        });
        
        // Call connection handler
        if (connection_handler_) {
            connection_handler_(std::move(ws_connection));
        }
        
    } catch (const std::exception&) {
        active_connections_.fetch_sub(1);
    }
}

Result<http::HttpResponse> WebSocketServer::process_handshake(const http::HttpRequest& request) {
    // Get WebSocket key
    auto ws_key = request.get_header("Sec-WebSocket-Key");
    if (!ws_key) {
        return make_error("Missing Sec-WebSocket-Key header");
    }
    
    // Create response
    http::HttpResponse response(http::HttpStatusCode::SwitchingProtocols);
    response.set_header("Upgrade", "websocket");
    response.set_header("Connection", "Upgrade");
    response.set_header("Sec-WebSocket-Accept", generate_websocket_accept(ws_key.value()));
    
    // Handle subprotocol negotiation
    auto requested_protocols = request.get_header("Sec-WebSocket-Protocol");
    if (requested_protocols) {
        std::string selected_protocol = select_subprotocol(requested_protocols.value());
        if (!selected_protocol.empty()) {
            response.set_header("Sec-WebSocket-Protocol", selected_protocol);
        }
    }
    
    // Handle extension negotiation
    auto requested_extensions = request.get_header("Sec-WebSocket-Extensions");
    if (requested_extensions) {
        auto selected_extensions = select_extensions(requested_extensions.value());
        if (!selected_extensions.empty()) {
            response.set_header("Sec-WebSocket-Extensions", 
                              websocket_utils::build_extension_header(selected_extensions));
        }
    }
    
    return response;
}

bool WebSocketServer::validate_websocket_request(const http::HttpRequest& request) {
    // Check HTTP method
    if (request.method() != http::HttpMethod::GET) {
        return false;
    }
    
    // Check HTTP version
    if (request.version() != http::HttpVersion::HTTP_1_1) {
        return false;
    }
    
    // Check required headers
    auto host = request.get_header("Host");
    if (!host) return false;
    
    auto upgrade = request.get_header("Upgrade");
    if (!upgrade || upgrade.value() != "websocket") return false;
    
    auto connection = request.get_header("Connection");
    if (!connection || connection.value().find("Upgrade") == std::string::npos) return false;
    
    auto ws_key = request.get_header("Sec-WebSocket-Key");
    if (!ws_key) return false;
    
    auto ws_version = request.get_header("Sec-WebSocket-Version");
    if (!ws_version || ws_version.value() != websocket_utils::constants::WEBSOCKET_VERSION) return false;
    
    // Validate WebSocket key (should be 16 bytes base64 encoded)
    auto key_bytes_result = websocket_utils::base64_decode(ws_key.value());
    if (!key_bytes_result || key_bytes_result.value().size() != 16) {
        return false;
    }
    
    return true;
}

std::string WebSocketServer::select_subprotocol(const std::string& requested_protocols) {
    auto protocols = websocket_utils::parse_subprotocol_list(requested_protocols);
    
    // Find first supported protocol
    for (const auto& requested : protocols) {
        for (const auto& supported : supported_subprotocols_) {
            if (requested == supported) {
                return requested;
            }
        }
    }
    
    return ""; // No matching protocol
}

std::vector<WebSocketExtension> WebSocketServer::select_extensions(const std::string& requested_extensions) {
    auto extensions = websocket_utils::parse_extension_list(requested_extensions);
    std::vector<WebSocketExtension> selected;
    
    // Simple extension matching (can be enhanced)
    for (const auto& requested : extensions) {
        if (requested.find("permessage-deflate") != std::string::npos) {
            // Check if we support it
            auto it = std::find(supported_extensions_.begin(), supported_extensions_.end(), 
                              WebSocketExtension::PerMessageDeflate);
            if (it != supported_extensions_.end()) {
                selected.push_back(WebSocketExtension::PerMessageDeflate);
            }
        }
    }
    
    return selected;
}

std::string WebSocketServer::generate_websocket_accept(std::string_view key) {
    std::string combined = std::string(key) + websocket_utils::constants::WEBSOCKET_MAGIC_STRING;
    std::string hash = websocket_utils::sha1_hash(combined);
    return websocket_utils::base64_encode(hash);
}

} // namespace networkquests::websocket