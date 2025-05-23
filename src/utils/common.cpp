#include "networkquests/common.hpp"

#include <iomanip>
#include <sstream>
#include <chrono>
#include <regex>
#include <algorithm>

namespace networkquests::utils {

std::string current_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    
    return ss.str();
}

std::string format_bytes(std::size_t bytes) {
    static constexpr std::array<const char*, 7> units = {
        "B", "KB", "MB", "GB", "TB", "PB", "EB"
    };
    
    if (bytes == 0) {
        return "0 B";
    }
    
    int unit_index = 0;
    double size = static_cast<double>(bytes);
    
    while (size >= 1024.0 && unit_index < static_cast<int>(units.size()) - 1) {
        size /= 1024.0;
        ++unit_index;
    }
    
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << size << " " << units[unit_index];
    return ss.str();
}

bool is_valid_port(Port port) {
    // Port 0 is valid (system assigns available port)
    // Ports 1-65535 are valid
    return port <= 65535;
}

bool is_valid_ipv4(std::string_view ip) {
    // Regular expression for IPv4 validation
    static const std::regex ipv4_pattern(
        R"(^(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$)"
    );
    
    std::string ip_str{ip};
    return std::regex_match(ip_str, ipv4_pattern);
}

} // namespace networkquests::utils