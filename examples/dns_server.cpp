#include "networkquests/dns.hpp"
#include "networkquests/logger.hpp"
#include <iostream>
#include <string>
#include <thread>
#include <signal.h>
#include <atomic>
#include <sstream>
#include <fstream>

using namespace NetworkQuests::Dns;

std::atomic<bool> g_running{true};

void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "\nReceived shutdown signal. Stopping server...\n";
        g_running.store(false);
    }
}

class DnsServerApp {
public:
    DnsServerApp() : server_(5353) {  // Use non-privileged port
        Logger::set_level(LogLevel::INFO);
        
        // Setup signal handlers
        signal(SIGINT, signal_handler);
        signal(SIGTERM, signal_handler);
        
        // Display banner
        std::cout << "\n";
        std::cout << "╔══════════════════════════════════════════════════════════════╗\n";
        std::cout << "║                    NetworkQuests DNS Server                  ║\n";
        std::cout << "║                Educational Authoritative Server              ║\n";
        std::cout << "╚══════════════════════════════════════════════════════════════╝\n";
        std::cout << "\n";
        
        setup_default_zones();
    }
    
    void run() {
        std::cout << "Starting DNS server on port 5353...\n";
        std::cout << "Note: Use port 5353 to avoid requiring root privileges\n";
        std::cout << "Test with: dig @127.0.0.1 -p 5353 example.local\n\n";
        
        auto result = server_.start();
        if (!result.has_value()) {
            std::cerr << "Failed to start DNS server: " << result.error().message << std::endl;
            return;
        }
        
        std::cout << "DNS server started successfully!\n";
        show_status();
        
        // Start interactive console in a separate thread
        std::thread console_thread(&DnsServerApp::run_console, this);
        
        // Main server loop
        while (g_running.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        // Cleanup
        server_.stop();
        console_thread.join();
        std::cout << "DNS server stopped.\n";
    }

private:
    DnsServer server_;
    std::shared_ptr<DnsZone> example_zone_;
    std::shared_ptr<DnsZone> test_zone_;
    
    void setup_default_zones() {
        // Create example.local zone
        example_zone_ = std::make_shared<DnsZone>("example.local.");
        
        // Add SOA record
        example_zone_->add_record(DnsResourceRecord(
            "example.local.", DnsType::SOA, DnsClass::IN, 3600,
            std::make_unique<SOARecord>("ns1.example.local.", "admin.example.local.",
                                        2024010101, 3600, 1800, 604800, 86400)
        ));
        
        // Add NS records
        example_zone_->add_record(DnsResourceRecord(
            "example.local.", DnsType::NS, DnsClass::IN, 3600,
            std::make_unique<NSRecord>("ns1.example.local.")
        ));
        
        // Add A records
        example_zone_->add_record(DnsResourceRecord(
            "example.local.", DnsType::A, DnsClass::IN, 300,
            std::make_unique<ARecord>("192.168.1.100")
        ));
        
        example_zone_->add_record(DnsResourceRecord(
            "www.example.local.", DnsType::A, DnsClass::IN, 300,
            std::make_unique<ARecord>("192.168.1.101")
        ));
        
        example_zone_->add_record(DnsResourceRecord(
            "mail.example.local.", DnsType::A, DnsClass::IN, 300,
            std::make_unique<ARecord>("192.168.1.102")
        ));
        
        // Add MX record
        example_zone_->add_record(DnsResourceRecord(
            "example.local.", DnsType::MX, DnsClass::IN, 3600,
            std::make_unique<MXRecord>(10, "mail.example.local.")
        ));
        
        // Add TXT record
        example_zone_->add_record(DnsResourceRecord(
            "example.local.", DnsType::TXT, DnsClass::IN, 300,
            std::make_unique<TXTRecord>("v=spf1 mx ~all")
        ));
        
        // Add CNAME record
        example_zone_->add_record(DnsResourceRecord(
            "ftp.example.local.", DnsType::CNAME, DnsClass::IN, 300,
            std::make_unique<CNAMERecord>("www.example.local.")
        ));
        
        server_.add_zone(example_zone_);
        
        // Create test.local zone
        test_zone_ = std::make_shared<DnsZone>("test.local.");
        
        // Add basic records for test zone
        test_zone_->add_record(DnsResourceRecord(
            "test.local.", DnsType::SOA, DnsClass::IN, 3600,
            std::make_unique<SOARecord>("ns1.test.local.", "admin.test.local.",
                                        2024010101, 3600, 1800, 604800, 86400)
        ));
        
        test_zone_->add_record(DnsResourceRecord(
            "test.local.", DnsType::A, DnsClass::IN, 300,
            std::make_unique<ARecord>("10.0.0.1")
        ));
        
        test_zone_->add_record(DnsResourceRecord(
            "api.test.local.", DnsType::A, DnsClass::IN, 300,
            std::make_unique<ARecord>("10.0.0.2")
        ));
        
        server_.add_zone(test_zone_);
        
        Logger::log(LogLevel::INFO, "Default zones configured: example.local, test.local");
    }
    
    void run_console() {
        std::cout << "\nInteractive console started. Type 'help' for commands.\n";
        
        while (g_running.load()) {
            std::cout << "dns-server> ";
            std::string line;
            if (!std::getline(std::cin, line)) {
                break;  // EOF
            }
            
            if (line.empty()) continue;
            
            std::istringstream iss(line);
            std::string command;
            iss >> command;
            
            if (command == "help") {
                show_help();
            } else if (command == "status") {
                show_status();
            } else if (command == "zones") {
                show_zones();
            } else if (command == "records") {
                show_records(iss);
            } else if (command == "add") {
                add_record(iss);
            } else if (command == "remove") {
                remove_record(iss);
            } else if (command == "load") {
                load_zone_file(iss);
            } else if (command == "stats") {
                show_statistics();
            } else if (command == "test") {
                run_self_test();
            } else if (command == "recursion") {
                handle_recursion(iss);
            } else if (command == "quit" || command == "exit") {
                g_running.store(false);
                break;
            } else if (command == "clear") {
                system("clear");  // Unix/Linux clear screen
            } else {
                std::cout << "Unknown command: " << command << "\n";
                std::cout << "Type 'help' for available commands.\n";
            }
        }
    }
    
    void show_help() {
        std::cout << "\nAvailable commands:\n";
        std::cout << "  help                       - Show this help message\n";
        std::cout << "  status                     - Show server status\n";
        std::cout << "  zones                      - List all configured zones\n";
        std::cout << "  records <zone>             - Show records for a zone\n";
        std::cout << "  add <zone> <name> <type> <ttl> <data> - Add a record\n";
        std::cout << "  remove <zone> <name> <type> - Remove a record\n";
        std::cout << "  load <zone> <file>         - Load zone from file\n";
        std::cout << "  recursion [on|off]         - Enable/disable recursion\n";
        std::cout << "  stats                      - Show server statistics\n";
        std::cout << "  test                       - Run self-tests\n";
        std::cout << "  clear                      - Clear screen\n";
        std::cout << "  quit, exit                 - Stop server and exit\n";
        std::cout << "\nExample:\n";
        std::cout << "  add example.local host1 A 300 192.168.1.50\n";
        std::cout << "  records example.local\n";
    }
    
    void show_status() {
        std::cout << "\n=== DNS Server Status ===\n";
        std::cout << "Running: " << (server_.is_running() ? "Yes" : "No") << "\n";
        std::cout << "Port: 5353\n";
        std::cout << "Zones loaded: 2 (example.local, test.local)\n";
        std::cout << "Recursion: Disabled (educational mode)\n";
        std::cout << "\nTest commands:\n";
        std::cout << "  dig @127.0.0.1 -p 5353 example.local\n";
        std::cout << "  dig @127.0.0.1 -p 5353 www.example.local\n";
        std::cout << "  dig @127.0.0.1 -p 5353 example.local MX\n";
        std::cout << "  nslookup -port=5353 example.local 127.0.0.1\n";
    }
    
    void show_zones() {
        std::cout << "\n=== Configured Zones ===\n";
        std::cout << "1. example.local. (demonstration zone)\n";
        std::cout << "   - Complete example with A, MX, NS, SOA, TXT, CNAME records\n";
        std::cout << "2. test.local. (testing zone)\n";
        std::cout << "   - Simple zone for testing purposes\n";
        std::cout << "\nUse 'records <zone>' to see all records in a zone.\n";
    }
    
    void show_records(std::istringstream& iss) {
        std::string zone_name;
        if (!(iss >> zone_name)) {
            std::cout << "Usage: records <zone_name>\n";
            return;
        }
        
        std::shared_ptr<DnsZone> zone;
        if (zone_name == "example.local" || zone_name == "example.local.") {
            zone = example_zone_;
        } else if (zone_name == "test.local" || zone_name == "test.local.") {
            zone = test_zone_;
        } else {
            std::cout << "Zone not found: " << zone_name << "\n";
            return;
        }
        
        std::cout << "\n=== Records for " << zone->get_origin() << " ===\n";
        
        // Get all record types
        std::vector<DnsType> types = {
            DnsType::SOA, DnsType::NS, DnsType::A, 
            DnsType::AAAA, DnsType::MX, DnsType::TXT, DnsType::CNAME
        };
        
        for (auto type : types) {
            auto records = zone->find_records(zone->get_origin(), type, DnsClass::IN);
            for (const auto& record : records) {
                std::cout << "  " << record.to_string() << "\n";
            }
            
            // Also check for subdomains (simplified)
            if (type == DnsType::A || type == DnsType::CNAME) {
                std::vector<std::string> subdomains;
                if (zone_name.find("example") != std::string::npos) {
                    subdomains = {"www.example.local.", "mail.example.local.", "ftp.example.local."};
                } else if (zone_name.find("test") != std::string::npos) {
                    subdomains = {"api.test.local."};
                }
                
                for (const auto& subdomain : subdomains) {
                    auto sub_records = zone->find_records(subdomain, type, DnsClass::IN);
                    for (const auto& record : sub_records) {
                        std::cout << "  " << record.to_string() << "\n";
                    }
                }
            }
        }
    }
    
    void add_record(std::istringstream& iss) {
        std::string zone_name, record_name, type_str, ttl_str, data;
        if (!(iss >> zone_name >> record_name >> type_str >> ttl_str)) {
            std::cout << "Usage: add <zone> <name> <type> <ttl> <data>\n";
            return;
        }
        
        // Read rest of line as data
        std::getline(iss, data);
        if (!data.empty() && data[0] == ' ') {
            data = data.substr(1);  // Remove leading space
        }
        
        if (data.empty()) {
            std::cout << "Missing record data\n";
            return;
        }
        
        auto type_result = Utils::string_to_dns_type(type_str);
        if (!type_result.has_value()) {
            std::cout << "Invalid record type: " << type_str << "\n";
            return;
        }
        
        uint32_t ttl;
        try {
            ttl = std::stoul(ttl_str);
        } catch (const std::exception&) {
            std::cout << "Invalid TTL: " << ttl_str << "\n";
            return;
        }
        
        std::shared_ptr<DnsZone> zone;
        if (zone_name == "example.local" || zone_name == "example.local.") {
            zone = example_zone_;
        } else if (zone_name == "test.local" || zone_name == "test.local.") {
            zone = test_zone_;
        } else {
            std::cout << "Zone not found: " << zone_name << "\n";
            return;
        }
        
        try {
            std::unique_ptr<DnsRData> rdata;
            switch (type_result.value()) {
                case DnsType::A:
                    rdata = std::make_unique<ARecord>(data);
                    break;
                case DnsType::AAAA:
                    rdata = std::make_unique<AAAARecord>(data);
                    break;
                case DnsType::NS:
                    rdata = std::make_unique<NSRecord>(data);
                    break;
                case DnsType::CNAME:
                    rdata = std::make_unique<CNAMERecord>(data);
                    break;
                case DnsType::TXT:
                    rdata = std::make_unique<TXTRecord>(data);
                    break;
                default:
                    std::cout << "Record type not supported for manual addition: " << type_str << "\n";
                    return;
            }
            
            if (rdata) {
                DnsResourceRecord record(record_name, type_result.value(), DnsClass::IN, ttl, std::move(rdata));
                zone->add_record(record);
                std::cout << "Record added successfully\n";
            }
        } catch (const std::exception& e) {
            std::cout << "Failed to add record: " << e.what() << "\n";
        }
    }
    
    void remove_record(std::istringstream& iss) {
        std::string zone_name, record_name, type_str;
        if (!(iss >> zone_name >> record_name >> type_str)) {
            std::cout << "Usage: remove <zone> <name> <type>\n";
            return;
        }
        
        auto type_result = Utils::string_to_dns_type(type_str);
        if (!type_result.has_value()) {
            std::cout << "Invalid record type: " << type_str << "\n";
            return;
        }
        
        std::shared_ptr<DnsZone> zone;
        if (zone_name == "example.local" || zone_name == "example.local.") {
            zone = example_zone_;
        } else if (zone_name == "test.local" || zone_name == "test.local.") {
            zone = test_zone_;
        } else {
            std::cout << "Zone not found: " << zone_name << "\n";
            return;
        }
        
        zone->remove_record(record_name, type_result.value());
        std::cout << "Record removed (if it existed)\n";
    }
    
    void load_zone_file(std::istringstream& iss) {
        std::string zone_name, filename;
        if (!(iss >> zone_name >> filename)) {
            std::cout << "Usage: load <zone> <filename>\n";
            return;
        }
        
        std::shared_ptr<DnsZone> zone;
        if (zone_name == "example.local" || zone_name == "example.local.") {
            zone = example_zone_;
        } else if (zone_name == "test.local" || zone_name == "test.local.") {
            zone = test_zone_;
        } else {
            std::cout << "Zone not found: " << zone_name << "\n";
            return;
        }
        
        std::ifstream file(filename);
        if (!file) {
            std::cout << "Cannot open file: " << filename << "\n";
            return;
        }
        
        std::string content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
        
        auto result = zone->load_from_string(content);
        if (result.has_value()) {
            std::cout << "Zone file loaded successfully\n";
        } else {
            std::cout << "Failed to load zone file: " << result.error().message << "\n";
        }
    }
    
    void handle_recursion(std::istringstream& iss) {
        std::string action;
        if (!(iss >> action)) {
            std::cout << "Recursion is currently disabled (educational mode)\n";
            std::cout << "Usage: recursion [on|off]\n";
            return;
        }
        
        if (action == "on") {
            server_.set_recursion_enabled(true);
            std::cout << "Recursion enabled\n";
        } else if (action == "off") {
            server_.set_recursion_enabled(false);
            std::cout << "Recursion disabled\n";
        } else {
            std::cout << "Invalid option. Use 'on' or 'off'\n";
        }
    }
    
    void show_statistics() {
        std::cout << "\n=== Server Statistics ===\n";
        std::cout << "This is a basic educational implementation.\n";
        std::cout << "In a production server, you would see:\n";
        std::cout << "- Query count by type\n";
        std::cout << "- Response time statistics\n";
        std::cout << "- Cache hit/miss ratios\n";
        std::cout << "- Client connection stats\n";
        std::cout << "- Error rates\n";
    }
    
    void run_self_test() {
        std::cout << "\n=== Running DNS Server Self-Test ===\n";
        
        // Create a test client
        DnsClient test_client("127.0.0.1", 5353);
        
        struct TestCase {
            std::string name;
            std::string domain;
            DnsType type;
            bool should_succeed;
        };
        
        std::vector<TestCase> tests = {
            {"Example.local A record", "example.local", DnsType::A, true},
            {"WWW subdomain", "www.example.local", DnsType::A, true},
            {"MX record", "example.local", DnsType::MX, true},
            {"TXT record", "example.local", DnsType::TXT, true},
            {"CNAME record", "ftp.example.local", DnsType::CNAME, true},
            {"Non-existent domain", "nonexistent.local", DnsType::A, false},
            {"Test zone", "test.local", DnsType::A, true},
            {"API subdomain", "api.test.local", DnsType::A, true}
        };
        
        int passed = 0;
        int total = tests.size();
        
        for (const auto& test : tests) {
            std::cout << "\nTesting: " << test.name << "\n";
            
            auto result = test_client.query(test.domain, test.type);
            
            if (test.should_succeed) {
                if (result.has_value() && !result.value().get_answers().empty()) {
                    std::cout << "✓ PASS - Got " << result.value().get_answers().size() << " answers\n";
                    passed++;
                } else {
                    std::cout << "✗ FAIL - Expected success but got no answers\n";
                }
            } else {
                if (!result.has_value() || result.value().get_answers().empty()) {
                    std::cout << "✓ PASS - Correctly failed as expected\n";
                    passed++;
                } else {
                    std::cout << "✗ FAIL - Expected failure but got answers\n";
                }
            }
        }
        
        std::cout << "\n=== Self-Test Results ===\n";
        std::cout << "Passed: " << passed << "/" << total << "\n";
        if (passed == total) {
            std::cout << "🎉 All tests passed! Server is working correctly.\n";
        } else {
            std::cout << "⚠️  Some tests failed. Check server configuration.\n";
        }
    }
};

int main() {
    try {
        DnsServerApp app;
        app.run();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}