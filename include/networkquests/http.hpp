#pragma once

#include "networkquests/tcp.hpp"
#include "networkquests/common.hpp"
#include "networkquests/logger.hpp"

#include <string>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <functional>
#include <regex>

namespace networkquests::http {

// HTTP version enumeration
enum class HttpVersion {
    HTTP_1_0,
    HTTP_1_1
};

// HTTP methods
enum class HttpMethod {
    GET,
    POST,
    PUT,
    DELETE,
    HEAD,
    OPTIONS,
    PATCH,
    CONNECT,
    TRACE
};

// HTTP status codes
enum class HttpStatusCode {
    // 1xx Informational
    Continue = 100,
    SwitchingProtocols = 101,
    
    // 2xx Success
    OK = 200,
    Created = 201,
    Accepted = 202,
    NoContent = 204,
    
    // 3xx Redirection
    MovedPermanently = 301,
    Found = 302,
    NotModified = 304,
    
    // 4xx Client Error
    BadRequest = 400,
    Unauthorized = 401,
    Forbidden = 403,
    NotFound = 404,
    MethodNotAllowed = 405,
    
    // 5xx Server Error
    InternalServerError = 500,
    NotImplemented = 501,
    BadGateway = 502,
    ServiceUnavailable = 503
};

// HTTP headers container
using HttpHeaders = std::unordered_map<std::string, std::string>;

/**
 * @brief Represents an HTTP request message
 */
class HttpRequest {
public:
    HttpRequest() = default;
    HttpRequest(HttpMethod method, std::string_view uri, HttpVersion version = HttpVersion::HTTP_1_1);
    
    // Accessors
    HttpMethod method() const { return method_; }
    const std::string& uri() const { return uri_; }
    HttpVersion version() const { return version_; }
    const HttpHeaders& headers() const { return headers_; }
    const std::string& body() const { return body_; }
    
    // Mutators
    void set_method(HttpMethod method) { method_ = method; }
    void set_uri(std::string_view uri) { uri_ = uri; }
    void set_version(HttpVersion version) { version_ = version; }
    void set_body(std::string_view body) { body_ = body; }
    
    // Header management
    void set_header(std::string_view name, std::string_view value);
    std::optional<std::string> get_header(std::string_view name) const;
    void remove_header(std::string_view name);
    
    // Convenience methods
    void set_content_type(std::string_view content_type);
    void set_content_length(size_t length);
    void set_user_agent(std::string_view user_agent);
    void set_authorization(std::string_view auth);
    
    // Serialization
    std::string to_string() const;
    static Result<HttpRequest> from_string(std::string_view data);
    
private:
    HttpMethod method_ = HttpMethod::GET;
    std::string uri_ = "/";
    HttpVersion version_ = HttpVersion::HTTP_1_1;
    HttpHeaders headers_;
    std::string body_;
};

/**
 * @brief Represents an HTTP response message
 */
class HttpResponse {
public:
    HttpResponse() = default;
    HttpResponse(HttpStatusCode status, HttpVersion version = HttpVersion::HTTP_1_1);
    
    // Accessors
    HttpStatusCode status() const { return status_; }
    HttpVersion version() const { return version_; }
    const HttpHeaders& headers() const { return headers_; }
    const std::string& body() const { return body_; }
    
    // Mutators
    void set_status(HttpStatusCode status) { status_ = status; }
    void set_version(HttpVersion version) { version_ = version; }
    void set_body(std::string_view body) { body_ = body; }
    
    // Header management
    void set_header(std::string_view name, std::string_view value);
    std::optional<std::string> get_header(std::string_view name) const;
    void remove_header(std::string_view name);
    
    // Convenience methods
    void set_content_type(std::string_view content_type);
    void set_content_length(size_t length);
    void set_server(std::string_view server);
    void set_cache_control(std::string_view cache_control);
    
    // Serialization
    std::string to_string() const;
    static Result<HttpResponse> from_string(std::string_view data);
    
private:
    HttpStatusCode status_ = HttpStatusCode::OK;
    HttpVersion version_ = HttpVersion::HTTP_1_1;
    HttpHeaders headers_;
    std::string body_;
};

/**
 * @brief HTTP client for making requests
 */
class HttpClient {
public:
    HttpClient();
    explicit HttpClient(std::chrono::milliseconds timeout);
    
    // Move-only type
    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;
    HttpClient(HttpClient&&) noexcept = default;
    HttpClient& operator=(HttpClient&&) noexcept = default;
    
    // HTTP methods
    Result<HttpResponse> get(std::string_view url, const HttpHeaders& headers = {});
    Result<HttpResponse> post(std::string_view url, std::string_view body, 
                             const HttpHeaders& headers = {});
    Result<HttpResponse> put(std::string_view url, std::string_view body,
                            const HttpHeaders& headers = {});
    Result<HttpResponse> delete_resource(std::string_view url, const HttpHeaders& headers = {});
    Result<HttpResponse> head(std::string_view url, const HttpHeaders& headers = {});
    
    // Generic request method
    Result<HttpResponse> send_request(const HttpRequest& request, std::string_view host, Port port = 80);
    
    // Settings
    void set_timeout(std::chrono::milliseconds timeout) { timeout_ = timeout; }
    void set_user_agent(std::string_view user_agent) { user_agent_ = user_agent; }
    void set_default_headers(const HttpHeaders& headers) { default_headers_ = headers; }
    
private:
    std::chrono::milliseconds timeout_;
    std::string user_agent_;
    HttpHeaders default_headers_;
    
    struct UrlParts {
        std::string scheme;
        std::string host;
        Port port;
        std::string path;
    };
    
    Result<UrlParts> parse_url(std::string_view url) const;
};

/**
 * @brief HTTP server for handling requests
 */
class HttpServer {
public:
    using RequestHandler = std::function<HttpResponse(const HttpRequest&)>;
    
    explicit HttpServer(Port port);
    HttpServer(const SocketAddress& bind_addr);
    
    // Move-only type
    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;
    HttpServer(HttpServer&&) noexcept = default;
    HttpServer& operator=(HttpServer&&) noexcept = default;
    
    // Route registration
    void route(HttpMethod method, std::string_view path, RequestHandler handler);
    void get(std::string_view path, RequestHandler handler);
    void post(std::string_view path, RequestHandler handler);
    void put(std::string_view path, RequestHandler handler);
    void delete_route(std::string_view path, RequestHandler handler);
    
    // Static file serving
    void serve_static(std::string_view url_prefix, std::string_view directory);
    
    // Middleware
    using Middleware = std::function<HttpResponse(const HttpRequest&, std::function<HttpResponse()>)>;
    void use_middleware(Middleware middleware);
    
    // Server control
    Result<void> start();
    void stop();
    bool is_running() const;
    
    // Settings
    void set_server_name(std::string_view name) { server_name_ = name; }
    void set_max_connections(int max_conn) { max_connections_ = max_conn; }
    
private:
    struct Route {
        HttpMethod method;
        std::string path;
        std::regex path_regex;
        RequestHandler handler;
    };
    
    Port port_;
    SocketAddress bind_addr_;
    std::vector<Route> routes_;
    std::vector<Middleware> middlewares_;
    std::unordered_map<std::string, std::string> static_routes_;
    std::string server_name_;
    int max_connections_;
    std::atomic<bool> running_{false};
    
    void handle_client(tcp::TcpConnection connection);
    HttpResponse process_request(const HttpRequest& request);
    HttpResponse serve_static_file(std::string_view file_path);
    bool match_route(const Route& route, const HttpRequest& request) const;
};

// Utility functions
namespace http_utils {

/**
 * @brief Convert HTTP method to string
 */
std::string_view method_to_string(HttpMethod method);

/**
 * @brief Convert string to HTTP method
 */
Result<HttpMethod> string_to_method(std::string_view method);

/**
 * @brief Convert HTTP version to string
 */
std::string_view version_to_string(HttpVersion version);

/**
 * @brief Convert string to HTTP version
 */
Result<HttpVersion> string_to_version(std::string_view version);

/**
 * @brief Convert status code to string
 */
std::string_view status_to_string(HttpStatusCode status);

/**
 * @brief Get status code description
 */
std::string_view status_description(HttpStatusCode status);

/**
 * @brief URL encode a string
 */
std::string url_encode(std::string_view input);

/**
 * @brief URL decode a string
 */
Result<std::string> url_decode(std::string_view input);

/**
 * @brief Parse query parameters from URL
 */
std::unordered_map<std::string, std::string> parse_query_params(std::string_view query);

/**
 * @brief Build query string from parameters
 */
std::string build_query_string(const std::unordered_map<std::string, std::string>& params);

/**
 * @brief Parse Content-Type header
 */
struct ContentType {
    std::string media_type;
    std::unordered_map<std::string, std::string> parameters;
};

Result<ContentType> parse_content_type(std::string_view content_type);

/**
 * @brief Get MIME type for file extension
 */
std::string_view get_mime_type(std::string_view file_extension);

/**
 * @brief Simple HTTP GET request
 */
Result<std::string> simple_get(std::string_view url, std::chrono::milliseconds timeout = std::chrono::seconds(30));

/**
 * @brief Simple HTTP POST request
 */
Result<std::string> simple_post(std::string_view url, std::string_view body, 
                               std::string_view content_type = "text/plain",
                               std::chrono::milliseconds timeout = std::chrono::seconds(30));

} // namespace http_utils

} // namespace networkquests::http