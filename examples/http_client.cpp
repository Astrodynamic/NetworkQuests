#include "networkquests/http.hpp"
#include "networkquests/socket.hpp"
#include "networkquests/logger.hpp"
#include "networkquests/common.hpp"

#include <iostream>
#include <string>

using namespace networkquests;
using namespace networkquests::http;

void print_response(const HttpResponse& response) {
    std::cout << "Status: " << static_cast<int>(response.status()) 
              << " " << http_utils::status_description(response.status()) << "\n";
    
    std::cout << "Headers:\n";
    for (const auto& [name, value] : response.headers()) {
        std::cout << "  " << name << ": " << value << "\n";
    }
    
    if (!response.body().empty()) {
        std::cout << "Body (" << response.body().size() << " bytes):\n";
        std::cout << response.body() << "\n";
    }
    std::cout << std::string(50, '-') << "\n";
}

void test_get_request(HttpClient& client, std::string_view url) {
    std::cout << "GET " << url << "\n";
    
    auto response = client.get(url);
    if (response) {
        print_response(response.value());
    } else {
        std::cerr << "GET request failed: " << response.error().message() << "\n";
    }
}

void test_post_request(HttpClient& client, std::string_view url, std::string_view data) {
    std::cout << "POST " << url << "\n";
    std::cout << "Data: " << data << "\n";
    
    HttpHeaders headers;
    headers["content-type"] = "application/json";
    
    auto response = client.post(url, data, headers);
    if (response) {
        print_response(response.value());
    } else {
        std::cerr << "POST request failed: " << response.error().message() << "\n";
    }
}

void test_head_request(HttpClient& client, std::string_view url) {
    std::cout << "HEAD " << url << "\n";
    
    auto response = client.head(url);
    if (response) {
        print_response(response.value());
    } else {
        std::cerr << "HEAD request failed: " << response.error().message() << "\n";
    }
}

void interactive_mode(HttpClient& client) {
    std::cout << "\n=== Interactive HTTP Client ===\n";
    std::cout << "Commands:\n";
    std::cout << "  get <url>                - GET request\n";
    std::cout << "  post <url> <json>        - POST request with JSON data\n";
    std::cout << "  head <url>               - HEAD request\n";
    std::cout << "  delete <url>             - DELETE request\n";
    std::cout << "  quit                     - Exit\n\n";
    
    std::string line;
    while (std::cout << "> " && std::getline(std::cin, line)) {
        if (line.empty()) continue;
        
        std::istringstream iss(line);
        std::string command;
        iss >> command;
        
        if (command == "quit" || command == "exit") {
            break;
        } else if (command == "get") {
            std::string url;
            iss >> url;
            if (url.empty()) {
                std::cout << "Usage: get <url>\n";
                continue;
            }
            test_get_request(client, url);
            
        } else if (command == "post") {
            std::string url, json_data;
            iss >> url;
            std::getline(iss, json_data);
            
            if (url.empty() || json_data.empty()) {
                std::cout << "Usage: post <url> <json>\n";
                continue;
            }
            
            // Trim leading whitespace from json_data
            json_data.erase(0, json_data.find_first_not_of(" \t"));
            test_post_request(client, url, json_data);
            
        } else if (command == "head") {
            std::string url;
            iss >> url;
            if (url.empty()) {
                std::cout << "Usage: head <url>\n";
                continue;
            }
            test_head_request(client, url);
            
        } else if (command == "delete") {
            std::string url;
            iss >> url;
            if (url.empty()) {
                std::cout << "Usage: delete <url>\n";
                continue;
            }
            
            std::cout << "DELETE " << url << "\n";
            auto response = client.delete_resource(url);
            if (response) {
                print_response(response.value());
            } else {
                std::cerr << "DELETE request failed: " << response.error().message() << "\n";
            }
            
        } else {
            std::cout << "Unknown command: " << command << "\n";
        }
    }
}

int main(int argc, char* argv[]) {
    // Initialize networking
    socket_utils::NetworkingRAII networking;
    if (!networking.is_initialized()) {
        std::cerr << "Failed to initialize networking\n";
        return 1;
    }
    
    // Set logging level
    Logger::instance().set_level(LogLevel::Info);
    
    // Create HTTP client with 30 second timeout
    HttpClient client(std::chrono::seconds(30));
    client.set_user_agent("NetworkQuests HTTP Client/1.0");
    
    if (argc > 1) {
        // Command line mode
        std::string url = argv[1];
        
        std::cout << "NetworkQuests HTTP Client\n";
        std::cout << "Testing URL: " << url << "\n\n";
        
        // Test different HTTP methods
        test_get_request(client, url);
        test_head_request(client, url);
        
        // If it looks like an API endpoint, test POST
        if (url.find("/api/") != std::string::npos) {
            std::cout << "Detected API endpoint, testing POST...\n";
            test_post_request(client, url, R"({"name":"Test User"})");
        }
        
    } else {
        // Demonstration mode with local server
        std::cout << "NetworkQuests HTTP Client Demo\n";
        std::cout << "==============================\n\n";
        
        std::cout << "This demo assumes you have the NetworkQuests HTTP server running on localhost:8080\n";
        std::cout << "Start the server with: ./http_server 8080\n\n";
        
        std::string base_url = "http://localhost:8080";
        
        // Test various endpoints
        std::cout << "Testing server status...\n";
        test_get_request(client, base_url + "/api/status");
        
        std::cout << "Testing homepage...\n";
        test_head_request(client, base_url + "/");
        
        std::cout << "Testing user list...\n";
        test_get_request(client, base_url + "/api/users");
        
        std::cout << "Testing specific user...\n";
        test_get_request(client, base_url + "/api/users/1");
        
        std::cout << "Testing user creation...\n";
        test_post_request(client, base_url + "/api/users", R"({"name":"HTTP Client Test User"})");
        
        std::cout << "Testing updated user list...\n";
        test_get_request(client, base_url + "/api/users");
        
        // Interactive mode
        std::cout << "\nWould you like to enter interactive mode? (y/n): ";
        std::string response;
        std::getline(std::cin, response);
        
        if (response == "y" || response == "yes" || response == "Y") {
            interactive_mode(client);
        }
    }
    
    std::cout << "\nDemo complete. Goodbye!\n";
    return 0;
}