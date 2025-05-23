#include "networkquests/http.hpp"
#include <thread>
#include <fstream>
#include <filesystem>

namespace networkquests::http {

HttpServer::HttpServer(Port port) 
    : port_(port)
    , bind_addr_(SocketAddress::from_ipv4("0.0.0.0", port).value_or(SocketAddress{}))
    , server_name_("NetworkQuests/1.0")
    , max_connections_(100) {
}

HttpServer::HttpServer(const SocketAddress& bind_addr)
    : port_(bind_addr.port())
    , bind_addr_(bind_addr)
    , server_name_("NetworkQuests/1.0")
    , max_connections_(100) {
}

void HttpServer::route(HttpMethod method, std::string_view path, RequestHandler handler) {
    Route route;
    route.method = method;
    route.path = std::string(path);
    
    // Convert path to regex pattern
    // Simple pattern: /users/{id} becomes /users/([^/]+)
    std::string pattern = route.path;
    
    // Escape special regex characters except for {} which we'll handle specially
    pattern = std::regex_replace(pattern, std::regex(R"([\.\[\]()^$*+?|\\])"), R"(\$&)");
    
    // Convert {param} to capture groups
    pattern = std::regex_replace(pattern, std::regex(R"(\{[^}]+\})"), R"(([^/]+))");
    
    // Ensure exact match
    pattern = "^" + pattern + "$";
    
    route.path_regex = std::regex(pattern);
    route.handler = std::move(handler);
    
    routes_.push_back(std::move(route));
    
    LOG_DEBUG("Registered route: {} {} -> {}", 
              http_utils::method_to_string(method), path, pattern);
}

void HttpServer::get(std::string_view path, RequestHandler handler) {
    route(HttpMethod::GET, path, std::move(handler));
}

void HttpServer::post(std::string_view path, RequestHandler handler) {
    route(HttpMethod::POST, path, std::move(handler));
}

void HttpServer::put(std::string_view path, RequestHandler handler) {
    route(HttpMethod::PUT, path, std::move(handler));
}

void HttpServer::delete_route(std::string_view path, RequestHandler handler) {
    route(HttpMethod::DELETE, path, std::move(handler));
}

void HttpServer::serve_static(std::string_view url_prefix, std::string_view directory) {
    static_routes_[std::string(url_prefix)] = std::string(directory);
    LOG_DEBUG("Serving static files: {} -> {}", url_prefix, directory);
}

void HttpServer::use_middleware(Middleware middleware) {
    middlewares_.push_back(std::move(middleware));
    LOG_DEBUG("Added middleware (total: {})", middlewares_.size());
}

Result<void> HttpServer::start() {
    if (running_) {
        return make_error_result<void>(NetworkError::AlreadyConnected);
    }
    
    LOG_INFO("Starting HTTP server on {}", bind_addr_.to_string());
    
    try {
        // Create TCP server
        tcp::TcpServer tcp_server(bind_addr_);
        
        running_ = true;
        
        // Accept connections
        auto listen_result = tcp_server.listen();
        if (!listen_result) {
            running_ = false;
            return listen_result;
        }
        
        auto start_result = tcp_server.start([this](tcp::TcpConnection connection) {
            handle_client(std::move(connection));
        });
        
        if (!start_result) {
            running_ = false;
            return start_result;
        }
        
        LOG_INFO("HTTP server started successfully");
        return Result<void>::success();
        
    } catch (const std::exception& e) {
        running_ = false;
        LOG_ERROR("Failed to start HTTP server: {}", e.what());
        return make_error_result<void>(NetworkError::ListenFailed);
    }
}

void HttpServer::stop() {
    if (running_) {
        running_ = false;
        LOG_INFO("HTTP server stopped");
    }
}

bool HttpServer::is_running() const {
    return running_;
}

void HttpServer::handle_client(tcp::TcpConnection connection) {
    auto remote_addr = connection.remote_address();
    std::string client_info = remote_addr ? remote_addr.value().to_string() : "unknown";
    
    LOG_DEBUG("Handling HTTP connection from {}", client_info);
    
    try {
        // Read the HTTP request
        std::string request_data;
        std::string chunk;
        bool headers_complete = false;
        size_t content_length = 0;
        bool has_content_length = false;
        
        // Read until we have complete headers
        while (!headers_complete && connection.is_connected()) {
            auto recv_result = connection.receive_string(std::chrono::seconds(30));
            if (!recv_result) {
                LOG_WARNING("Failed to receive HTTP request from {}: {}", 
                           client_info, recv_result.error().message());
                return;
            }
            
            chunk = recv_result.value();
            request_data += chunk;
            
            // Check if we have complete headers
            size_t header_end = request_data.find("\r\n\r\n");
            if (header_end != std::string::npos) {
                headers_complete = true;
                
                // Parse Content-Length from headers
                std::string_view headers_part = std::string_view(request_data).substr(0, header_end);
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
                size_t body_received = request_data.size() - body_start;
                
                // If we need more body data, continue reading
                if (has_content_length && body_received < content_length) {
                    size_t remaining = content_length - body_received;
                    LOG_DEBUG("Need to read {} more bytes of body", remaining);
                    
                    while (remaining > 0 && connection.is_connected()) {
                        auto body_result = connection.receive_string(std::chrono::seconds(30));
                        if (!body_result) {
                            LOG_WARNING("Failed to receive HTTP request body from {}: {}", 
                                       client_info, body_result.error().message());
                            return;
                        }
                        
                        std::string body_chunk = body_result.value();
                        request_data += body_chunk;
                        remaining -= body_chunk.size();
                    }
                }
            }
            
            // Prevent infinite loop and DoS
            if (request_data.size() > 1024 * 1024) { // 1MB limit
                LOG_WARNING("HTTP request from {} too large", client_info);
                return;
            }
        }
        
        LOG_DEBUG("Received HTTP request from {} ({} bytes)", client_info, request_data.size());
        
        // Parse the HTTP request
        auto request_result = HttpRequest::from_string(request_data);
        if (!request_result) {
            LOG_WARNING("Failed to parse HTTP request from {}: {}", 
                       client_info, request_result.error().message());
            
            // Send 400 Bad Request
            HttpResponse error_response(HttpStatusCode::BadRequest);
            error_response.set_server(server_name_);
            error_response.set_content_type("text/plain");
            error_response.set_body("Bad Request");
            error_response.set_content_length(error_response.body().size());
            
            std::string response_str = error_response.to_string();
            connection.send_string(response_str);
            return;
        }
        
        auto request = request_result.value();
        LOG_INFO("{} {} {} from {}", 
                 http_utils::method_to_string(request.method()),
                 request.uri(),
                 http_utils::version_to_string(request.version()),
                 client_info);
        
        // Process the request
        HttpResponse response = process_request(request);
        
        // Set server header if not already set
        if (!response.get_header("server")) {
            response.set_server(server_name_);
        }
        
        // Set Content-Length if not already set and we have a body
        if (!response.get_header("content-length") && !response.body().empty()) {
            response.set_content_length(response.body().size());
        }
        
        // Send response
        std::string response_str = response.to_string();
        auto send_result = connection.send_string(response_str);
        
        if (send_result) {
            LOG_DEBUG("Sent HTTP response to {} ({} bytes, status {})", 
                     client_info, send_result.value(), static_cast<int>(response.status()));
        } else {
            LOG_WARNING("Failed to send HTTP response to {}: {}", 
                       client_info, send_result.error().message());
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR("Exception handling HTTP client {}: {}", client_info, e.what());
    }
}

HttpResponse HttpServer::process_request(const HttpRequest& request) {
    // Apply middleware chain
    auto final_handler = [this, &request]() -> HttpResponse {
        // First, try to match routes
        for (const auto& route : routes_) {
            if (match_route(route, request)) {
                LOG_DEBUG("Matched route: {} {}", 
                         http_utils::method_to_string(route.method), route.path);
                return route.handler(request);
            }
        }
        
        // Try static file serving
        for (const auto& [prefix, directory] : static_routes_) {
            if (request.uri().starts_with(prefix)) {
                std::string file_path = request.uri().substr(prefix.length());
                if (file_path.empty() || file_path == "/") {
                    file_path = "/index.html";
                }
                
                std::string full_path = directory + file_path;
                return serve_static_file(full_path);
            }
        }
        
        // No route matched, return 404
        HttpResponse not_found(HttpStatusCode::NotFound);
        not_found.set_content_type("text/plain");
        not_found.set_body("Not Found");
        not_found.set_content_length(not_found.body().size());
        return not_found;
    };
    
    // Apply middlewares in reverse order (last added, first executed)
    for (auto it = middlewares_.rbegin(); it != middlewares_.rend(); ++it) {
        auto current_handler = final_handler;
        auto middleware = *it;
        final_handler = [middleware, current_handler, &request]() -> HttpResponse {
            return middleware(request, current_handler);
        };
    }
    
    return final_handler();
}

HttpResponse HttpServer::serve_static_file(std::string_view file_path) {
    try {
        std::filesystem::path fs_path(file_path);
        
        // Security check: prevent directory traversal
        std::filesystem::path canonical_path = std::filesystem::weakly_canonical(fs_path);
        std::string canonical_str = canonical_path.string();
        
        if (canonical_str.find("..") != std::string::npos) {
            LOG_WARNING("Directory traversal attempt: {}", file_path);
            HttpResponse forbidden(HttpStatusCode::Forbidden);
            forbidden.set_content_type("text/plain");
            forbidden.set_body("Forbidden");
            forbidden.set_content_length(forbidden.body().size());
            return forbidden;
        }
        
        // Check if file exists and is regular file
        if (!std::filesystem::exists(canonical_path) || !std::filesystem::is_regular_file(canonical_path)) {
            HttpResponse not_found(HttpStatusCode::NotFound);
            not_found.set_content_type("text/plain");
            not_found.set_body("File Not Found");
            not_found.set_content_length(not_found.body().size());
            return not_found;
        }
        
        // Read file content
        std::ifstream file(canonical_path, std::ios::binary);
        if (!file) {
            HttpResponse server_error(HttpStatusCode::InternalServerError);
            server_error.set_content_type("text/plain");
            server_error.set_body("Failed to read file");
            server_error.set_content_length(server_error.body().size());
            return server_error;
        }
        
        std::string content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
        
        // Determine MIME type
        std::string extension = canonical_path.extension().string();
        std::string_view mime_type = http_utils::get_mime_type(extension);
        
        HttpResponse response(HttpStatusCode::OK);
        response.set_content_type(mime_type);
        response.set_body(content);
        response.set_content_length(content.size());
        
        LOG_DEBUG("Serving static file: {} ({} bytes, {})", 
                 file_path, content.size(), mime_type);
        
        return response;
        
    } catch (const std::exception& e) {
        LOG_ERROR("Error serving static file {}: {}", file_path, e.what());
        HttpResponse server_error(HttpStatusCode::InternalServerError);
        server_error.set_content_type("text/plain");
        server_error.set_body("Internal Server Error");
        server_error.set_content_length(server_error.body().size());
        return server_error;
    }
}

bool HttpServer::match_route(const Route& route, const HttpRequest& request) const {
    // Check method
    if (route.method != request.method()) {
        return false;
    }
    
    // Extract path from URI (remove query string)
    std::string request_path = request.uri();
    size_t query_pos = request_path.find('?');
    if (query_pos != std::string::npos) {
        request_path = request_path.substr(0, query_pos);
    }
    
    // Check path pattern
    return std::regex_match(request_path, route.path_regex);
}

} // namespace networkquests::http