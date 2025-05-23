#include "networkquests/http.hpp"
#include <sstream>
#include <algorithm>
#include <cctype>

namespace networkquests::http {

// HttpRequest implementation
HttpRequest::HttpRequest(HttpMethod method, std::string_view uri, HttpVersion version)
    : method_(method), uri_(uri), version_(version) {
}

void HttpRequest::set_header(std::string_view name, std::string_view value) {
    std::string name_lower(name);
    std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);
    headers_[name_lower] = value;
}

std::optional<std::string> HttpRequest::get_header(std::string_view name) const {
    std::string name_lower(name);
    std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);
    
    auto it = headers_.find(name_lower);
    if (it != headers_.end()) {
        return it->second;
    }
    return std::nullopt;
}

void HttpRequest::remove_header(std::string_view name) {
    std::string name_lower(name);
    std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);
    headers_.erase(name_lower);
}

void HttpRequest::set_content_type(std::string_view content_type) {
    set_header("content-type", content_type);
}

void HttpRequest::set_content_length(size_t length) {
    set_header("content-length", std::to_string(length));
}

void HttpRequest::set_user_agent(std::string_view user_agent) {
    set_header("user-agent", user_agent);
}

void HttpRequest::set_authorization(std::string_view auth) {
    set_header("authorization", auth);
}

std::string HttpRequest::to_string() const {
    std::ostringstream oss;
    
    // Request line
    oss << http_utils::method_to_string(method_) << " " 
        << uri_ << " " 
        << http_utils::version_to_string(version_) << "\r\n";
    
    // Headers
    for (const auto& [name, value] : headers_) {
        oss << name << ": " << value << "\r\n";
    }
    
    // Empty line separating headers from body
    oss << "\r\n";
    
    // Body
    if (!body_.empty()) {
        oss << body_;
    }
    
    return oss.str();
}

Result<HttpRequest> HttpRequest::from_string(std::string_view data) {
    HttpRequest request;
    
    // Find the end of headers (double CRLF)
    size_t header_end = data.find("\r\n\r\n");
    if (header_end == std::string_view::npos) {
        return make_error_result<HttpRequest>(NetworkError::ProtocolError);
    }
    
    std::string_view headers_part = data.substr(0, header_end);
    std::string_view body_part = data.substr(header_end + 4);
    
    // Parse headers line by line
    std::istringstream header_stream(std::string(headers_part));
    std::string line;
    bool first_line = true;
    
    while (std::getline(header_stream, line)) {
        // Remove carriage return if present
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        
        if (first_line) {
            // Parse request line: "METHOD URI VERSION"
            std::istringstream request_line(line);
            std::string method_str, uri_str, version_str;
            
            if (!(request_line >> method_str >> uri_str >> version_str)) {
                return make_error_result<HttpRequest>(NetworkError::ProtocolError);
            }
            
            auto method_result = http_utils::string_to_method(method_str);
            if (!method_result) {
                return make_error_result<HttpRequest>(method_result.error());
            }
            
            auto version_result = http_utils::string_to_version(version_str);
            if (!version_result) {
                return make_error_result<HttpRequest>(version_result.error());
            }
            
            request.method_ = method_result.value();
            request.uri_ = uri_str;
            request.version_ = version_result.value();
            
            first_line = false;
        } else {
            // Parse header line: "Name: Value"
            size_t colon_pos = line.find(':');
            if (colon_pos == std::string::npos) {
                continue; // Skip malformed header lines
            }
            
            std::string name = line.substr(0, colon_pos);
            std::string value = line.substr(colon_pos + 1);
            
            // Trim whitespace
            name.erase(0, name.find_first_not_of(" \t"));
            name.erase(name.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);
            
            request.set_header(name, value);
        }
    }
    
    // Set body
    request.body_ = body_part;
    
    // Validate Content-Length if present
    auto content_length = request.get_header("content-length");
    if (content_length) {
        try {
            size_t expected_length = std::stoull(*content_length);
            if (body_part.size() != expected_length) {
                LOG_WARNING("Content-Length mismatch: expected {}, got {}", 
                           expected_length, body_part.size());
            }
        } catch (const std::exception& e) {
            LOG_WARNING("Invalid Content-Length header: {}", *content_length);
        }
    }
    
    return Result<HttpRequest>::success(std::move(request));
}

// HttpResponse implementation
HttpResponse::HttpResponse(HttpStatusCode status, HttpVersion version)
    : status_(status), version_(version) {
}

void HttpResponse::set_header(std::string_view name, std::string_view value) {
    std::string name_lower(name);
    std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);
    headers_[name_lower] = value;
}

std::optional<std::string> HttpResponse::get_header(std::string_view name) const {
    std::string name_lower(name);
    std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);
    
    auto it = headers_.find(name_lower);
    if (it != headers_.end()) {
        return it->second;
    }
    return std::nullopt;
}

void HttpResponse::remove_header(std::string_view name) {
    std::string name_lower(name);
    std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);
    headers_.erase(name_lower);
}

void HttpResponse::set_content_type(std::string_view content_type) {
    set_header("content-type", content_type);
}

void HttpResponse::set_content_length(size_t length) {
    set_header("content-length", std::to_string(length));
}

void HttpResponse::set_server(std::string_view server) {
    set_header("server", server);
}

void HttpResponse::set_cache_control(std::string_view cache_control) {
    set_header("cache-control", cache_control);
}

std::string HttpResponse::to_string() const {
    std::ostringstream oss;
    
    // Status line
    oss << http_utils::version_to_string(version_) << " "
        << static_cast<int>(status_) << " "
        << http_utils::status_description(status_) << "\r\n";
    
    // Headers
    for (const auto& [name, value] : headers_) {
        oss << name << ": " << value << "\r\n";
    }
    
    // Empty line separating headers from body
    oss << "\r\n";
    
    // Body
    if (!body_.empty()) {
        oss << body_;
    }
    
    return oss.str();
}

Result<HttpResponse> HttpResponse::from_string(std::string_view data) {
    HttpResponse response;
    
    // Find the end of headers (double CRLF)
    size_t header_end = data.find("\r\n\r\n");
    if (header_end == std::string_view::npos) {
        return make_error_result<HttpResponse>(NetworkError::ProtocolError);
    }
    
    std::string_view headers_part = data.substr(0, header_end);
    std::string_view body_part = data.substr(header_end + 4);
    
    // Parse headers line by line
    std::istringstream header_stream(std::string(headers_part));
    std::string line;
    bool first_line = true;
    
    while (std::getline(header_stream, line)) {
        // Remove carriage return if present
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        
        if (first_line) {
            // Parse status line: "VERSION STATUS_CODE REASON_PHRASE"
            std::istringstream status_line(line);
            std::string version_str, status_str;
            
            if (!(status_line >> version_str >> status_str)) {
                return make_error_result<HttpResponse>(NetworkError::ProtocolError);
            }
            
            auto version_result = http_utils::string_to_version(version_str);
            if (!version_result) {
                return make_error_result<HttpResponse>(version_result.error());
            }
            
            try {
                int status_code = std::stoi(status_str);
                response.status_ = static_cast<HttpStatusCode>(status_code);
            } catch (const std::exception& e) {
                return make_error_result<HttpResponse>(NetworkError::ProtocolError);
            }
            
            response.version_ = version_result.value();
            first_line = false;
        } else {
            // Parse header line: "Name: Value"
            size_t colon_pos = line.find(':');
            if (colon_pos == std::string::npos) {
                continue; // Skip malformed header lines
            }
            
            std::string name = line.substr(0, colon_pos);
            std::string value = line.substr(colon_pos + 1);
            
            // Trim whitespace
            name.erase(0, name.find_first_not_of(" \t"));
            name.erase(name.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);
            
            response.set_header(name, value);
        }
    }
    
    // Set body
    response.body_ = body_part;
    
    // Validate Content-Length if present
    auto content_length = response.get_header("content-length");
    if (content_length) {
        try {
            size_t expected_length = std::stoull(*content_length);
            if (body_part.size() != expected_length) {
                LOG_WARNING("Content-Length mismatch: expected {}, got {}", 
                           expected_length, body_part.size());
            }
        } catch (const std::exception& e) {
            LOG_WARNING("Invalid Content-Length header: {}", *content_length);
        }
    }
    
    return Result<HttpResponse>::success(std::move(response));
}

} // namespace networkquests::http