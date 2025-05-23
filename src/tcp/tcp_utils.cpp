#include "networkquests/tcp.hpp"

#include <fstream>
#include <filesystem>

namespace networkquests::tcp::utils {

Result<void> send_file(TcpConnection& connection, std::string_view file_path) {
    std::ifstream file(std::string{file_path}, std::ios::binary);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open file for reading: {}", file_path);
        return make_error_code(NetworkError::ProtocolError);
    }
    
    // Get file size
    file.seekg(0, std::ios::end);
    auto file_size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    LOG_INFO("Sending file {} ({} bytes)", file_path, file_size);
    
    // Send file in chunks
    constexpr std::size_t CHUNK_SIZE = 8192;
    std::vector<char> buffer(CHUNK_SIZE);
    std::size_t total_sent = 0;
    
    while (file.good() && total_sent < static_cast<std::size_t>(file_size)) {
        file.read(buffer.data(), CHUNK_SIZE);
        auto bytes_read = file.gcount();
        
        if (bytes_read > 0) {
            auto bytes_span = std::span<const std::byte>(
                reinterpret_cast<const std::byte*>(buffer.data()), 
                static_cast<std::size_t>(bytes_read)
            );
            
            auto send_result = connection.send(bytes_span);
            if (!send_result) {
                LOG_ERROR("Failed to send file chunk: {}", send_result.error().message());
                return send_result.error();
            }
            
            total_sent += send_result.value();
        }
    }
    
    LOG_INFO("Successfully sent file {} ({} bytes)", file_path, total_sent);
    return Result<void>{};
}

Result<void> receive_file(TcpConnection& connection, std::string_view file_path) {
    // Create directories if they don't exist
    std::filesystem::path path{file_path};
    auto parent_path = path.parent_path();
    if (!parent_path.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(parent_path, ec);
        if (ec) {
            LOG_ERROR("Failed to create directories for {}: {}", file_path, ec.message());
            return make_error_code(NetworkError::ProtocolError);
        }
    }
    
    std::ofstream file(std::string{file_path}, std::ios::binary);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open file for writing: {}", file_path);
        return make_error_code(NetworkError::ProtocolError);
    }
    
    LOG_INFO("Receiving file to {}", file_path);
    
    // Receive file in chunks
    constexpr std::size_t CHUNK_SIZE = 8192;
    std::array<std::byte, CHUNK_SIZE> buffer;
    std::size_t total_received = 0;
    
    while (connection.is_connected()) {
        auto receive_result = connection.receive(std::span<std::byte>(buffer));
        if (!receive_result) {
            if (receive_result.error() == make_error_code(NetworkError::ConnectionFailed)) {
                // Connection closed by peer, this might be normal end of file
                break;
            }
            LOG_ERROR("Failed to receive file chunk: {}", receive_result.error().message());
            return receive_result.error();
        }
        
        auto bytes_received = receive_result.value();
        if (bytes_received == 0) {
            // Connection closed by peer
            break;
        }
        
        file.write(reinterpret_cast<const char*>(buffer.data()), 
                   static_cast<std::streamsize>(bytes_received));
        if (!file.good()) {
            LOG_ERROR("Failed to write to file {}", file_path);
            return make_error_code(NetworkError::ProtocolError);
        }
        
        total_received += bytes_received;
    }
    
    LOG_INFO("Successfully received file {} ({} bytes)", file_path, total_received);
    return Result<void>{};
}

Result<std::string> download_string(std::string_view host, Port port, std::string_view request) {
    TcpClient client;
    return client.send_request(host, port, request);
}

Result<void> upload_string(std::string_view host, Port port, std::string_view data) {
    TcpClient client;
    
    auto connection_result = client.connect(host, port);
    if (!connection_result) {
        return connection_result.error();
    }
    
    auto connection = std::move(connection_result.value());
    
    Message message(data);
    auto send_result = connection.send_message(message);
    if (!send_result) {
        LOG_ERROR("Failed to upload string: {}", send_result.error().message());
        return send_result.error();
    }
    
    LOG_INFO("Successfully uploaded {} bytes to {}:{}", data.size(), host, port);
    return Result<void>{};
}

} // namespace networkquests::tcp::utils