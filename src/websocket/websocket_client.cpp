#include "networkquests/websocket.hpp"

#include <regex>
#include <random>

namespace networkquests::websocket {

// WebSocketClient implementation

WebSocketClient::WebSocketClient() {
    // Set default headers
    headers_["User-Agent"] = "NetworkQuests-WebSocket/1.0";
}

WebSocketClient::WebSocketClient(const http::HttpHeaders& headers) 
    : headers_(headers) {
    if (headers_.find("User-Agent") == headers_.end()) {
        headers_["User-Agent"] = "NetworkQuests-WebSocket/1.0";
    }
}

void WebSocketClient::add_header(std::string_view name, std::string_view value) {
    headers_[std::string(name)] = std::string(value);
}

Result<std::unique_ptr<WebSocketConnection>> WebSocketClient::connect(std::string_view url) {
    auto url_parts_result = parse_url(url);
    if (!url_parts_result) {
        return url_parts_result.error();
    }
    
    auto url_parts = url_parts_result.value();
    return connect(url_parts.host, url_parts.port, url_parts.path);
}

Result<std::unique_ptr<WebSocketConnection>> WebSocketClient::connect(std::string_view host, Port port, 
                                                                    std::string_view path) {
    // Create TCP connection
    tcp::TcpClient tcp_client;
    auto tcp_conn_result = tcp_client.connect(host, port, timeout_);
    if (!tcp_conn_result) {
        return tcp_conn_result.error();
    }
    
    auto tcp_conn = std::move(tcp_conn_result.value());
    
    // Perform WebSocket handshake
    auto handshake_request_result = create_handshake_request(host, port, path);
    if (!handshake_request_result) {
        return handshake_request_result.error();
    }
    
    auto handshake_request = handshake_request_result.value();
    std::string websocket_key = handshake_request.get_header("Sec-WebSocket-Key").value_or("");
    
    // Send handshake request
    std::string request_str = handshake_request.to_string();
    auto send_result = tcp_conn.send(request_str);
    if (!send_result) {
        return send_result.error();
    }
    
    // Receive handshake response
    auto response_data_result = tcp_conn.receive(4096, timeout_);
    if (!response_data_result) {
        return response_data_result.error();
    }
    
    auto response_data = response_data_result.value();
    auto response_result = http::HttpResponse::from_string(response_data);
    if (!response_result) {
        return make_error("Invalid HTTP response: " + response_result.error().message);
    }
    
    auto response = response_result.value();
    
    // Validate handshake response
    auto validate_result = validate_handshake_response(response, websocket_key);
    if (!validate_result) {
        return validate_result.error();
    }
    
    // Parse selected subprotocol and extensions
    auto subprotocol_header = response.get_header("Sec-WebSocket-Protocol");
    if (subprotocol_header) {
        selected_subprotocol_ = subprotocol_header.value();
    }
    
    auto extensions_header = response.get_header("Sec-WebSocket-Extensions");
    if (extensions_header) {
        auto extension_list = websocket_utils::parse_extension_list(extensions_header.value());
        // Parse extensions (simplified for now)
        selected_extensions_.clear();
        for (const auto& ext : extension_list) {
            if (ext.find("permessage-deflate") != std::string::npos) {
                selected_extensions_.push_back(WebSocketExtension::PerMessageDeflate);
            }
        }
    }
    
    // Create WebSocket connection
    auto ws_conn = std::make_unique<WebSocketConnection>(std::move(tcp_conn));
    ws_conn->state_ = WebSocketState::Open;
    ws_conn->is_client_ = true;
    
    return ws_conn;
}

Result<http::HttpRequest> WebSocketClient::create_handshake_request(std::string_view host, Port port, 
                                                                  std::string_view path) {
    http::HttpRequest request(http::HttpMethod::GET, path, http::HttpVersion::HTTP_1_1);
    
    // Required WebSocket headers
    std::string host_header = std::string(host);
    if (port != 80 && port != 443) {
        host_header += ":" + std::to_string(port);
    }
    request.set_header("Host", host_header);
    request.set_header("Upgrade", "websocket");
    request.set_header("Connection", "Upgrade");
    request.set_header("Sec-WebSocket-Key", generate_websocket_key());
    request.set_header("Sec-WebSocket-Version", websocket_utils::constants::WEBSOCKET_VERSION);
    
    // Optional headers
    if (!subprotocol_.empty()) {
        request.set_header("Sec-WebSocket-Protocol", subprotocol_);
    }
    
    if (!extensions_.empty()) {
        request.set_header("Sec-WebSocket-Extensions", 
                          websocket_utils::build_extension_header(extensions_));
    }
    
    // Add custom headers
    for (const auto& [name, value] : headers_) {
        request.set_header(name, value);
    }
    
    return request;
}

Result<void> WebSocketClient::validate_handshake_response(const http::HttpResponse& response, 
                                                        std::string_view websocket_key) {
    // Check status code
    if (response.status() != http::HttpStatusCode::SwitchingProtocols) {
        return make_error("Expected 101 Switching Protocols, got " + 
                         std::to_string(static_cast<int>(response.status())));
    }
    
    // Check upgrade header
    auto upgrade_header = response.get_header("Upgrade");
    if (!upgrade_header || upgrade_header.value() != "websocket") {
        return make_error("Missing or invalid Upgrade header");
    }
    
    // Check connection header
    auto connection_header = response.get_header("Connection");
    if (!connection_header || connection_header.value().find("Upgrade") == std::string::npos) {
        return make_error("Missing or invalid Connection header");
    }
    
    // Check Sec-WebSocket-Accept
    auto accept_header = response.get_header("Sec-WebSocket-Accept");
    if (!accept_header) {
        return make_error("Missing Sec-WebSocket-Accept header");
    }
    
    std::string expected_accept = calculate_websocket_accept(websocket_key);
    if (accept_header.value() != expected_accept) {
        return make_error("Invalid Sec-WebSocket-Accept header");
    }
    
    return make_success();
}

std::string WebSocketClient::generate_websocket_key() {
    // Generate 16 random bytes
    std::vector<uint8_t> key_bytes(16);
    
    static thread_local std::random_device rd;
    static thread_local std::mt19937 gen(rd());
    static thread_local std::uniform_int_distribution<uint8_t> dis;
    
    for (auto& byte : key_bytes) {
        byte = dis(gen);
    }
    
    return websocket_utils::base64_encode(key_bytes);
}

std::string WebSocketClient::calculate_websocket_accept(std::string_view key) {
    std::string combined = std::string(key) + websocket_utils::constants::WEBSOCKET_MAGIC_STRING;
    std::string hash = websocket_utils::sha1_hash(combined);
    return websocket_utils::base64_encode(hash);
}

Result<WebSocketClient::UrlParts> WebSocketClient::parse_url(std::string_view url) const {
    // Regular expression for WebSocket URL parsing
    std::regex url_regex(R"(^(ws|wss)://([^:/]+)(?::(\d+))?(/.*)?$)");
    std::string url_str(url);
    std::smatch matches;
    
    if (!std::regex_match(url_str, matches, url_regex)) {
        return make_error("Invalid WebSocket URL format");
    }
    
    UrlParts parts;
    parts.scheme = matches[1].str();
    parts.host = matches[2].str();
    
    if (matches[3].matched) {
        try {
            parts.port = static_cast<Port>(std::stoi(matches[3].str()));
        } catch (const std::exception&) {
            return make_error("Invalid port number");
        }
    } else {
        parts.port = (parts.scheme == "wss") ? 443 : 80;
    }
    
    parts.path = matches[4].matched ? matches[4].str() : "/";
    
    return parts;
}

} // namespace networkquests::websocket