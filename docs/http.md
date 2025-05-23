# HTTP Protocol Implementation

## Overview

The HTTP (HyperText Transfer Protocol) implementation in NetworkQuests provides a complete, educational framework for building web applications and APIs. HTTP is the foundation of data communication on the World Wide Web, operating at the application layer (Layer 7) of the OSI model.

## Key Features

- **HttpRequest/HttpResponse**: Complete message parsing and generation
- **HttpClient**: Full-featured client with support for all HTTP methods
- **HttpServer**: Production-ready server with routing and middleware
- **URL Parsing**: Automatic URL parsing with scheme, host, port, and path extraction
- **Header Management**: Case-insensitive header handling
- **MIME Type Detection**: Automatic content type detection for static files
- **Middleware Support**: Express.js-style middleware chain
- **Cross-platform**: Works on Windows, Linux, and macOS
- **Modern C++20**: Uses latest C++ features and best practices

## HTTP Protocol Theory

### What is HTTP?

HTTP is a stateless, application-level protocol for distributed, collaborative, hypermedia information systems. Key characteristics:

- **Request-Response Model**: Client sends requests, server sends responses
- **Stateless**: Each request is independent and contains all necessary information
- **Text-Based**: Human-readable protocol with headers and optional body
- **Extensible**: Support for custom headers and methods
- **Cacheable**: Built-in caching mechanisms

### HTTP Message Structure

#### Request Message
```
GET /api/users HTTP/1.1
Host: example.com
User-Agent: NetworkQuests/1.0
Accept: application/json

[Optional Body]
```

#### Response Message  
```
HTTP/1.1 200 OK
Content-Type: application/json
Content-Length: 42
Server: NetworkQuests/1.0

{"users": [{"id": 1, "name": "Alice"}]}
```

### HTTP Methods

| Method | Purpose | Idempotent | Safe |
|--------|---------|------------|------|
| GET | Retrieve data | Yes | Yes |
| POST | Create/Submit data | No | No |
| PUT | Update/Replace data | Yes | No |
| DELETE | Remove data | Yes | No |
| HEAD | Get headers only | Yes | Yes |
| OPTIONS | Get allowed methods | Yes | Yes |

### Status Codes

- **1xx Informational**: Processing continues
- **2xx Success**: Request successful
- **3xx Redirection**: Further action needed
- **4xx Client Error**: Bad request
- **5xx Server Error**: Server failed

## Class Documentation

### HttpRequest

Represents an HTTP request message with method, URI, headers, and body.

```cpp
#include "networkquests/http.hpp"

// Create a GET request
HttpRequest request(HttpMethod::GET, "/api/users");
request.set_header("Accept", "application/json");
request.set_user_agent("MyApp/1.0");

// Create a POST request
HttpRequest post_request(HttpMethod::POST, "/api/users");
post_request.set_content_type("application/json");
post_request.set_body(R"({"name":"Alice"})");
post_request.set_content_length(post_request.body().size());
```

#### Key Methods

**Header Management:**
```cpp
void set_header(std::string_view name, std::string_view value);
std::optional<std::string> get_header(std::string_view name) const;
void remove_header(std::string_view name);

// Convenience methods
void set_content_type(std::string_view content_type);
void set_user_agent(std::string_view user_agent);
void set_authorization(std::string_view auth);
```

**Serialization:**
```cpp
std::string to_string() const;
static Result<HttpRequest> from_string(std::string_view data);
```

### HttpResponse

Represents an HTTP response message with status code, headers, and body.

```cpp
// Create a successful response
HttpResponse response(HttpStatusCode::OK);
response.set_content_type("application/json");
response.set_body(R"({"status":"success"})");
response.set_content_length(response.body().size());

// Create an error response
HttpResponse error(HttpStatusCode::NotFound);
error.set_content_type("text/plain");
error.set_body("Resource not found");
```

### HttpClient

High-level HTTP client supporting all major HTTP methods.

```cpp
// Create client with timeout
HttpClient client(std::chrono::seconds(30));
client.set_user_agent("MyApp/1.0");

// GET request
auto response = client.get("http://api.example.com/users");
if (response) {
    std::cout << "Response: " << response.value().body() << std::endl;
}

// POST request with JSON
HttpHeaders headers;
headers["content-type"] = "application/json";
auto post_response = client.post("http://api.example.com/users", 
                                R"({"name":"Bob"})", headers);

// Other methods
auto put_response = client.put(url, data, headers);
auto delete_response = client.delete_resource(url);
auto head_response = client.head(url);
```

### HttpServer

Production-ready HTTP server with routing and middleware support.

```cpp
// Create server
HttpServer server(8080);
server.set_server_name("MyApp/1.0");

// Add routes
server.get("/", [](const HttpRequest& req) -> HttpResponse {
    HttpResponse response(HttpStatusCode::OK);
    response.set_content_type("text/html");
    response.set_body("<h1>Welcome</h1>");
    return response;
});

server.post("/api/users", [](const HttpRequest& req) -> HttpResponse {
    // Process user creation
    HttpResponse response(HttpStatusCode::Created);
    response.set_content_type("application/json");
    response.set_body(R"({"id":123,"name":"Created"})");
    return response;
});

// Start server
server.start();
```

## Usage Examples

### Simple HTTP Client

```cpp
#include "networkquests/http.hpp"

int main() {
    networkquests::socket_utils::NetworkingRAII networking;
    
    HttpClient client;
    
    // Simple GET request
    auto response = client.get("http://httpbin.org/json");
    if (response) {
        std::cout << "Status: " << static_cast<int>(response.value().status()) << std::endl;
        std::cout << "Body: " << response.value().body() << std::endl;
    }
    
    return 0;
}
```

### RESTful API Server

```cpp
#include "networkquests/http.hpp"
#include <map>

std::map<int, std::string> users;
int next_id = 1;

int main() {
    networkquests::socket_utils::NetworkingRAII networking;
    
    HttpServer server(8080);
    
    // GET /api/users - List all users
    server.get("/api/users", [](const HttpRequest& req) -> HttpResponse {
        std::ostringstream json;
        json << "[";
        bool first = true;
        for (const auto& [id, name] : users) {
            if (!first) json << ",";
            json << R"({"id":)" << id << R"(,"name":")" << name << R"("})";
            first = false;
        }
        json << "]";
        
        HttpResponse response(HttpStatusCode::OK);
        response.set_content_type("application/json");
        response.set_body(json.str());
        return response;
    });
    
    // POST /api/users - Create user
    server.post("/api/users", [](const HttpRequest& req) -> HttpResponse {
        // Simple JSON parsing (use proper JSON library in production)
        std::string body = req.body();
        size_t name_start = body.find(R"("name":")")+ 8;
        size_t name_end = body.find('"', name_start);
        
        if (name_start != std::string::npos && name_end != std::string::npos) {
            std::string name = body.substr(name_start, name_end - name_start);
            int id = next_id++;
            users[id] = name;
            
            std::ostringstream json;
            json << R"({"id":)" << id << R"(,"name":")" << name << R"("})";
            
            HttpResponse response(HttpStatusCode::Created);
            response.set_content_type("application/json");
            response.set_body(json.str());
            return response;
        }
        
        HttpResponse error(HttpStatusCode::BadRequest);
        error.set_content_type("application/json");
        error.set_body(R"({"error":"Invalid JSON"})");
        return error;
    });
    
    std::cout << "Server running on http://localhost:8080\n";
    server.start();
    
    return 0;
}
```

### Middleware Example

```cpp
// CORS middleware
HttpResponse cors_middleware(const HttpRequest& request, std::function<HttpResponse()> next) {
    auto response = next();
    response.set_header("Access-Control-Allow-Origin", "*");
    response.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE");
    return response;
}

// Logging middleware
HttpResponse logging_middleware(const HttpRequest& request, std::function<HttpResponse()> next) {
    auto start = std::chrono::steady_clock::now();
    auto response = next();
    auto duration = std::chrono::steady_clock::now() - start;
    
    std::cout << http_utils::method_to_string(request.method()) 
              << " " << request.uri() 
              << " -> " << static_cast<int>(response.status())
              << " (" << duration.count() << "ms)" << std::endl;
    
    return response;
}

// Add to server
server.use_middleware(cors_middleware);
server.use_middleware(logging_middleware);
```

### Static File Serving

```cpp
HttpServer server(8080);

// Serve static files from ./public directory
server.serve_static("/static", "./public");

// Custom file handler
server.get("/download/{filename}", [](const HttpRequest& req) -> HttpResponse {
    // Extract filename from URL path
    std::string uri = req.uri();
    size_t last_slash = uri.find_last_of('/');
    std::string filename = uri.substr(last_slash + 1);
    
    // Serve file with appropriate headers
    // ... file reading logic ...
    
    HttpResponse response(HttpStatusCode::OK);
    response.set_content_type("application/octet-stream");
    response.set_header("Content-Disposition", "attachment; filename=" + filename);
    return response;
});
```

## HTTP Utilities

The `http_utils` namespace provides helpful utility functions:

```cpp
namespace http_utils {
    // Type conversions
    std::string_view method_to_string(HttpMethod method);
    Result<HttpMethod> string_to_method(std::string_view method);
    std::string_view status_description(HttpStatusCode status);
    
    // URL encoding/decoding
    std::string url_encode(std::string_view input);
    Result<std::string> url_decode(std::string_view input);
    
    // Query parameter parsing
    std::unordered_map<std::string, std::string> parse_query_params(std::string_view query);
    std::string build_query_string(const std::unordered_map<std::string, std::string>& params);
    
    // Content type handling
    Result<ContentType> parse_content_type(std::string_view content_type);
    std::string_view get_mime_type(std::string_view file_extension);
    
    // Convenience functions
    Result<std::string> simple_get(std::string_view url);
    Result<std::string> simple_post(std::string_view url, std::string_view body);
}
```

## Best Practices

### Error Handling

Always check return values and handle HTTP errors appropriately:

```cpp
auto response = client.get(url);
if (!response) {
    std::cerr << "Request failed: " << response.error().message() << std::endl;
    return;
}

if (response.value().status() != HttpStatusCode::OK) {
    std::cerr << "HTTP error: " << static_cast<int>(response.value().status()) << std::endl;
    return;
}
```

### Headers Management

Use case-insensitive header access and set appropriate headers:

```cpp
// Setting headers
request.set_content_type("application/json");
request.set_header("Accept", "application/json");
request.set_user_agent("MyApp/1.0");

// Reading headers (case-insensitive)
auto content_type = response.get_header("content-type");
auto content_length = response.get_header("Content-Length");
```

### Content Length

Always set Content-Length for requests/responses with body:

```cpp
request.set_body(json_data);
request.set_content_length(json_data.size());

response.set_body(html_content);
response.set_content_length(html_content.size());
```

### URL Handling

Properly encode URLs and handle query parameters:

```cpp
// URL encoding
std::string encoded = http_utils::url_encode("hello world");
// Result: "hello%20world"

// Query parameters
std::unordered_map<std::string, std::string> params = {
    {"name", "John Doe"},
    {"age", "30"}
};
std::string query = http_utils::build_query_string(params);
// Result: "name=John%20Doe&age=30"
```

## Performance Considerations

### Connection Management

For multiple requests to the same server, consider connection reuse:

```cpp
// Create client once and reuse
HttpClient client;
client.set_timeout(std::chrono::seconds(10));

for (const auto& endpoint : endpoints) {
    auto response = client.get("http://api.example.com" + endpoint);
    // Process response...
}
```

### Timeouts

Set appropriate timeouts based on your use case:

```cpp
// Short timeout for real-time APIs
HttpClient fast_client(std::chrono::seconds(5));

// Longer timeout for file uploads
HttpClient upload_client(std::chrono::minutes(5));
```

### Memory Management

Be mindful of response sizes and implement limits:

```cpp
// Server with request size limit
server.set_max_request_size(10 * 1024 * 1024); // 10MB

// Client with response size checking
auto response = client.get(url);
if (response && response.value().body().size() > max_size) {
    std::cerr << "Response too large" << std::endl;
}
```

## Security Considerations

### Input Validation

Always validate and sanitize inputs:

```cpp
server.post("/api/users", [](const HttpRequest& req) -> HttpResponse {
    // Validate Content-Type
    auto content_type = req.get_header("content-type");
    if (!content_type || content_type->find("application/json") == std::string::npos) {
        HttpResponse error(HttpStatusCode::BadRequest);
        error.set_body("Expected JSON content");
        return error;
    }
    
    // Validate body size
    if (req.body().size() > 1024) {
        HttpResponse error(HttpStatusCode::PayloadTooLarge);
        return error;
    }
    
    // Process request...
});
```

### HTTPS Support

While this implementation focuses on HTTP, consider HTTPS for production:

```cpp
// Note: This is a conceptual example
// Real HTTPS requires SSL/TLS implementation
HttpsClient secure_client;
secure_client.set_certificate_verification(true);
auto response = secure_client.get("https://secure.example.com/api");
```

### CORS Headers

Properly handle Cross-Origin Resource Sharing:

```cpp
HttpResponse cors_handler(const HttpRequest& req, std::function<HttpResponse()> next) {
    if (req.method() == HttpMethod::OPTIONS) {
        HttpResponse response(HttpStatusCode::OK);
        response.set_header("Access-Control-Allow-Origin", "*");
        response.set_header("Access-Control-Allow-Methods", "GET,POST,PUT,DELETE");
        response.set_header("Access-Control-Allow-Headers", "Content-Type,Authorization");
        return response;
    }
    
    auto response = next();
    response.set_header("Access-Control-Allow-Origin", "*");
    return response;
}
```

## Advanced Topics

### Custom HTTP Methods

Extend the implementation for custom methods:

```cpp
// Add to HttpMethod enum
enum class HttpMethod {
    // ... existing methods ...
    PATCH,
    CUSTOM_METHOD
};

// Implement in utils
std::string_view method_to_string(HttpMethod method) {
    switch (method) {
        // ... existing cases ...
        case HttpMethod::CUSTOM_METHOD: return "CUSTOM";
    }
}
```

### Content Compression

Add support for gzip/deflate compression:

```cpp
// Conceptual example
class CompressionMiddleware {
public:
    HttpResponse operator()(const HttpRequest& req, std::function<HttpResponse()> next) {
        auto response = next();
        
        auto accept_encoding = req.get_header("accept-encoding");
        if (accept_encoding && accept_encoding->find("gzip") != std::string::npos) {
            auto compressed_body = gzip_compress(response.body());
            response.set_body(compressed_body);
            response.set_header("Content-Encoding", "gzip");
            response.set_content_length(compressed_body.size());
        }
        
        return response;
    }
};
```

### WebSocket Upgrade

Handle WebSocket upgrade requests:

```cpp
server.get("/ws", [](const HttpRequest& req) -> HttpResponse {
    auto upgrade = req.get_header("upgrade");
    auto connection = req.get_header("connection");
    
    if (upgrade && *upgrade == "websocket" && 
        connection && connection->find("Upgrade") != std::string::npos) {
        
        // Handle WebSocket handshake
        HttpResponse response(HttpStatusCode::SwitchingProtocols);
        response.set_header("Upgrade", "websocket");
        response.set_header("Connection", "Upgrade");
        // ... WebSocket key processing ...
        return response;
    }
    
    HttpResponse error(HttpStatusCode::BadRequest);
    return error;
});
```

## Testing and Debugging

### Unit Testing

```cpp
#include <catch2/catch.hpp>

TEST_CASE("HTTP Request Parsing") {
    std::string http_text = "GET /api/users HTTP/1.1\r\n"
                           "Host: example.com\r\n"
                           "User-Agent: Test\r\n"
                           "\r\n";
    
    auto request = HttpRequest::from_string(http_text);
    REQUIRE(request.has_value());
    REQUIRE(request.value().method() == HttpMethod::GET);
    REQUIRE(request.value().uri() == "/api/users");
}
```

### Debug Logging

```cpp
// Enable debug logging
Logger::instance().set_level(LogLevel::Debug);

// HTTP operations will log detailed information
HttpClient client;
auto response = client.get("http://example.com/api");
```

## Conclusion

The NetworkQuests HTTP implementation provides a complete, educational framework for building modern web applications and APIs in C++. It combines:

- **Educational Value**: Clear examples and comprehensive documentation
- **Modern Design**: C++20 features and best practices
- **Production Ready**: Robust error handling and performance considerations
- **Extensibility**: Middleware system and customization points

Whether you're building a simple REST API, a file server, or learning HTTP protocol internals, this implementation provides the foundation you need with excellent educational value.