#include "networkquests/snmp.hpp"
#include "networkquests/logger.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>

using namespace networkquests;
using namespace networkquests::snmp;

class SnmpClientExample {
public:
    SnmpClientExample() : manager_(std::chrono::milliseconds(5000)) {
        // Set SNMP version to v2c for better features
        manager_.set_version(SnmpVersion::V2C);
        manager_.set_retries(3);
    }
    
    void run() {
        show_banner();
        
        while (true) {
            show_menu();
            
            std::string choice;
            std::cout << "Enter your choice (1-9, q to quit): ";
            std::getline(std::cin, choice);
            
            if (choice == "q" || choice == "Q") {
                break;
            }
            
            try {
                switch (std::stoi(choice)) {
                    case 1: connect_to_agent(); break;
                    case 2: perform_get_operation(); break;
                    case 3: perform_set_operation(); break;
                    case 4: perform_get_next_operation(); break;
                    case 5: perform_get_bulk_operation(); break;
                    case 6: walk_system_table(); break;
                    case 7: send_inform_request(); break;
                    case 8: test_multiple_operations(); break;
                    case 9: show_connection_info(); break;
                    default:
                        std::cout << "Invalid choice. Please try again.\n\n";
                        break;
                }
            } catch (const std::exception& e) {
                std::cout << "Error: " << e.what() << "\n\n";
            }
        }
        
        if (manager_.is_connected()) {
            manager_.disconnect();
            std::cout << "Disconnected from SNMP agent.\n";
        }
    }

private:
    SnmpManager manager_;
    
    void show_banner() {
        std::cout << "========================================\n"
                  << "    NetworkQuests SNMP Client Demo\n"
                  << "========================================\n\n";
    }
    
    void show_menu() {
        std::cout << "SNMP Client Operations:\n"
                  << "1. Connect to SNMP Agent\n"
                  << "2. SNMP GET Operation\n"
                  << "3. SNMP SET Operation\n"
                  << "4. SNMP GETNEXT Operation\n"
                  << "5. SNMP GETBULK Operation\n"
                  << "6. Walk System Table\n"
                  << "7. Send INFORM Request\n"
                  << "8. Test Multiple Operations\n"
                  << "9. Show Connection Info\n"
                  << "q. Quit\n\n";
    }
    
    void connect_to_agent() {
        std::string host;
        std::string port_str;
        
        std::cout << "Enter SNMP agent host [127.0.0.1]: ";
        std::getline(std::cin, host);
        if (host.empty()) {
            host = "127.0.0.1";
        }
        
        std::cout << "Enter SNMP agent port [161]: ";
        std::getline(std::cin, port_str);
        Port port = port_str.empty() ? 161 : static_cast<Port>(std::stoi(port_str));
        
        SocketAddress agent_addr = SocketAddress::from_string(host, port);
        
        std::cout << "Connecting to SNMP agent at " << agent_addr.to_string() << "...\n";
        
        auto result = manager_.connect(agent_addr);
        if (result) {
            std::cout << "Successfully connected to SNMP agent!\n";
            std::cout << "Local address: " << manager_.local_address().to_string() << "\n";
            std::cout << "Remote address: " << manager_.remote_address().to_string() << "\n\n";
        } else {
            std::cout << "Failed to connect: " << result.error() << "\n\n";
        }
    }
    
    void perform_get_operation() {
        if (!check_connection()) return;
        
        std::cout << "SNMP GET Operation\n";
        std::cout << "Example OIDs:\n";
        std::cout << "  1.3.6.1.2.1.1.1.0 - System description\n";
        std::cout << "  1.3.6.1.2.1.1.3.0 - System uptime\n";
        std::cout << "  1.3.6.1.2.1.1.5.0 - System name\n\n";
        
        std::string oid_str;
        std::cout << "Enter OID to get: ";
        std::getline(std::cin, oid_str);
        
        if (oid_str.empty()) {
            oid_str = "1.3.6.1.2.1.1.1.0"; // System description
        }
        
        std::string community;
        std::cout << "Enter community string [public]: ";
        std::getline(std::cin, community);
        if (community.empty()) {
            community = "public";
        }
        
        auto oid_result = ObjectIdentifier::from_string(oid_str);
        if (!oid_result) {
            std::cout << "Invalid OID format: " << oid_result.error() << "\n\n";
            return;
        }
        
        std::cout << "Performing GET request...\n";
        auto result = manager_.get(oid_result.value(), community);
        
        if (result) {
            std::cout << "Success!\n";
            std::cout << "OID: " << oid_str << "\n";
            std::cout << "Value: " << snmp_utils::snmp_value_to_string(result.value(), DataType::OCTET_STRING) << "\n\n";
        } else {
            std::cout << "GET failed: " << result.error() << "\n\n";
        }
    }
    
    void perform_set_operation() {
        if (!check_connection()) return;
        
        std::cout << "SNMP SET Operation\n";
        std::cout << "Example writable OIDs:\n";
        std::cout << "  1.3.6.1.2.1.1.4.0 - System contact\n";
        std::cout << "  1.3.6.1.2.1.1.5.0 - System name\n";
        std::cout << "  1.3.6.1.2.1.1.6.0 - System location\n\n";
        
        std::string oid_str;
        std::cout << "Enter OID to set: ";
        std::getline(std::cin, oid_str);
        
        if (oid_str.empty()) {
            oid_str = "1.3.6.1.2.1.1.4.0"; // System contact
        }
        
        std::string value;
        std::cout << "Enter new value: ";
        std::getline(std::cin, value);
        
        std::string community;
        std::cout << "Enter community string [private]: ";
        std::getline(std::cin, community);
        if (community.empty()) {
            community = "private";
        }
        
        auto oid_result = ObjectIdentifier::from_string(oid_str);
        if (!oid_result) {
            std::cout << "Invalid OID format: " << oid_result.error() << "\n\n";
            return;
        }
        
        std::cout << "Performing SET request...\n";
        auto result = manager_.set(oid_result.value(), SnmpValue(value), DataType::OCTET_STRING, community);
        
        if (result) {
            std::cout << "SET operation successful!\n";
            std::cout << "OID: " << oid_str << "\n";
            std::cout << "New value: " << value << "\n\n";
        } else {
            std::cout << "SET failed: " << result.error() << "\n\n";
        }
    }
    
    void perform_get_next_operation() {
        if (!check_connection()) return;
        
        std::cout << "SNMP GETNEXT Operation\n";
        std::cout << "This operation retrieves the next object in the MIB tree.\n\n";
        
        std::string oid_str;
        std::cout << "Enter starting OID [1.3.6.1.2.1.1]: ";
        std::getline(std::cin, oid_str);
        
        if (oid_str.empty()) {
            oid_str = "1.3.6.1.2.1.1"; // System group
        }
        
        std::string community;
        std::cout << "Enter community string [public]: ";
        std::getline(std::cin, community);
        if (community.empty()) {
            community = "public";
        }
        
        auto oid_result = ObjectIdentifier::from_string(oid_str);
        if (!oid_result) {
            std::cout << "Invalid OID format: " << oid_result.error() << "\n\n";
            return;
        }
        
        std::cout << "Performing GETNEXT request...\n";
        auto result = manager_.get_next(oid_result.value(), community);
        
        if (result) {
            const auto& vb = result.value();
            std::cout << "Success!\n";
            std::cout << "Next OID: " << vb.oid().to_string() << "\n";
            std::cout << "Value: " << snmp_utils::snmp_value_to_string(vb.value(), vb.type()) << "\n";
            std::cout << "Type: " << snmp_utils::data_type_to_string(vb.type()) << "\n\n";
        } else {
            std::cout << "GETNEXT failed: " << result.error() << "\n\n";
        }
    }
    
    void perform_get_bulk_operation() {
        if (!check_connection()) return;
        
        std::cout << "SNMP GETBULK Operation (SNMPv2c only)\n";
        std::cout << "This operation efficiently retrieves multiple objects.\n\n";
        
        std::string oid_str;
        std::cout << "Enter starting OID [1.3.6.1.2.1.1]: ";
        std::getline(std::cin, oid_str);
        
        if (oid_str.empty()) {
            oid_str = "1.3.6.1.2.1.1"; // System group
        }
        
        std::string max_reps_str;
        std::cout << "Enter max repetitions [10]: ";
        std::getline(std::cin, max_reps_str);
        int32_t max_reps = max_reps_str.empty() ? 10 : std::stoi(max_reps_str);
        
        std::string community;
        std::cout << "Enter community string [public]: ";
        std::getline(std::cin, community);
        if (community.empty()) {
            community = "public";
        }
        
        auto oid_result = ObjectIdentifier::from_string(oid_str);
        if (!oid_result) {
            std::cout << "Invalid OID format: " << oid_result.error() << "\n\n";
            return;
        }
        
        std::vector<ObjectIdentifier> oids = {oid_result.value()};
        
        std::cout << "Performing GETBULK request...\n";
        auto result = manager_.get_bulk(oids, 0, max_reps, community);
        
        if (result) {
            const auto& vb_list = result.value();
            std::cout << "Success! Retrieved " << vb_list.size() << " objects:\n\n";
            
            for (size_t i = 0; i < vb_list.size(); ++i) {
                const auto& vb = vb_list[i];
                std::cout << (i + 1) << ". OID: " << vb.oid().to_string() << "\n";
                std::cout << "   Value: " << snmp_utils::snmp_value_to_string(vb.value(), vb.type()) << "\n";
                std::cout << "   Type: " << snmp_utils::data_type_to_string(vb.type()) << "\n\n";
                
                if (i >= 19) { // Limit output
                    std::cout << "   ... and " << (vb_list.size() - i - 1) << " more objects\n\n";
                    break;
                }
            }
        } else {
            std::cout << "GETBULK failed: " << result.error() << "\n\n";
        }
    }
    
    void walk_system_table() {
        if (!check_connection()) return;
        
        std::cout << "Walking System MIB Table\n";
        std::cout << "This demonstrates table traversal using GETNEXT operations.\n\n";
        
        std::string community;
        std::cout << "Enter community string [public]: ";
        std::getline(std::cin, community);
        if (community.empty()) {
            community = "public";
        }
        
        ObjectIdentifier system_oid("1.3.6.1.2.1.1"); // System group
        
        std::cout << "Walking system MIB (1.3.6.1.2.1.1)...\n";
        auto result = manager_.walk_table(system_oid, community);
        
        if (result) {
            const auto& vb_list = result.value();
            std::cout << "Found " << vb_list.size() << " system objects:\n\n";
            
            for (const auto& vb : vb_list) {
                std::cout << "OID: " << vb.oid().to_string() << "\n";
                std::cout << "Value: " << snmp_utils::snmp_value_to_string(vb.value(), vb.type()) << "\n";
                std::cout << "Type: " << snmp_utils::data_type_to_string(vb.type()) << "\n";
                std::cout << "--------------------\n";
            }
            std::cout << "\n";
        } else {
            std::cout << "Table walk failed: " << result.error() << "\n\n";
        }
    }
    
    void send_inform_request() {
        if (!check_connection()) return;
        
        std::cout << "Sending SNMP INFORM Request (SNMPv2c only)\n";
        std::cout << "INFORM requests are acknowledged traps.\n\n";
        
        std::string trap_oid_str;
        std::cout << "Enter trap OID [1.3.6.1.6.3.1.1.5.4]: ";
        std::getline(std::cin, trap_oid_str);
        
        if (trap_oid_str.empty()) {
            trap_oid_str = "1.3.6.1.6.3.1.1.5.4"; // Link up trap
        }
        
        std::string community;
        std::cout << "Enter community string [public]: ";
        std::getline(std::cin, community);
        if (community.empty()) {
            community = "public";
        }
        
        auto oid_result = ObjectIdentifier::from_string(trap_oid_str);
        if (!oid_result) {
            std::cout << "Invalid OID format: " << oid_result.error() << "\n\n";
            return;
        }
        
        std::cout << "Sending INFORM request...\n";
        auto result = manager_.inform(oid_result.value(), std::monostate{}, DataType::NULL_VALUE, community);
        
        if (result) {
            std::cout << "INFORM request sent and acknowledged successfully!\n";
            std::cout << "Trap OID: " << trap_oid_str << "\n\n";
        } else {
            std::cout << "INFORM failed: " << result.error() << "\n\n";
        }
    }
    
    void test_multiple_operations() {
        if (!check_connection()) return;
        
        std::cout << "Testing Multiple SNMP Operations\n";
        std::cout << "This will perform several operations in sequence.\n\n";
        
        std::string community;
        std::cout << "Enter community string [public]: ";
        std::getline(std::cin, community);
        if (community.empty()) {
            community = "public";
        }
        
        // Test multiple GET operations
        std::vector<ObjectIdentifier> test_oids = {
            ObjectIdentifier("1.3.6.1.2.1.1.1.0"), // sysDescr
            ObjectIdentifier("1.3.6.1.2.1.1.3.0"), // sysUpTime
            ObjectIdentifier("1.3.6.1.2.1.1.5.0"), // sysName
            ObjectIdentifier("1.3.6.1.2.1.1.6.0")  // sysLocation
        };
        
        std::cout << "Performing multiple GET operations...\n";
        auto result = manager_.get_multiple(test_oids, community);
        
        if (result) {
            const auto& vb_list = result.value();
            std::cout << "All operations successful!\n\n";
            
            std::vector<std::string> descriptions = {
                "System Description",
                "System Uptime",
                "System Name",
                "System Location"
            };
            
            for (size_t i = 0; i < vb_list.size() && i < descriptions.size(); ++i) {
                const auto& vb = vb_list[i];
                std::cout << descriptions[i] << ":\n";
                std::cout << "  OID: " << vb.oid().to_string() << "\n";
                std::cout << "  Value: " << snmp_utils::snmp_value_to_string(vb.value(), vb.type()) << "\n\n";
            }
        } else {
            std::cout << "Multiple operations failed: " << result.error() << "\n\n";
        }
    }
    
    void show_connection_info() {
        if (!manager_.is_connected()) {
            std::cout << "Not connected to any SNMP agent.\n\n";
            return;
        }
        
        std::cout << "Connection Information:\n";
        std::cout << "Status: Connected\n";
        std::cout << "Local address: " << manager_.local_address().to_string() << "\n";
        std::cout << "Remote address: " << manager_.remote_address().to_string() << "\n";
        std::cout << "SNMP version: " << snmp_utils::version_to_string(SnmpVersion::V2C) << "\n";
        std::cout << "Timeout: 5000ms\n";
        std::cout << "Retries: 3\n\n";
    }
    
    bool check_connection() {
        if (!manager_.is_connected()) {
            std::cout << "Not connected to SNMP agent. Please connect first (option 1).\n\n";
            return false;
        }
        return true;
    }
};

int main() {
    try {
        // Initialize logging
        Logger::set_level(LogLevel::INFO);
        
        std::cout << "Starting SNMP Client Example...\n\n";
        
        SnmpClientExample client;
        client.run();
        
        std::cout << "SNMP Client Example completed.\n";
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}