#include "networkquests/dns.hpp"
#include "networkquests/logger.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>

using namespace NetworkQuests::Dns;

class DnsClientApp {
public:
    DnsClientApp() : client_("8.8.8.8") {
        Logger::set_level(LogLevel::INFO);
        Logger::log(LogLevel::INFO, "DNS Client Example Started");
        
        // Display banner
        std::cout << "\n";
        std::cout << "╔══════════════════════════════════════════════════════════════╗\n";
        std::cout << "║                    NetworkQuests DNS Client                  ║\n";
        std::cout << "║                  Educational DNS Resolver                    ║\n";
        std::cout << "╚══════════════════════════════════════════════════════════════╝\n";
        std::cout << "\n";
    }
    
    void run() {
        show_help();
        
        while (true) {
            std::cout << "\ndns> ";
            std::string line;
            if (!std::getline(std::cin, line)) {
                break;  // EOF
            }
            
            std::istringstream iss(line);
            std::string command;
            iss >> command;
            
            if (command.empty()) continue;
            
            if (command == "help" || command == "?") {
                show_help();
            } else if (command == "quit" || command == "exit") {
                break;
            } else if (command == "server") {
                handle_server_command(iss);
            } else if (command == "timeout") {
                handle_timeout_command(iss);
            } else if (command == "retries") {
                handle_retries_command(iss);
            } else if (command == "cache") {
                handle_cache_command(iss);
            } else if (command == "query" || command == "q") {
                handle_query_command(iss);
            } else if (command == "resolve" || command == "r") {
                handle_resolve_command(iss);
            } else if (command == "reverse" || command == "ptr") {
                handle_reverse_command(iss);
            } else if (command == "test") {
                run_tests();
            } else if (command == "benchmark") {
                run_benchmark();
            } else {
                std::cout << "Unknown command: " << command << "\n";
                std::cout << "Type 'help' for available commands.\n";
            }
        }
        
        std::cout << "\nGoodbye!\n";
    }

private:
    DnsClient client_;
    
    void show_help() {
        std::cout << "Available commands:\n";
        std::cout << "  help, ?                    - Show this help message\n";
        std::cout << "  server <ip> [port]         - Set DNS server (default: 8.8.8.8:53)\n";
        std::cout << "  timeout <ms>               - Set query timeout in milliseconds\n";
        std::cout << "  retries <n>                - Set number of retries\n";
        std::cout << "  cache [clear]              - Show cache status or clear cache\n";
        std::cout << "  query <domain> [type]      - Query specific record type\n";
        std::cout << "  resolve <domain>           - Resolve A records (IPv4 addresses)\n";
        std::cout << "  reverse <ip>               - Reverse DNS lookup (PTR record)\n";
        std::cout << "  test                       - Run comprehensive tests\n";
        std::cout << "  benchmark                  - Run performance benchmark\n";
        std::cout << "  quit, exit                 - Exit the program\n";
        std::cout << "\nRecord types: A, AAAA, NS, MX, TXT, CNAME, PTR, SOA\n";
        std::cout << "Examples:\n";
        std::cout << "  resolve google.com\n";
        std::cout << "  query google.com MX\n";
        std::cout << "  reverse 8.8.8.8\n";
    }
    
    void handle_server_command(std::istringstream& iss) {
        std::string server;
        uint16_t port = 53;
        
        if (!(iss >> server)) {
            std::cout << "Usage: server <ip> [port]\n";
            return;
        }
        
        iss >> port;  // Optional port
        
        client_.set_server(server, port);
        std::cout << "DNS server set to " << server << ":" << port << "\n";
    }
    
    void handle_timeout_command(std::istringstream& iss) {
        int timeout_ms;
        if (!(iss >> timeout_ms) || timeout_ms <= 0) {
            std::cout << "Usage: timeout <milliseconds>\n";
            return;
        }
        
        client_.set_timeout(std::chrono::milliseconds(timeout_ms));
        std::cout << "Query timeout set to " << timeout_ms << "ms\n";
    }
    
    void handle_retries_command(std::istringstream& iss) {
        int retries;
        if (!(iss >> retries) || retries < 0) {
            std::cout << "Usage: retries <number>\n";
            return;
        }
        
        client_.set_retries(retries);
        std::cout << "Retries set to " << retries << "\n";
    }
    
    void handle_cache_command(std::istringstream& iss) {
        std::string action;
        iss >> action;
        
        if (action == "clear") {
            client_.clear_cache();
            std::cout << "DNS cache cleared\n";
        } else {
            std::cout << "Cache size: " << client_.get_cache_size() << " entries\n";
            std::cout << "Use 'cache clear' to clear the cache\n";
        }
    }
    
    void handle_query_command(std::istringstream& iss) {
        std::string domain, type_str = "A";
        if (!(iss >> domain)) {
            std::cout << "Usage: query <domain> [type]\n";
            return;
        }
        
        iss >> type_str;  // Optional type
        
        auto type_result = Utils::string_to_dns_type(type_str);
        if (!type_result.has_value()) {
            std::cout << "Invalid record type: " << type_str << "\n";
            return;
        }
        
        std::cout << "Querying " << domain << " for " << type_str << " records...\n";
        
        auto start_time = std::chrono::steady_clock::now();
        auto result = client_.query(domain, type_result.value());
        auto end_time = std::chrono::steady_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (!result.has_value()) {
            std::cout << "Query failed: " << result.error().message << "\n";
            return;
        }
        
        auto response = result.value();
        std::cout << "\nQuery completed in " << duration.count() << "ms\n";
        std::cout << response.to_string() << "\n";
    }
    
    void handle_resolve_command(std::istringstream& iss) {
        std::string domain;
        if (!(iss >> domain)) {
            std::cout << "Usage: resolve <domain>\n";
            return;
        }
        
        std::cout << "Resolving " << domain << "...\n";
        
        // Try IPv4 first
        auto ipv4_result = client_.resolve_a(domain);
        if (ipv4_result.has_value() && !ipv4_result.value().empty()) {
            std::cout << "\nIPv4 addresses:\n";
            for (const auto& ip : ipv4_result.value()) {
                std::cout << "  " << ip << "\n";
            }
        }
        
        // Try IPv6
        auto ipv6_result = client_.resolve_aaaa(domain);
        if (ipv6_result.has_value() && !ipv6_result.value().empty()) {
            std::cout << "\nIPv6 addresses:\n";
            for (const auto& ip : ipv6_result.value()) {
                std::cout << "  " << ip << "\n";
            }
        }
        
        if ((!ipv4_result.has_value() || ipv4_result.value().empty()) &&
            (!ipv6_result.has_value() || ipv6_result.value().empty())) {
            std::cout << "No A or AAAA records found\n";
        }
    }
    
    void handle_reverse_command(std::istringstream& iss) {
        std::string ip;
        if (!(iss >> ip)) {
            std::cout << "Usage: reverse <ip_address>\n";
            return;
        }
        
        std::cout << "Reverse lookup for " << ip << "...\n";
        
        auto result = client_.resolve_ptr(ip);
        if (result.has_value()) {
            std::cout << "Hostname: " << result.value() << "\n";
        } else {
            std::cout << "Reverse lookup failed: " << result.error().message << "\n";
        }
    }
    
    void run_tests() {
        std::cout << "\n=== Running DNS Client Tests ===\n";
        
        struct TestCase {
            std::string name;
            std::string domain;
            DnsType type;
        };
        
        std::vector<TestCase> tests = {
            {"Google A Record", "google.com", DnsType::A},
            {"Google MX Records", "google.com", DnsType::MX},
            {"Google NS Records", "google.com", DnsType::NS},
            {"Google TXT Records", "google.com", DnsType::TXT},
            {"IPv6 Records", "ipv6.google.com", DnsType::AAAA},
            {"Root Servers", ".", DnsType::NS}
        };
        
        int passed = 0;
        int total = tests.size();
        
        for (const auto& test : tests) {
            std::cout << "\nTesting: " << test.name << "\n";
            std::cout << "Query: " << test.domain << " " << Utils::dns_type_to_string(test.type) << "\n";
            
            auto start = std::chrono::steady_clock::now();
            auto result = client_.query(test.domain, test.type);
            auto end = std::chrono::steady_clock::now();
            
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            
            if (result.has_value()) {
                auto response = result.value();
                std::cout << "✓ Success (" << duration.count() << "ms)\n";
                std::cout << "  Answers: " << response.get_answers().size() << "\n";
                std::cout << "  Authorities: " << response.get_authorities().size() << "\n";
                std::cout << "  Additionals: " << response.get_additionals().size() << "\n";
                passed++;
            } else {
                std::cout << "✗ Failed: " << result.error().message << "\n";
            }
        }
        
        std::cout << "\n=== Test Results ===\n";
        std::cout << "Passed: " << passed << "/" << total << "\n";
        if (passed == total) {
            std::cout << "🎉 All tests passed!\n";
        }
    }
    
    void run_benchmark() {
        std::cout << "\n=== DNS Performance Benchmark ===\n";
        
        std::vector<std::string> domains = {
            "google.com", "facebook.com", "youtube.com", "amazon.com",
            "wikipedia.org", "twitter.com", "instagram.com", "reddit.com"
        };
        
        const int iterations = 3;
        std::vector<std::chrono::milliseconds> times;
        
        std::cout << "Testing " << domains.size() << " domains, " << iterations << " iterations each...\n";
        
        for (const auto& domain : domains) {
            std::cout << "\nTesting " << domain << ":\n";
            
            for (int i = 0; i < iterations; ++i) {
                auto start = std::chrono::steady_clock::now();
                auto result = client_.resolve_a(domain);
                auto end = std::chrono::steady_clock::now();
                
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
                times.push_back(duration);
                
                if (result.has_value() && !result.value().empty()) {
                    std::cout << "  Iteration " << (i+1) << ": " << duration.count() << "ms (" 
                              << result.value().size() << " records)\n";
                } else {
                    std::cout << "  Iteration " << (i+1) << ": " << duration.count() << "ms (failed)\n";
                }
            }
        }
        
        // Calculate statistics
        if (!times.empty()) {
            auto total_time = std::chrono::milliseconds(0);
            auto min_time = times[0];
            auto max_time = times[0];
            
            for (const auto& time : times) {
                total_time += time;
                min_time = std::min(min_time, time);
                max_time = std::max(max_time, time);
            }
            
            auto avg_time = total_time / times.size();
            
            std::cout << "\n=== Benchmark Results ===\n";
            std::cout << "Total queries: " << times.size() << "\n";
            std::cout << "Average time: " << avg_time.count() << "ms\n";
            std::cout << "Minimum time: " << min_time.count() << "ms\n";
            std::cout << "Maximum time: " << max_time.count() << "ms\n";
            std::cout << "Cache entries: " << client_.get_cache_size() << "\n";
        }
    }
};

int main() {
    try {
        DnsClientApp app;
        app.run();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}