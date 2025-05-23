#include "networkquests/http.hpp"
#include <regex>

namespace networkquests::http {

HttpClient::HttpClient() 
    : timeout_(std::chrono::seconds(30))
    , user_agent_("NetworkQuests/1.0") {
}

HttpClient::HttpClient(std::chrono::milliseconds timeout)
    : timeout_(timeout)
    , user_agent_("NetworkQuests/1.0") {
}

Result<HttpResponse> HttpClient::get(std::string_view url, const HttpHeaders& headers) {
    HttpRequest request(HttpMethod::GET, "/");
    
    // Merge headers
    for (const auto& [name, value] : default_headers_) {
        request.set_header(name, value);
    }
    for (const auto& [name, value] : headers) {
        request.set_header(name, value);
    }
    
    // Set User-Agent if not provided
    if (!request.get_header("user-agent")) {
        request.set_user_agent(user_agent_);
    }
    
    auto url_parts = parse_url(url);
    if (!url_parts) {
        return make_error_result<HttpResponse>(url_parts.error());
    }
    
    request.set_uri(url_parts.value().path);
    request.set_header("host", url_parts.value().host);
    
    return send_request(request, url_parts.value().host, url_parts.value().port);
}

Result<HttpResponse> HttpClient::post(std::string_view url, std::string_view body, 
                                     const HttpHeaders& headers) {
    HttpRequest request(HttpMethod::POST, "/");
    request.set_body(body);
    request.set_content_length(body.size());
    
    // Merge headers
    for (const auto& [name, value] : default_headers_) {
        request.set_header(name, value);
    }
    for (const auto& [name, value] : headers) {
        request.set_header(name, value);
    }
    
    // Set User-Agent if not provided
    if (!request.get_header("user-agent")) {
        request.set_user_agent(user_agent_);
    }
    
    // Set default Content-Type if not provided
    if (!request.get_header("content-type")) {
        request.set_content_type("text/plain");
    }
    
    auto url_parts = parse_url(url);
    if (!url_parts) {
        return make_error_result<HttpResponse>(url_parts.error());
    }
    
    request.set_uri(url_parts.value().path);
    request.set_header("host", url_parts.value().host);
    
    return send_request(request, url_parts.value().host, url_parts.value().port);
}

Result<HttpResponse> HttpClient::put(std::string_view url, std::string_view body,
                                    const HttpHeaders& headers) {
    HttpRequest request(HttpMethod::PUT, "/");
    request.set_body(body);
    request.set_content_length(body.size());
    
    // Merge headers
    for (const auto& [name, value] : default_headers_) {
        request.set_header(name, value);
    }
    for (const auto& [name, value] : headers) {
        request.set_header(name, value);
    }
    
    // Set User-Agent if not provided
    if (!request.get_header("user-agent")) {
        request.set_user_agent(user_agent_);
    }
    
    // Set default Content-Type if not provided
    if (!request.get_header("content-type")) {
        request.set_content_type("text/plain");
    }
    
    auto url_parts = parse_url(url);
    if (!url_parts) {
        return make_error_result<HttpResponse>(url_parts.error());
    }
    
    request.set_uri(url_parts.value().path);
    request.set_header("host", url_parts.value().host);
    
    return send_request(request, url_parts.value().host, url_parts.value().port);
}

Result<HttpResponse> HttpClient::delete_resource(std::string_view url, const HttpHeaders& headers) {
    HttpRequest request(HttpMethod::DELETE, "/");
    
    // Merge headers
    for (const auto& [name, value] : default_headers_) {
        request.set_header(name, value);
    }
    for (const auto& [name, value] : headers) {
        request.set_header(name, value);
    }
    
    // Set User-Agent if not provided
    if (!request.get_header("user-agent")) {
        request.set_user_agent(user_agent_);
    }
    
    auto url_parts = parse_url(url);
    if (!url_parts) {
        return make_error_result<HttpResponse>(url_parts.error());
    }
    
    request.set_uri(url_parts.value().path);
    request.set_header("host", url_parts.value().host);
    
    return send_request(request, url_parts.value().host, url_parts.value().port);
}

Result<HttpResponse> HttpClient::head(std::string_view url, const HttpHeaders& headers) {
    HttpRequest request(HttpMethod::HEAD, "/");
    
    // Merge headers
    for (const auto& [name, value] : default_headers_) {
        request.set_header(name, value);
    }
    for (const auto& [name, value] : headers) {
        request.set_header(name, value);
    }
    
    // Set User-Agent if not provided
    if (!request.get_header("user-agent")) {
        request.set_user_agent(user_agent_);
    }
    
    auto url_parts = parse_url(url);
    if (!url_parts) {
        return make_error_result<HttpResponse>(url_parts.error());
    }
    
    request.set_uri(url_parts.value().path);
    request.set_header("host", url_parts.value().host);
    
    return send_request(request, url_parts.value().host, url_parts.value().port);
}

Result<HttpResponse> HttpClient::send_request(const HttpRequest& request, std::string_view host, Port port) {
    LOG_DEBUG("Sending HTTP {} request to {}:{}{}", 
              http_utils::method_to_string(request.method()),
              host, port, request.uri());
    
    try {
        // Create TCP client
        tcp::TcpClient client(std::string(host), port);
        
        // Set connection timeout
        auto connect_result = client.connect(timeout_);
        if (!connect_result) {
            LOG_ERROR("Failed to connect to {}:{}: {}", host, port, connect_result.error().message());
            return make_error_result<HttpResponse>(connect_result.error());
        }
        
        auto& connection = connect_result.value();
        
        // Send request
        std::string request_str = request.to_string();
        auto send_result = connection.send_string(request_str);
        if (!send_result) {
            LOG_ERROR("Failed to send HTTP request: {}", send_result.error().message());
            return make_error_result<HttpResponse>(send_result.error());
        }
        
        LOG_DEBUG("Sent {} bytes", send_result.value());
        
        // Receive response
        std::string response_data;
        std::string chunk;
        bool headers_complete = false;
        size_t content_length = 0;
        bool has_content_length = false;
        
        // First, read until we have complete headers
        while (!headers_complete) {
            auto recv_result = connection.receive_string(timeout_);
            if (!recv_result) {
                LOG_ERROR("Failed to receive HTTP response: {}", recv_result.error().message());
                return make_error_result<HttpResponse>(recv_result.error());
            }
            
            chunk = recv_result.value();
            response_data += chunk;
            
            // Check if we have complete headers
            size_t header_end = response_data.find("\r\n\r\n");
            if (header_end != std::string::npos) {
                headers_complete = true;
                
                // Parse Content-Length from headers
                std::string_view headers_part = std::string_view(response_data).substr(0, header_end);
                std::regex content_length_regex(R"(content-length\s*:\s*(\d+))", std::regex_constants::icase);
                std::smatch match;
                std::string headers_str(headers_part);
                
                if (std::regex_search(headers_str, match, content_length_regex)) {
                    content_length = std::stoull(match[1].str());
                    has_content_length = true;
                    LOG_DEBUG("Found Content-Length: {}", content_length);
                }
                
                // Calculate how much body we already have
                size_t body_start = header_end + 4;
                size_t body_received = response_data.size() - body_start;
                
                // If we need more body data, continue reading
                if (has_content_length && body_received < content_length) {
                    size_t remaining = content_length - body_received;
                    LOG_DEBUG("Need to read {} more bytes of body", remaining);
                    
                    while (remaining > 0) {
                        auto body_result = connection.receive_string(timeout_);
                        if (!body_result) {
                            LOG_ERROR("Failed to receive HTTP response body: {}", body_result.error().message());
                            return make_error_result<HttpResponse>(body_result.error());
                        }
                        
                        std::string body_chunk = body_result.value();
                        response_data += body_chunk;
                        remaining -= body_chunk.size();
                        
                        LOG_DEBUG("Received {} bytes, {} remaining", body_chunk.size(), remaining);
                    }
                }
            }
            
            // Prevent infinite loop
            if (response_data.size() > 1024 * 1024) { // 1MB limit
                LOG_ERROR("HTTP response too large");
                return make_error_result<HttpResponse>(NetworkError::ProtocolError);
            }
        }
        
        LOG_DEBUG("Received complete HTTP response ({} bytes)", response_data.size());
        
        // Parse response
        auto response_result = HttpResponse::from_string(response_data);
        if (!response_result) {
            LOG_ERROR("Failed to parse HTTP response: {}", response_result.error().message());
            return response_result;
        }
        
        auto response = response_result.value();
        LOG_DEBUG("HTTP response: {} {}", 
                  static_cast<int>(response.status()),
                  http_utils::status_description(response.status()));
        
        return Result<HttpResponse>::success(std::move(response));
        
    } catch (const std::exception& e) {
        LOG_ERROR("HTTP request failed: {}", e.what());
        return make_error_result<HttpResponse>(NetworkError::ConnectionFailed);
    }
}

Result<HttpClient::UrlParts> HttpClient::parse_url(std::string_view url) const {
    UrlParts parts;
    
    // Regular expression for URL parsing
    // Matches: scheme://host:port/path?query#fragment
    std::regex url_regex(R"(^(https?)://([^:/\?#]+)(?::(\d+))?(/[^\?#]*)?(?:\?([^#]*))?(?:#(.*))?$)");
    std::smatch match;
    std::string url_str(url);
    
    if (!std::regex_match(url_str, match, url_regex)) {
        LOG_ERROR("Invalid URL format: {}", url);
        return make_error_result<UrlParts>(NetworkError::InvalidAddress);
    }
    
    parts.scheme = match[1].str();
    parts.host = match[2].str();
    
    // Parse port
    if (match[3].matched) {
        try {
            parts.port = static_cast<Port>(std::stoi(match[3].str()));
        } catch (const std::exception& e) {
            LOG_ERROR("Invalid port in URL: {}", match[3].str());
            return make_error_result<UrlParts>(NetworkError::InvalidAddress);
        }
    } else {
        // Default ports
        if (parts.scheme == "http") {
            parts.port = 80;
        } else if (parts.scheme == "https") {
            parts.port = 443;
        } else {
            LOG_ERROR("Unsupported scheme: {}", parts.scheme);
            return make_error_result<UrlParts>(NetworkError::ProtocolError);
        }
    }
    
    // Parse path
    if (match[4].matched) {
        parts.path = match[4].str();
        
        // Add query string if present
        if (match[5].matched) {
            parts.path += "?" + match[5].str();
        }
    } else {
        parts.path = "/";
    }
    
    LOG_DEBUG("Parsed URL: scheme={}, host={}, port={}, path={}", 
              parts.scheme, parts.host, parts.port, parts.path);
    
    return Result<UrlParts>::success(std::move(parts));
}

} // namespace networkquests::http