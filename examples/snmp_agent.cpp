#include "networkquests/snmp.hpp"
#include "networkquests/logger.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <atomic>
#include <random>

using namespace networkquests;
using namespace networkquests::snmp;

class SnmpAgentExample {
public:
    SnmpAgentExample() : agent_(161), running_(false), stats_enabled_(false) {
        // Initialize agent configuration
        setup_agent();
        setup_custom_mib();
    }
    
    void run() {
        show_banner();
        
        while (true) {
            show_menu();
            
            std::string choice;
            std::cout << "Enter your choice (1-8, q to quit): ";
            std::getline(std::cin, choice);
            
            if (choice == "q" || choice == "Q") {
                break;
            }
            
            try {
                switch (std::stoi(choice)) {
                    case 1: start_agent(); break;
                    case 2: stop_agent(); break;
                    case 3: show_agent_status(); break;
                    case 4: manage_mib_objects(); break;
                    case 5: send_test_trap(); break;
                    case 6: configure_communities(); break;
                    case 7: start_statistics_monitoring(); break;
                    case 8: test_agent_functionality(); break;
                    default:
                        std::cout << "Invalid choice. Please try again.\n\n";
                        break;
                }
            } catch (const std::exception& e) {
                std::cout << "Error: " << e.what() << "\n\n";
            }
        }
        
        if (agent_.is_running()) {
            std::cout << "Stopping agent...\n";
            agent_.stop();
        }
        
        if (stats_enabled_) {
            stats_enabled_ = false;
            if (stats_thread_.joinable()) {
                stats_thread_.join();
            }
        }
    }

private:
    SnmpAgent agent_;
    std::atomic<bool> running_;
    std::atomic<bool> stats_enabled_;
    std::thread stats_thread_;
    
    void show_banner() {
        std::cout << "========================================\n"
                  << "    NetworkQuests SNMP Agent Demo\n"
                  << "========================================\n\n";
    }
    
    void show_menu() {
        std::cout << "SNMP Agent Operations:\n"
                  << "1. Start SNMP Agent\n"
                  << "2. Stop SNMP Agent\n"
                  << "3. Show Agent Status\n"
                  << "4. Manage MIB Objects\n"
                  << "5. Send Test Trap\n"
                  << "6. Configure Communities\n"
                  << "7. Start Statistics Monitoring\n"
                  << "8. Test Agent Functionality\n"
                  << "q. Quit\n\n";
    }
    
    void setup_agent() {
        // Configure system information
        agent_.set_system_description("NetworkQuests SNMP Agent Demo v1.0");
        agent_.set_system_contact("admin@networkquests.org");
        agent_.set_system_name("NetworkQuests-SNMP-Demo");
        agent_.set_system_location("Demo Environment");
        
        // Setup trap handler
        agent_.set_trap_handler([this](const SnmpMessage& trap, const SocketAddress& sender) {
            handle_received_trap(trap, sender);
        });
    }
    
    void setup_custom_mib() {
        // Add some custom MIB objects for demonstration
        
        // Custom enterprise OID: 1.3.6.1.4.1.12345 (example)
        ObjectIdentifier enterprise_base("1.3.6.1.4.1.12345");
        
        // Demo string value
        ObjectIdentifier demo_string = enterprise_base;
        demo_string.append(1);
        demo_string.append(1);
        demo_string.append(0);
        agent_.add_static_object(demo_string, std::string("NetworkQuests Demo String"), DataType::OCTET_STRING, true);
        
        // Demo integer value
        ObjectIdentifier demo_int = enterprise_base;
        demo_int.append(1);
        demo_int.append(2);
        demo_int.append(0);
        agent_.add_static_object(demo_int, static_cast<int32_t>(42), DataType::INTEGER, true);
        
        // Demo counter (read-only)
        ObjectIdentifier demo_counter = enterprise_base;
        demo_counter.append(1);
        demo_counter.append(3);
        demo_counter.append(0);
        
        auto counter_obj = std::make_unique<MibObject>(demo_counter, DataType::COUNTER, false);
        counter_obj->set_get_handler([this]() -> Result<SnmpValue> {
            static uint32_t counter = 0;
            return SnmpValue(++counter);
        });
        agent_.add_mib_object(std::move(counter_obj));
        
        // Demo gauge (system load simulation)
        ObjectIdentifier demo_gauge = enterprise_base;
        demo_gauge.append(1);
        demo_gauge.append(4);
        demo_gauge.append(0);
        
        auto gauge_obj = std::make_unique<MibObject>(demo_gauge, DataType::GAUGE, false);
        gauge_obj->set_get_handler([this]() -> Result<SnmpValue> {
            static std::random_device rd;
            static std::mt19937 gen(rd());
            static std::uniform_int_distribution<uint32_t> dis(0, 100);
            return SnmpValue(dis(gen)); // Random load 0-100%
        });
        agent_.add_mib_object(std::move(gauge_obj));
        
        // Demo time ticks
        ObjectIdentifier demo_ticks = enterprise_base;
        demo_ticks.append(1);
        demo_ticks.append(5);
        demo_ticks.append(0);
        
        auto ticks_obj = std::make_unique<MibObject>(demo_ticks, DataType::TIME_TICKS, false);
        ticks_obj->set_get_handler([this]() -> Result<SnmpValue> {
            auto now = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
            return SnmpValue(static_cast<uint32_t>(duration.count() / 10)); // centiseconds
        });
        agent_.add_mib_object(std::move(ticks_obj));
    }
    
    void start_agent() {
        if (agent_.is_running()) {
            std::cout << "Agent is already running.\n\n";
            return;
        }
        
        std::string port_str;
        std::cout << "Enter port number [161]: ";
        std::getline(std::cin, port_str);
        
        if (!port_str.empty()) {
            Port port = static_cast<Port>(std::stoi(port_str));
            agent_ = SnmpAgent(port);
            setup_agent();
            setup_custom_mib();
        }
        
        std::cout << "Starting SNMP agent...\n";
        auto result = agent_.start();
        
        if (result) {
            std::cout << "SNMP agent started successfully!\n";
            std::cout << "Listening on: " << agent_.local_address().to_string() << "\n";
            std::cout << "Agent is ready to handle SNMP requests.\n\n";
            
            // Print some useful information
            auto mib_oids = agent_.get_mib_oids();
            std::cout << "MIB contains " << mib_oids.size() << " objects.\n";
            std::cout << "Default communities: read='public', write='private'\n\n";
        } else {
            std::cout << "Failed to start agent: " << result.error() << "\n\n";
        }
    }
    
    void stop_agent() {
        if (!agent_.is_running()) {
            std::cout << "Agent is not running.\n\n";
            return;
        }
        
        std::cout << "Stopping SNMP agent...\n";
        agent_.stop();
        std::cout << "SNMP agent stopped.\n\n";
    }
    
    void show_agent_status() {
        std::cout << "SNMP Agent Status:\n";
        std::cout << "Status: " << (agent_.is_running() ? "Running" : "Stopped") << "\n";
        
        if (agent_.is_running()) {
            std::cout << "Local address: " << agent_.local_address().to_string() << "\n";
            std::cout << "Active connections: " << agent_.active_connections() << "\n";
            
            auto mib_oids = agent_.get_mib_oids();
            std::cout << "MIB objects: " << mib_oids.size() << "\n";
            
            std::cout << "\nAvailable OIDs (first 10):\n";
            for (size_t i = 0; i < std::min(mib_oids.size(), size_t(10)); ++i) {
                std::cout << "  " << mib_oids[i].to_string() << "\n";
            }
            if (mib_oids.size() > 10) {
                std::cout << "  ... and " << (mib_oids.size() - 10) << " more\n";
            }
        }
        std::cout << "\n";
    }
    
    void manage_mib_objects() {
        if (!agent_.is_running()) {
            std::cout << "Agent must be running to manage MIB objects.\n\n";
            return;
        }
        
        std::cout << "MIB Object Management:\n";
        std::cout << "1. Add new object\n";
        std::cout << "2. List all objects\n";
        std::cout << "3. Remove object\n";
        std::cout << "4. Back to main menu\n\n";
        
        std::string choice;
        std::cout << "Enter choice: ";
        std::getline(std::cin, choice);
        
        try {
            switch (std::stoi(choice)) {
                case 1: add_mib_object(); break;
                case 2: list_mib_objects(); break;
                case 3: remove_mib_object(); break;
                case 4: return;
                default: std::cout << "Invalid choice.\n\n"; break;
            }
        } catch (const std::exception& e) {
            std::cout << "Error: " << e.what() << "\n\n";
        }
    }
    
    void add_mib_object() {
        std::string oid_str;
        std::cout << "Enter OID: ";
        std::getline(std::cin, oid_str);
        
        auto oid_result = ObjectIdentifier::from_string(oid_str);
        if (!oid_result) {
            std::cout << "Invalid OID format: " << oid_result.error() << "\n\n";
            return;
        }
        
        std::string value;
        std::cout << "Enter value: ";
        std::getline(std::cin, value);
        
        std::string writable_str;
        std::cout << "Is writable? (y/n) [n]: ";
        std::getline(std::cin, writable_str);
        bool writable = (writable_str == "y" || writable_str == "Y");
        
        agent_.add_static_object(oid_result.value(), value, DataType::OCTET_STRING, writable);
        std::cout << "MIB object added successfully!\n\n";
    }
    
    void list_mib_objects() {
        auto oids = agent_.get_mib_oids();
        std::cout << "MIB Objects (" << oids.size() << " total):\n\n";
        
        for (const auto& oid : oids) {
            std::cout << "  " << oid.to_string() << "\n";
        }
        std::cout << "\n";
    }
    
    void remove_mib_object() {
        std::string oid_str;
        std::cout << "Enter OID to remove: ";
        std::getline(std::cin, oid_str);
        
        auto oid_result = ObjectIdentifier::from_string(oid_str);
        if (!oid_result) {
            std::cout << "Invalid OID format: " << oid_result.error() << "\n\n";
            return;
        }
        
        agent_.remove_mib_object(oid_result.value());
        std::cout << "MIB object removed (if it existed).\n\n";
    }
    
    void send_test_trap() {
        if (!agent_.is_running()) {
            std::cout << "Agent must be running to send traps.\n\n";
            return;
        }
        
        std::string manager_host;
        std::cout << "Enter manager host [127.0.0.1]: ";
        std::getline(std::cin, manager_host);
        if (manager_host.empty()) {
            manager_host = "127.0.0.1";
        }
        
        std::string port_str;
        std::cout << "Enter manager port [162]: ";
        std::getline(std::cin, port_str);
        Port port = port_str.empty() ? 162 : static_cast<Port>(std::stoi(port_str));
        
        SocketAddress manager_addr = SocketAddress::from_string(manager_host, port);
        
        std::cout << "Trap types:\n";
        std::cout << "1. SNMPv2 generic trap\n";
        std::cout << "2. SNMPv1 cold start trap\n";
        std::cout << "3. Custom trap with data\n\n";
        
        std::string trap_choice;
        std::cout << "Enter trap type: ";
        std::getline(std::cin, trap_choice);
        
        std::string community;
        std::cout << "Enter community [public]: ";
        std::getline(std::cin, community);
        if (community.empty()) {
            community = "public";
        }
        
        try {
            switch (std::stoi(trap_choice)) {
                case 1: send_v2_trap(manager_addr, community); break;
                case 2: send_v1_trap(manager_addr, community); break;
                case 3: send_custom_trap(manager_addr, community); break;
                default: std::cout << "Invalid trap type.\n\n"; break;
            }
        } catch (const std::exception& e) {
            std::cout << "Error: " << e.what() << "\n\n";
        }
    }
    
    void send_v2_trap(const SocketAddress& manager_addr, const std::string& community) {
        ObjectIdentifier trap_oid("1.3.6.1.6.3.1.1.5.1"); // Cold start trap
        
        std::vector<VariableBinding> variables;
        
        std::cout << "Sending SNMPv2 trap...\n";
        auto result = agent_.send_trap(manager_addr, trap_oid, variables, community);
        
        if (result) {
            std::cout << "Trap sent successfully to " << manager_addr.to_string() << "\n\n";
        } else {
            std::cout << "Failed to send trap: " << result.error() << "\n\n";
        }
    }
    
    void send_v1_trap(const SocketAddress& manager_addr, const std::string& community) {
        ObjectIdentifier enterprise("1.3.6.1.4.1.12345"); // Example enterprise OID
        
        std::vector<VariableBinding> variables;
        
        std::cout << "Sending SNMPv1 trap...\n";
        auto result = agent_.send_v1_trap(manager_addr, enterprise, GenericTrap::COLD_START, 0, variables, community);
        
        if (result) {
            std::cout << "Trap sent successfully to " << manager_addr.to_string() << "\n\n";
        } else {
            std::cout << "Failed to send trap: " << result.error() << "\n\n";
        }
    }
    
    void send_custom_trap(const SocketAddress& manager_addr, const std::string& community) {
        ObjectIdentifier trap_oid("1.3.6.1.4.1.12345.0.1"); // Custom trap OID
        
        std::vector<VariableBinding> variables;
        
        // Add some custom data
        VariableBinding vb1;
        vb1.set_oid(ObjectIdentifier("1.3.6.1.4.1.12345.1.1.0"));
        vb1.set_value(std::string("Custom trap notification"));
        vb1.set_type(DataType::OCTET_STRING);
        variables.push_back(vb1);
        
        VariableBinding vb2;
        vb2.set_oid(ObjectIdentifier("1.3.6.1.4.1.12345.1.2.0"));
        vb2.set_value(static_cast<int32_t>(123));
        vb2.set_type(DataType::INTEGER);
        variables.push_back(vb2);
        
        std::cout << "Sending custom trap with data...\n";
        auto result = agent_.send_trap(manager_addr, trap_oid, variables, community);
        
        if (result) {
            std::cout << "Custom trap sent successfully to " << manager_addr.to_string() << "\n";
            std::cout << "Trap OID: " << trap_oid.to_string() << "\n";
            std::cout << "Variables: " << variables.size() << "\n\n";
        } else {
            std::cout << "Failed to send trap: " << result.error() << "\n\n";
        }
    }
    
    void configure_communities() {
        std::cout << "Community Configuration:\n";
        std::cout << "1. Add read community\n";
        std::cout << "2. Add write community\n";
        std::cout << "3. Remove community\n";
        std::cout << "4. Back to main menu\n\n";
        
        std::string choice;
        std::cout << "Enter choice: ";
        std::getline(std::cin, choice);
        
        try {
            switch (std::stoi(choice)) {
                case 1: {
                    std::string community;
                    std::cout << "Enter read community name: ";
                    std::getline(std::cin, community);
                    agent_.add_read_community(community);
                    std::cout << "Read community '" << community << "' added.\n\n";
                    break;
                }
                case 2: {
                    std::string community;
                    std::cout << "Enter write community name: ";
                    std::getline(std::cin, community);
                    agent_.add_write_community(community);
                    std::cout << "Write community '" << community << "' added.\n\n";
                    break;
                }
                case 3: {
                    std::string community;
                    std::cout << "Enter community name to remove: ";
                    std::getline(std::cin, community);
                    agent_.remove_community(community);
                    std::cout << "Community '" << community << "' removed.\n\n";
                    break;
                }
                case 4:
                    return;
                default:
                    std::cout << "Invalid choice.\n\n";
                    break;
            }
        } catch (const std::exception& e) {
            std::cout << "Error: " << e.what() << "\n\n";
        }
    }
    
    void start_statistics_monitoring() {
        if (stats_enabled_) {
            std::cout << "Statistics monitoring is already running.\n\n";
            return;
        }
        
        if (!agent_.is_running()) {
            std::cout << "Agent must be running to monitor statistics.\n\n";
            return;
        }
        
        stats_enabled_ = true;
        stats_thread_ = std::thread(&SnmpAgentExample::statistics_monitor, this);
        
        std::cout << "Statistics monitoring started.\n";
        std::cout << "Press Enter to stop monitoring...\n";
        
        std::string dummy;
        std::getline(std::cin, dummy);
        
        stats_enabled_ = false;
        if (stats_thread_.joinable()) {
            stats_thread_.join();
        }
        
        std::cout << "Statistics monitoring stopped.\n\n";
    }
    
    void statistics_monitor() {
        auto last_connections = agent_.active_connections();
        
        while (stats_enabled_) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            
            if (!stats_enabled_) break;
            
            auto current_connections = agent_.active_connections();
            auto mib_size = agent_.get_mib_oids().size();
            
            std::cout << "\r[STATS] Active: " << current_connections 
                      << ", MIB Objects: " << mib_size 
                      << ", Address: " << agent_.local_address().to_string()
                      << std::flush;
            
            last_connections = current_connections;
        }
        std::cout << "\n";
    }
    
    void test_agent_functionality() {
        if (!agent_.is_running()) {
            std::cout << "Agent must be running to test functionality.\n\n";
            return;
        }
        
        std::cout << "Testing Agent Functionality:\n\n";
        
        // Test 1: Check system objects
        std::cout << "1. Testing system MIB objects...\n";
        auto system_oids = agent_.get_mib_oids();
        size_t system_count = 0;
        for (const auto& oid : system_oids) {
            if (oid.to_string().starts_with("1.3.6.1.2.1.1")) {
                system_count++;
            }
        }
        std::cout << "   Found " << system_count << " system MIB objects\n";
        
        // Test 2: Check custom objects
        std::cout << "2. Testing custom MIB objects...\n";
        size_t custom_count = 0;
        for (const auto& oid : system_oids) {
            if (oid.to_string().starts_with("1.3.6.1.4.1.12345")) {
                custom_count++;
            }
        }
        std::cout << "   Found " << custom_count << " custom MIB objects\n";
        
        // Test 3: Check communities
        std::cout << "3. Testing community configuration...\n";
        bool public_read = agent_.is_valid_read_community("public");
        bool private_write = agent_.is_valid_write_community("private");
        std::cout << "   Public read community: " << (public_read ? "OK" : "FAILED") << "\n";
        std::cout << "   Private write community: " << (private_write ? "OK" : "FAILED") << "\n";
        
        // Test 4: Agent status
        std::cout << "4. Testing agent status...\n";
        std::cout << "   Agent running: " << (agent_.is_running() ? "YES" : "NO") << "\n";
        std::cout << "   Local address: " << agent_.local_address().to_string() << "\n";
        std::cout << "   Active connections: " << agent_.active_connections() << "\n";
        
        std::cout << "\nAgent functionality test completed!\n\n";
    }
    
    void handle_received_trap(const SnmpMessage& trap, const SocketAddress& sender) {
        std::cout << "\n[TRAP RECEIVED] From: " << sender.to_string() << "\n";
        std::cout << "Community: " << trap.community() << "\n";
        std::cout << "Version: " << snmp_utils::version_to_string(trap.version()) << "\n";
        
        const auto* pdu = trap.pdu();
        if (pdu) {
            std::cout << "PDU Type: " << snmp_utils::pdu_type_to_string(pdu->type()) << "\n";
            std::cout << "Variables: " << pdu->variable_bindings().size() << "\n";
        }
        std::cout << "-------------------------------\n";
    }
};

int main() {
    try {
        // Initialize logging
        Logger::set_level(LogLevel::INFO);
        
        std::cout << "Starting SNMP Agent Example...\n\n";
        
        SnmpAgentExample agent;
        agent.run();
        
        std::cout << "SNMP Agent Example completed.\n";
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}