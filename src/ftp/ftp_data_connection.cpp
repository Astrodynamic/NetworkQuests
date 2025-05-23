#include "networkquests/ftp.hpp"
#include <fstream>
#include <random>

namespace NetworkQuests::Ftp {

// FtpDataConnection implementation
FtpDataConnection::~FtpDataConnection() {
    close();
}

Result<void> FtpDataConnection::setup_active(const std::string& client_ip, uint16_t client_port) {
    mode_ = FtpConnectionMode::ACTIVE;
    client_ip_ = client_ip;
    client_port_ = client_port;
    
    // Create socket for connecting to client
    socket_ = std::make_unique<TcpSocket>();
    
    return {};
}

Result<uint16_t> FtpDataConnection::setup_passive(const std::string& server_ip) {
    mode_ = FtpConnectionMode::PASSIVE;
    server_ip_ = server_ip.empty() ? Utils::get_local_ip_address() : server_ip;
    
    // Create server socket for listening
    server_ = std::make_unique<TcpServer>();
    
    auto result = server_->start(0); // Use random port
    if (!result) {
        return result.error();
    }
    
    server_port_ = server_->get_port();
    
    return server_port_;
}

Result<void> FtpDataConnection::accept_passive_connection() {
    if (mode_ != FtpConnectionMode::PASSIVE || !server_) {
        return Error("Not in passive mode or server not initialized");
    }
    
    auto connection_result = server_->accept_connection();
    if (!connection_result) {
        return connection_result.error();
    }
    
    socket_ = std::move(*connection_result);
    connected_ = true;
    
    return {};
}

Result<void> FtpDataConnection::connect() {
    if (mode_ == FtpConnectionMode::ACTIVE) {
        if (!socket_) {
            return Error("Socket not initialized for active mode");
        }
        
        auto result = socket_->connect(client_ip_, client_port_);
        if (!result) {
            return result.error();
        }
        
        connected_ = true;
    } else {
        // Passive mode connection is handled by accept_passive_connection
        if (!connected_) {
            return Error("Passive connection not accepted yet");
        }
    }
    
    return {};
}

void FtpDataConnection::close() {
    connected_ = false;
    
    if (socket_) {
        socket_->close();
        socket_.reset();
    }
    
    if (server_) {
        server_->stop();
        server_.reset();
    }
}

Result<void> FtpDataConnection::send_file(const std::filesystem::path& file_path) {
    if (!connected_ || !socket_) {
        return Error("Data connection not established");
    }
    
    try {
        std::ifstream file(file_path, std::ios::binary);
        if (!file.is_open()) {
            return Error("Cannot open file: " + file_path.string());
        }
        
        // Get file size for progress reporting
        file.seekg(0, std::ios::end);
        size_t total_size = file.tellg();
        file.seekg(0, std::ios::beg);
        
        std::vector<uint8_t> buffer(FTP_BUFFER_SIZE);
        size_t transferred = 0;
        
        while (file && transferred < total_size) {
            file.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
            size_t bytes_read = file.gcount();
            
            if (bytes_read == 0) {
                break;
            }
            
            buffer.resize(bytes_read);
            
            // Convert data based on transfer mode
            if (transfer_mode_ == FtpTransferMode::ASCII) {
                auto result = send_ascii_data(buffer);
                if (!result) {
                    return result.error();
                }
            } else {
                auto result = socket_->send(buffer);
                if (!result) {
                    return result.error();
                }
            }
            
            transferred += bytes_read;
            
            // Report progress if callback is set
            if (progress_callback_) {
                progress_callback_(transferred, total_size);
            }
            
            buffer.resize(FTP_BUFFER_SIZE);
        }
        
        return {};
        
    } catch (const std::exception& e) {
        return Error("Error sending file: " + std::string(e.what()));
    }
}

Result<void> FtpDataConnection::receive_file(const std::filesystem::path& file_path) {
    if (!connected_ || !socket_) {
        return Error("Data connection not established");
    }
    
    try {
        // Create directories if they don't exist
        std::filesystem::create_directories(file_path.parent_path());
        
        std::ofstream file(file_path, std::ios::binary);
        if (!file.is_open()) {
            return Error("Cannot create file: " + file_path.string());
        }
        
        std::vector<uint8_t> buffer(FTP_BUFFER_SIZE);
        size_t total_received = 0;
        
        while (true) {
            auto result = socket_->receive(buffer);
            if (!result) {
                if (result.error().message().find("Connection closed") != std::string::npos ||
                    result.error().message().find("EOF") != std::string::npos) {
                    break; // Normal end of transfer
                }
                return result.error();
            }
            
            auto received_data = *result;
            if (received_data.empty()) {
                break; // End of transfer
            }
            
            // Convert data based on transfer mode
            if (transfer_mode_ == FtpTransferMode::ASCII) {
                received_data = convert_from_ascii(received_data);
            }
            
            file.write(reinterpret_cast<const char*>(received_data.data()), received_data.size());
            if (!file) {
                return Error("Error writing to file: " + file_path.string());
            }
            
            total_received += received_data.size();
            
            // Report progress if callback is set
            if (progress_callback_) {
                progress_callback_(total_received, 0); // Unknown total size
            }
        }
        
        return {};
        
    } catch (const std::exception& e) {
        return Error("Error receiving file: " + std::string(e.what()));
    }
}

Result<void> FtpDataConnection::send_data(const std::vector<uint8_t>& data) {
    if (!connected_ || !socket_) {
        return Error("Data connection not established");
    }
    
    if (transfer_mode_ == FtpTransferMode::ASCII) {
        return send_ascii_data(data);
    } else {
        return socket_->send(data);
    }
}

Result<std::vector<uint8_t>> FtpDataConnection::receive_data() {
    if (!connected_ || !socket_) {
        return Error("Data connection not established");
    }
    
    std::vector<uint8_t> all_data;
    std::vector<uint8_t> buffer(FTP_BUFFER_SIZE);
    
    while (true) {
        auto result = socket_->receive(buffer);
        if (!result) {
            if (result.error().message().find("Connection closed") != std::string::npos ||
                result.error().message().find("EOF") != std::string::npos) {
                break; // Normal end of transfer
            }
            return result.error();
        }
        
        auto received_data = *result;
        if (received_data.empty()) {
            break; // End of transfer
        }
        
        all_data.insert(all_data.end(), received_data.begin(), received_data.end());
    }
    
    if (transfer_mode_ == FtpTransferMode::ASCII) {
        all_data = convert_from_ascii(all_data);
    }
    
    return all_data;
}

Result<void> FtpDataConnection::send_directory_listing(const std::string& listing) {
    return send_data(Utils::string_to_bytes(listing));
}

Result<std::string> FtpDataConnection::receive_directory_listing() {
    auto result = receive_data();
    if (!result) {
        return result.error();
    }
    
    return Utils::bytes_to_string(*result);
}

void FtpDataConnection::set_progress_callback(std::function<void(size_t, size_t)> callback) {
    progress_callback_ = callback;
}

Result<void> FtpDataConnection::send_ascii_data(const std::vector<uint8_t>& data) {
    auto converted_data = convert_to_ascii(data);
    return socket_->send(converted_data);
}

Result<std::vector<uint8_t>> FtpDataConnection::receive_ascii_data() {
    auto result = receive_data();
    if (!result) {
        return result.error();
    }
    
    return convert_from_ascii(*result);
}

std::vector<uint8_t> FtpDataConnection::convert_to_ascii(const std::vector<uint8_t>& data) {
    std::vector<uint8_t> converted;
    converted.reserve(data.size() * 2); // Worst case: every byte becomes CRLF
    
    for (uint8_t byte : data) {
        if (byte == '\n') {
            // Convert LF to CRLF
            converted.push_back('\r');
            converted.push_back('\n');
        } else if (byte != '\r') {
            // Pass through all bytes except standalone CR
            converted.push_back(byte);
        } else {
            // Handle CR - only add if not followed by LF
            converted.push_back(byte);
        }
    }
    
    return converted;
}

std::vector<uint8_t> FtpDataConnection::convert_from_ascii(const std::vector<uint8_t>& data) {
    std::vector<uint8_t> converted;
    converted.reserve(data.size());
    
    for (size_t i = 0; i < data.size(); ++i) {
        uint8_t byte = data[i];
        
        if (byte == '\r' && i + 1 < data.size() && data[i + 1] == '\n') {
            // Convert CRLF to LF
            converted.push_back('\n');
            ++i; // Skip the LF
        } else {
            converted.push_back(byte);
        }
    }
    
    return converted;
}

} // namespace NetworkQuests::Ftp