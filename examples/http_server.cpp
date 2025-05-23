#include "networkquests/http.hpp"
#include "networkquests/socket.hpp"
#include "networkquests/logger.hpp"
#include "networkquests/common.hpp"

#include <iostream>
#include <signal.h>
#include <thread>
#include <sstream>
#include <map>

using namespace networkquests;
using namespace networkquests::http;

std::atomic<bool> should_stop{false};

void signal_handler(int) {
    should_stop = true;
    std::cout << "\nReceived signal, shutting down...\n";
}

// Simple in-memory data store for demo
std::map<int, std::string> users = {
    {1, "Alice"},
    {2, "Bob"},
    {3, "Charlie"}
};
int next_user_id = 4;

// CORS middleware
HttpResponse cors_middleware(const HttpRequest& request, std::function<HttpResponse()> next) {
    auto response = next();
    
    response.set_header("Access-Control-Allow-Origin", "*");
    response.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    response.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
    
    return response;
}

// Logging middleware
HttpResponse logging_middleware(const HttpRequest& request, std::function<HttpResponse()> next) {
    auto start_time = std::chrono::steady_clock::now();
    
    auto response = next();
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    std::cout << "[" << utils::current_timestamp() << "] "
              << http_utils::method_to_string(request.method()) << " " 
              << request.uri() << " -> " 
              << static_cast<int>(response.status()) << " "
              << "(" << duration.count() << "ms)" << std::endl;
    
    return response;
}

// API handlers
HttpResponse get_users(const HttpRequest& request) {
    std::ostringstream json;
    json << "[";
    bool first = true;
    for (const auto& [id, name] : users) {
        if (!first) json << ",";
        json << "{\"id\":" << id << ",\"name\":\"" << name << "\"}";
        first = false;
    }
    json << "]";
    
    HttpResponse response(HttpStatusCode::OK);
    response.set_content_type("application/json");
    response.set_body(json.str());
    response.set_content_length(response.body().size());
    return response;
}

HttpResponse get_user(const HttpRequest& request) {
    // Extract user ID from URL (simplified parsing)
    std::string uri = request.uri();
    size_t last_slash = uri.find_last_of('/');
    if (last_slash == std::string::npos) {
        HttpResponse bad_request(HttpStatusCode::BadRequest);
        bad_request.set_content_type("text/plain");
        bad_request.set_body("Invalid user ID");
        bad_request.set_content_length(bad_request.body().size());
        return bad_request;
    }
    
    try {
        int user_id = std::stoi(uri.substr(last_slash + 1));
        auto it = users.find(user_id);
        
        if (it == users.end()) {
            HttpResponse not_found(HttpStatusCode::NotFound);
            not_found.set_content_type("application/json");
            not_found.set_body("{\"error\":\"User not found\"}");
            not_found.set_content_length(not_found.body().size());
            return not_found;
        }
        
        std::ostringstream json;
        json << "{\"id\":" << it->first << ",\"name\":\"" << it->second << "\"}";
        
        HttpResponse response(HttpStatusCode::OK);
        response.set_content_type("application/json");
        response.set_body(json.str());
        response.set_content_length(response.body().size());
        return response;
        
    } catch (const std::exception& e) {
        HttpResponse bad_request(HttpStatusCode::BadRequest);
        bad_request.set_content_type("text/plain");
        bad_request.set_body("Invalid user ID");
        bad_request.set_content_length(bad_request.body().size());
        return bad_request;
    }
}

HttpResponse create_user(const HttpRequest& request) {
    // Simple JSON parsing (in real app, use proper JSON library)
    std::string body = request.body();
    size_t name_start = body.find("\"name\":\"");
    if (name_start == std::string::npos) {
        HttpResponse bad_request(HttpStatusCode::BadRequest);
        bad_request.set_content_type("application/json");
        bad_request.set_body("{\"error\":\"Name field required\"}");
        bad_request.set_content_length(bad_request.body().size());
        return bad_request;
    }
    
    name_start += 8; // Skip "name":"
    size_t name_end = body.find("\"", name_start);
    if (name_end == std::string::npos) {
        HttpResponse bad_request(HttpStatusCode::BadRequest);
        bad_request.set_content_type("application/json");
        bad_request.set_body("{\"error\":\"Invalid JSON\"}");
        bad_request.set_content_length(bad_request.body().size());
        return bad_request;
    }
    
    std::string name = body.substr(name_start, name_end - name_start);
    int user_id = next_user_id++;
    users[user_id] = name;
    
    std::ostringstream json;
    json << "{\"id\":" << user_id << ",\"name\":\"" << name << "\"}";
    
    HttpResponse response(HttpStatusCode::Created);
    response.set_content_type("application/json");
    response.set_body(json.str());
    response.set_content_length(response.body().size());
    return response;
}

HttpResponse get_status(const HttpRequest& request) {
    std::ostringstream json;
    json << "{\"status\":\"OK\",\"users_count\":" << users.size() 
         << ",\"server\":\"NetworkQuests HTTP Server\"}";
    
    HttpResponse response(HttpStatusCode::OK);
    response.set_content_type("application/json");
    response.set_body(json.str());
    response.set_content_length(response.body().size());
    return response;
}

HttpResponse handle_options(const HttpRequest& request) {
    HttpResponse response(HttpStatusCode::OK);
    response.set_header("Access-Control-Allow-Origin", "*");
    response.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    response.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
    response.set_content_length(0);
    return response;
}

int main(int argc, char* argv[]) {
    // Set up signal handling
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Initialize networking
    socket_utils::NetworkingRAII networking;
    if (!networking.is_initialized()) {
        std::cerr << "Failed to initialize networking\n";
        return 1;
    }
    
    // Set logging level
    Logger::instance().set_level(LogLevel::Info);
    
    // Parse command line arguments
    Port port = 8080;
    if (argc > 1) {
        try {
            port = static_cast<Port>(std::stoi(argv[1]));
        } catch (const std::exception& e) {
            std::cerr << "Invalid port number: " << argv[1] << "\n";
            return 1;
        }
    }
    
    try {
        // Create HTTP server
        HttpServer server(port);
        server.set_server_name("NetworkQuests HTTP Server/1.0");
        
        // Add middleware
        server.use_middleware(logging_middleware);
        server.use_middleware(cors_middleware);
        
        // Register API routes
        server.get("/api/status", get_status);
        server.get("/api/users", get_users);
        server.get("/api/users/{id}", get_user);
        server.post("/api/users", create_user);
        server.route(HttpMethod::OPTIONS, "/api/{path}", handle_options);
        
        // Simple homepage
        server.get("/", [](const HttpRequest& request) -> HttpResponse {
            std::string html = R"(
<!DOCTYPE html>
<html>
<head>
    <title>NetworkQuests HTTP Server</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 40px; }
        .endpoint { background: #f5f5f5; padding: 10px; margin: 10px 0; border-radius: 5px; }
        .method { color: #007acc; font-weight: bold; }
    </style>
</head>
<body>
    <h1>🌐 NetworkQuests HTTP Server</h1>
    <p>Welcome to the NetworkQuests educational HTTP server!</p>
    
    <h2>Available API Endpoints:</h2>
    <div class="endpoint">
        <span class="method">GET</span> /api/status - Server status
    </div>
    <div class="endpoint">
        <span class="method">GET</span> /api/users - List all users
    </div>
    <div class="endpoint">
        <span class="method">GET</span> /api/users/{id} - Get specific user
    </div>
    <div class="endpoint">
        <span class="method">POST</span> /api/users - Create new user
    </div>
    
    <h2>Test Commands:</h2>
    <pre>
# Get status
curl http://localhost:)" + std::to_string(port) + R"(/api/status

# List users
curl http://localhost:)" + std::to_string(port) + R"(/api/users

# Get specific user
curl http://localhost:)" + std::to_string(port) + R"(/api/users/1

# Create user
curl -X POST -H "Content-Type: application/json" \
     -d '{"name":"David"}' \
     http://localhost:)" + std::to_string(port) + R"(/api/users
    </pre>
    
    <p><em>Built with NetworkQuests C++ HTTP implementation</em></p>
</body>
</html>
)";
            
            HttpResponse response(HttpStatusCode::OK);
            response.set_content_type("text/html");
            response.set_body(html);
            response.set_content_length(html.size());
            return response;
        });
        
        std::cout << "HTTP Server starting on port " << port << "\n";
        std::cout << "Visit http://localhost:" << port << " in your browser\n";
        std::cout << "API available at http://localhost:" << port << "/api/\n";
        std::cout << "Press Ctrl+C to stop the server\n\n";
        
        // Start server in a separate thread
        std::thread server_thread([&server]() {
            auto start_result = server.start();
            if (!start_result) {
                std::cerr << "Failed to start HTTP server: " << start_result.error().message() << "\n";
            }
        });
        
        // Wait for shutdown signal
        while (!should_stop) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        std::cout << "Shutting down server...\n";
        server.stop();
        
        // Wait for server thread to finish
        if (server_thread.joinable()) {
            server_thread.join();
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Server error: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}