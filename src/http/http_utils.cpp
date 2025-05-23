#include "networkquests/http.hpp"
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <cctype>
#include <cstdlib>

namespace networkquests::http::http_utils {

std::string_view method_to_string(HttpMethod method) {
    switch (method) {
        case HttpMethod::GET:     return "GET";
        case HttpMethod::POST:    return "POST";
        case HttpMethod::PUT:     return "PUT";
        case HttpMethod::DELETE:  return "DELETE";
        case HttpMethod::HEAD:    return "HEAD";
        case HttpMethod::OPTIONS: return "OPTIONS";
        case HttpMethod::PATCH:   return "PATCH";
        case HttpMethod::CONNECT: return "CONNECT";
        case HttpMethod::TRACE:   return "TRACE";
        default:                  return "UNKNOWN";
    }
}

Result<HttpMethod> string_to_method(std::string_view method) {
    std::string method_upper(method);
    std::transform(method_upper.begin(), method_upper.end(), method_upper.begin(), ::toupper);
    
    if (method_upper == "GET")     return Result<HttpMethod>::success(HttpMethod::GET);
    if (method_upper == "POST")    return Result<HttpMethod>::success(HttpMethod::POST);
    if (method_upper == "PUT")     return Result<HttpMethod>::success(HttpMethod::PUT);
    if (method_upper == "DELETE")  return Result<HttpMethod>::success(HttpMethod::DELETE);
    if (method_upper == "HEAD")    return Result<HttpMethod>::success(HttpMethod::HEAD);
    if (method_upper == "OPTIONS") return Result<HttpMethod>::success(HttpMethod::OPTIONS);
    if (method_upper == "PATCH")   return Result<HttpMethod>::success(HttpMethod::PATCH);
    if (method_upper == "CONNECT") return Result<HttpMethod>::success(HttpMethod::CONNECT);
    if (method_upper == "TRACE")   return Result<HttpMethod>::success(HttpMethod::TRACE);
    
    return make_error_result<HttpMethod>(NetworkError::ProtocolError);
}

std::string_view version_to_string(HttpVersion version) {
    switch (version) {
        case HttpVersion::HTTP_1_0: return "HTTP/1.0";
        case HttpVersion::HTTP_1_1: return "HTTP/1.1";
        default:                    return "HTTP/1.1";
    }
}

Result<HttpVersion> string_to_version(std::string_view version) {
    if (version == "HTTP/1.0") return Result<HttpVersion>::success(HttpVersion::HTTP_1_0);
    if (version == "HTTP/1.1") return Result<HttpVersion>::success(HttpVersion::HTTP_1_1);
    
    return make_error_result<HttpVersion>(NetworkError::ProtocolError);
}

std::string_view status_to_string(HttpStatusCode status) {
    static std::string status_str = std::to_string(static_cast<int>(status));
    return status_str;
}

std::string_view status_description(HttpStatusCode status) {
    switch (status) {
        // 1xx Informational
        case HttpStatusCode::Continue:           return "Continue";
        case HttpStatusCode::SwitchingProtocols: return "Switching Protocols";
        
        // 2xx Success
        case HttpStatusCode::OK:                 return "OK";
        case HttpStatusCode::Created:            return "Created";
        case HttpStatusCode::Accepted:           return "Accepted";
        case HttpStatusCode::NoContent:          return "No Content";
        
        // 3xx Redirection
        case HttpStatusCode::MovedPermanently:   return "Moved Permanently";
        case HttpStatusCode::Found:              return "Found";
        case HttpStatusCode::NotModified:        return "Not Modified";
        
        // 4xx Client Error
        case HttpStatusCode::BadRequest:         return "Bad Request";
        case HttpStatusCode::Unauthorized:       return "Unauthorized";
        case HttpStatusCode::Forbidden:          return "Forbidden";
        case HttpStatusCode::NotFound:           return "Not Found";
        case HttpStatusCode::MethodNotAllowed:   return "Method Not Allowed";
        
        // 5xx Server Error
        case HttpStatusCode::InternalServerError: return "Internal Server Error";
        case HttpStatusCode::NotImplemented:     return "Not Implemented";
        case HttpStatusCode::BadGateway:         return "Bad Gateway";
        case HttpStatusCode::ServiceUnavailable: return "Service Unavailable";
        
        default:                                 return "Unknown";
    }
}

std::string url_encode(std::string_view input) {
    std::ostringstream encoded;
    encoded.fill('0');
    encoded << std::hex;
    
    for (char c : input) {
        // Keep alphanumeric and safe characters unchanged
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded << c;
        } else {
            // Percent encode all other characters
            encoded << '%' << std::setw(2) << static_cast<unsigned char>(c);
        }
    }
    
    return encoded.str();
}

Result<std::string> url_decode(std::string_view input) {
    std::string decoded;
    decoded.reserve(input.size());
    
    for (size_t i = 0; i < input.size(); ++i) {
        if (input[i] == '%') {
            if (i + 2 >= input.size()) {
                return make_error_result<std::string>(NetworkError::ProtocolError);
            }
            
            // Parse hex digits
            char hex_str[3] = {input[i + 1], input[i + 2], '\0'};
            char* end_ptr;
            unsigned long hex_value = std::strtoul(hex_str, &end_ptr, 16);
            
            if (end_ptr != hex_str + 2) {
                return make_error_result<std::string>(NetworkError::ProtocolError);
            }
            
            decoded += static_cast<char>(hex_value);
            i += 2; // Skip the hex digits
        } else if (input[i] == '+') {
            // Convert '+' to space (application/x-www-form-urlencoded)
            decoded += ' ';
        } else {
            decoded += input[i];
        }
    }
    
    return Result<std::string>::success(std::move(decoded));
}

std::unordered_map<std::string, std::string> parse_query_params(std::string_view query) {
    std::unordered_map<std::string, std::string> params;
    
    size_t start = 0;
    while (start < query.size()) {
        size_t amp_pos = query.find('&', start);
        if (amp_pos == std::string_view::npos) {
            amp_pos = query.size();
        }
        
        std::string_view param = query.substr(start, amp_pos - start);
        size_t eq_pos = param.find('=');
        
        if (eq_pos != std::string_view::npos) {
            std::string key(param.substr(0, eq_pos));
            std::string value(param.substr(eq_pos + 1));
            
            // URL decode key and value
            auto decoded_key = url_decode(key);
            auto decoded_value = url_decode(value);
            
            if (decoded_key && decoded_value) {
                params[decoded_key.value()] = decoded_value.value();
            }
        } else if (!param.empty()) {
            // Key without value
            auto decoded_key = url_decode(param);
            if (decoded_key) {
                params[decoded_key.value()] = "";
            }
        }
        
        start = amp_pos + 1;
    }
    
    return params;
}

std::string build_query_string(const std::unordered_map<std::string, std::string>& params) {
    if (params.empty()) {
        return "";
    }
    
    std::ostringstream query;
    bool first = true;
    
    for (const auto& [key, value] : params) {
        if (!first) {
            query << '&';
        }
        first = false;
        
        query << url_encode(key);
        if (!value.empty()) {
            query << '=' << url_encode(value);
        }
    }
    
    return query.str();
}

Result<ContentType> parse_content_type(std::string_view content_type) {
    ContentType result;
    
    // Find the main type/subtype
    size_t semicolon_pos = content_type.find(';');
    result.media_type = content_type.substr(0, semicolon_pos);
    
    // Trim whitespace from media type
    result.media_type.erase(0, result.media_type.find_first_not_of(" \t"));
    result.media_type.erase(result.media_type.find_last_not_of(" \t") + 1);
    
    // Parse parameters
    if (semicolon_pos != std::string_view::npos) {
        std::string_view params_part = content_type.substr(semicolon_pos + 1);
        
        size_t start = 0;
        while (start < params_part.size()) {
            size_t semicolon = params_part.find(';', start);
            if (semicolon == std::string_view::npos) {
                semicolon = params_part.size();
            }
            
            std::string_view param = params_part.substr(start, semicolon - start);
            size_t eq_pos = param.find('=');
            
            if (eq_pos != std::string_view::npos) {
                std::string key(param.substr(0, eq_pos));
                std::string value(param.substr(eq_pos + 1));
                
                // Trim whitespace
                key.erase(0, key.find_first_not_of(" \t"));
                key.erase(key.find_last_not_of(" \t") + 1);
                value.erase(0, value.find_first_not_of(" \t"));
                value.erase(value.find_last_not_of(" \t") + 1);
                
                // Remove quotes if present
                if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
                    value = value.substr(1, value.size() - 2);
                }
                
                result.parameters[key] = value;
            }
            
            start = semicolon + 1;
        }
    }
    
    return Result<ContentType>::success(std::move(result));
}

std::string_view get_mime_type(std::string_view file_extension) {
    // Convert to lowercase
    std::string ext_lower(file_extension);
    std::transform(ext_lower.begin(), ext_lower.end(), ext_lower.begin(), ::tolower);
    
    // Remove leading dot if present
    if (!ext_lower.empty() && ext_lower[0] == '.') {
        ext_lower = ext_lower.substr(1);
    }
    
    // Common MIME types
    static const std::unordered_map<std::string, std::string_view> mime_types = {
        // Text
        {"txt",  "text/plain"},
        {"html", "text/html"},
        {"htm",  "text/html"},
        {"css",  "text/css"},
        {"js",   "text/javascript"},
        {"json", "application/json"},
        {"xml",  "application/xml"},
        
        // Images
        {"jpg",  "image/jpeg"},
        {"jpeg", "image/jpeg"},
        {"png",  "image/png"},
        {"gif",  "image/gif"},
        {"bmp",  "image/bmp"},
        {"ico",  "image/x-icon"},
        {"svg",  "image/svg+xml"},
        
        // Audio/Video
        {"mp3",  "audio/mpeg"},
        {"wav",  "audio/wav"},
        {"mp4",  "video/mp4"},
        {"avi",  "video/x-msvideo"},
        
        // Documents
        {"pdf",  "application/pdf"},
        {"doc",  "application/msword"},
        {"docx", "application/vnd.openxmlformats-officedocument.wordprocessingml.document"},
        
        // Archives
        {"zip",  "application/zip"},
        {"tar",  "application/x-tar"},
        {"gz",   "application/gzip"},
        
        // Binary
        {"bin",  "application/octet-stream"},
        {"exe",  "application/octet-stream"}
    };
    
    auto it = mime_types.find(ext_lower);
    if (it != mime_types.end()) {
        return it->second;
    }
    
    return "application/octet-stream"; // Default for unknown types
}

Result<std::string> simple_get(std::string_view url, std::chrono::milliseconds timeout) {
    try {
        HttpClient client(timeout);
        auto response = client.get(url);
        
        if (!response) {
            return make_error_result<std::string>(response.error());
        }
        
        return Result<std::string>::success(response.value().body());
    } catch (const std::exception& e) {
        LOG_ERROR("Simple GET failed: {}", e.what());
        return make_error_result<std::string>(NetworkError::ConnectionFailed);
    }
}

Result<std::string> simple_post(std::string_view url, std::string_view body, 
                               std::string_view content_type, std::chrono::milliseconds timeout) {
    try {
        HttpClient client(timeout);
        HttpHeaders headers;
        headers["content-type"] = std::string(content_type);
        
        auto response = client.post(url, body, headers);
        
        if (!response) {
            return make_error_result<std::string>(response.error());
        }
        
        return Result<std::string>::success(response.value().body());
    } catch (const std::exception& e) {
        LOG_ERROR("Simple POST failed: {}", e.what());
        return make_error_result<std::string>(NetworkError::ConnectionFailed);
    }
}

} // namespace networkquests::http::http_utils