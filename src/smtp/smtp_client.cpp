#include "networkquests/smtp.hpp"

#include <sstream>
#include <algorithm>
#include <regex>

namespace networkquests::smtp {

// SmtpConnection implementation

SmtpConnection::SmtpConnection(tcp::TcpConnection tcp_conn) 
    : tcp_conn_(std::move(tcp_conn)) {}

SocketAddress SmtpConnection::local_address() const {
    return tcp_conn_.local_address();
}

SocketAddress SmtpConnection::remote_address() const {
    return tcp_conn_.remote_address();
}

bool SmtpConnection::is_connected() const {
    return tcp_conn_.is_connected();
}

Result<void> SmtpConnection::send_command(const SmtpCommand_& command) {
    std::string cmd_str = command.to_string();
    return tcp_conn_.send(cmd_str);
}

Result<SmtpResponse> SmtpConnection::receive_response(std::chrono::milliseconds timeout) {
    std::string response_data;
    std::string line;
    
    while (true) {
        auto line_result = tcp_conn_.receive_line(timeout);
        if (!line_result) {
            return make_error("Failed to receive response: " + line_result.error().message);
        }
        
        line = line_result.value();
        response_data += line + "\n";
        
        // Check if this is the last line (space after code) or continuation (hyphen after code)
        if (line.length() >= 4 && line[3] == ' ') {
            break; // Last line
        }
    }
    
    return SmtpResponse::from_string(response_data);
}

Result<void> SmtpConnection::send_response(const SmtpResponse& response) {
    std::string resp_str = response.to_string();
    return tcp_conn_.send(resp_str);
}

Result<SmtpCommand_> SmtpConnection::receive_command(std::chrono::milliseconds timeout) {
    auto line_result = tcp_conn_.receive_line(timeout);
    if (!line_result) {
        return make_error("Failed to receive command: " + line_result.error().message);
    }
    
    return SmtpCommand_::from_string(line_result.value());
}

Result<void> SmtpConnection::send_data(std::string_view data) {
    return tcp_conn_.send(data);
}

Result<std::string> SmtpConnection::receive_data_block(std::chrono::milliseconds timeout) {
    std::string data;
    std::string line;
    
    while (true) {
        auto line_result = tcp_conn_.receive_line(timeout);
        if (!line_result) {
            return make_error("Failed to receive data: " + line_result.error().message);
        }
        
        line = line_result.value();
        
        // Check for end of data marker
        if (line == ".\r\n" || line == ".") {
            break;
        }
        
        // Remove dot-stuffing if present
        if (line.starts_with("..")) {
            line = line.substr(1);
        }
        
        data += line + "\n";
    }
    
    return data;
}

void SmtpConnection::close() {
    tcp_conn_.close();
}

// SmtpClient implementation

SmtpClient::SmtpClient() : timeout_(std::chrono::seconds(30)) {
    char hostname_buf[256];
    if (gethostname(hostname_buf, sizeof(hostname_buf)) == 0) {
        client_hostname_ = hostname_buf;
    } else {
        client_hostname_ = "localhost";
    }
}

SmtpClient::SmtpClient(std::chrono::milliseconds timeout) : timeout_(timeout) {
    char hostname_buf[256];
    if (gethostname(hostname_buf, sizeof(hostname_buf)) == 0) {
        client_hostname_ = hostname_buf;
    } else {
        client_hostname_ = "localhost";
    }
}

Result<void> SmtpClient::connect(std::string_view hostname, Port port) {
    try {
        tcp::TcpClient tcp_client;
        auto tcp_result = tcp_client.connect(hostname, port, timeout_);
        if (!tcp_result) {
            return make_error("Failed to connect to SMTP server: " + tcp_result.error().message);
        }
        
        connection_ = std::make_unique<SmtpConnection>(std::move(tcp_result.value()));
        
        // Receive greeting
        auto greeting_result = connection_->receive_response(timeout_);
        if (!greeting_result) {
            connection_.reset();
            return make_error("Failed to receive greeting: " + greeting_result.error().message);
        }
        
        auto greeting = greeting_result.value();
        if (!greeting.is_success()) {
            connection_.reset();
            return make_error("SMTP server returned error: " + greeting.to_string());
        }
        
        // Perform handshake
        auto handshake_result = perform_handshake();
        if (!handshake_result) {
            connection_.reset();
            return handshake_result;
        }
        
        return make_success();
        
    } catch (const std::exception& e) {
        return make_error("Connection failed: " + std::string(e.what()));
    }
}

Result<void> SmtpClient::connect_secure(std::string_view hostname, Port port) {
    // For now, implement as regular connection (TLS would be added in real implementation)
    return connect(hostname, port);
}

void SmtpClient::disconnect() {
    if (connection_ && connection_->is_connected()) {
        // Send QUIT command
        SmtpCommand_ quit_cmd(SmtpCommand::QUIT);
        connection_->send_command(quit_cmd);
        connection_->receive_response(timeout_);
        
        connection_->close();
    }
    connection_.reset();
}

bool SmtpClient::is_connected() const {
    return connection_ && connection_->is_connected();
}

Result<void> SmtpClient::authenticate(std::string_view username, std::string_view password, 
                                     SmtpAuthMethod method) {
    if (!connection_) {
        return make_error("Not connected to SMTP server");
    }
    
    // Check if server supports AUTH
    if (!supports_extension(SmtpExtension::AUTH)) {
        return make_error("Server does not support authentication");
    }
    
    switch (method) {
        case SmtpAuthMethod::PLAIN: {
            std::string auth_string = encode_auth_plain(username, password);
            SmtpCommand_ auth_cmd(SmtpCommand::AUTH, {"PLAIN", auth_string});
            
            auto send_result = connection_->send_command(auth_cmd);
            if (!send_result) {
                return send_result;
            }
            
            auto response_result = connection_->receive_response(timeout_);
            if (!response_result) {
                return make_error("Failed to receive auth response: " + response_result.error().message);
            }
            
            auto response = response_result.value();
            if (!response.is_success()) {
                return make_error("Authentication failed: " + response.to_string());
            }
            
            connection_->set_authenticated(true);
            return make_success();
        }
        
        case SmtpAuthMethod::LOGIN: {
            // AUTH LOGIN method
            SmtpCommand_ auth_cmd(SmtpCommand::AUTH, {"LOGIN"});
            
            auto send_result = connection_->send_command(auth_cmd);
            if (!send_result) {
                return send_result;
            }
            
            auto response_result = connection_->receive_response(timeout_);
            if (!response_result) {
                return make_error("Failed to receive auth response: " + response_result.error().message);
            }
            
            auto response = response_result.value();
            if (!response.is_intermediate()) {
                return make_error("Unexpected auth response: " + response.to_string());
            }
            
            // Send username
            std::string encoded_username = encode_auth_login(username);
            auto username_result = connection_->send_data(encoded_username + "\r\n");
            if (!username_result) {
                return username_result;
            }
            
            auto username_resp = connection_->receive_response(timeout_);
            if (!username_resp || !username_resp.value().is_intermediate()) {
                return make_error("Username authentication failed");
            }
            
            // Send password
            std::string encoded_password = encode_auth_login(password);
            auto password_result = connection_->send_data(encoded_password + "\r\n");
            if (!password_result) {
                return password_result;
            }
            
            auto password_resp = connection_->receive_response(timeout_);
            if (!password_resp || !password_resp.value().is_success()) {
                return make_error("Password authentication failed");
            }
            
            connection_->set_authenticated(true);
            return make_success();
        }
        
        default:
            return make_error("Authentication method not supported");
    }
}

Result<void> SmtpClient::start_tls() {
    if (!connection_) {
        return make_error("Not connected to SMTP server");
    }
    
    if (!supports_extension(SmtpExtension::STARTTLS)) {
        return make_error("Server does not support STARTTLS");
    }
    
    SmtpCommand_ starttls_cmd(SmtpCommand::STARTTLS);
    auto send_result = connection_->send_command(starttls_cmd);
    if (!send_result) {
        return send_result;
    }
    
    auto response_result = connection_->receive_response(timeout_);
    if (!response_result) {
        return make_error("Failed to receive STARTTLS response: " + response_result.error().message);
    }
    
    auto response = response_result.value();
    if (!response.is_success()) {
        return make_error("STARTTLS failed: " + response.to_string());
    }
    
    // In a real implementation, TLS handshake would be performed here
    connection_->set_secure(true);
    
    // After TLS, need to send EHLO again
    return send_ehlo();
}

Result<void> SmtpClient::send_message(const SmtpMessage& message) {
    if (!connection_) {
        return make_error("Not connected to SMTP server");
    }
    
    if (!message.is_valid()) {
        return make_error("Invalid message");
    }
    
    return send_mail_transaction(message);
}

Result<void> SmtpClient::send_raw_message(const EmailAddress& from, 
                                         const std::vector<EmailAddress>& to,
                                         std::string_view message_data) {
    if (!connection_) {
        return make_error("Not connected to SMTP server");
    }
    
    // Create temporary message for validation
    SmtpMessage temp_msg;
    temp_msg.set_from(from);
    for (const auto& addr : to) {
        temp_msg.add_to(addr);
    }
    
    if (!temp_msg.is_valid()) {
        return make_error("Invalid message parameters");
    }
    
    // MAIL FROM
    SmtpCommand_ mail_cmd(SmtpCommand::MAIL, {"FROM:<" + from.to_address_only() + ">"});
    auto mail_result = connection_->send_command(mail_cmd);
    if (!mail_result) {
        return mail_result;
    }
    
    auto mail_response = connection_->receive_response(timeout_);
    if (!mail_response || !mail_response.value().is_success()) {
        return make_error("MAIL FROM failed");
    }
    
    // RCPT TO
    for (const auto& addr : to) {
        SmtpCommand_ rcpt_cmd(SmtpCommand::RCPT, {"TO:<" + addr.to_address_only() + ">"});
        auto rcpt_result = connection_->send_command(rcpt_cmd);
        if (!rcpt_result) {
            return rcpt_result;
        }
        
        auto rcpt_response = connection_->receive_response(timeout_);
        if (!rcpt_response || !rcpt_response.value().is_success()) {
            return make_error("RCPT TO failed for: " + addr.to_address_only());
        }
    }
    
    // DATA
    SmtpCommand_ data_cmd(SmtpCommand::DATA);
    auto data_result = connection_->send_command(data_cmd);
    if (!data_result) {
        return data_result;
    }
    
    auto data_response = connection_->receive_response(timeout_);
    if (!data_response || !data_response.value().is_intermediate()) {
        return make_error("DATA command failed");
    }
    
    // Send message data with dot-stuffing
    std::string data_to_send;
    std::istringstream iss(std::string(message_data));
    std::string line;
    
    while (std::getline(iss, line)) {
        if (line.starts_with(".")) {
            data_to_send += ".";  // Dot-stuffing
        }
        data_to_send += line + "\r\n";
    }
    
    data_to_send += ".\r\n";  // End of data marker
    
    auto send_data_result = connection_->send_data(data_to_send);
    if (!send_data_result) {
        return send_data_result;
    }
    
    auto final_response = connection_->receive_response(timeout_);
    if (!final_response || !final_response.value().is_success()) {
        return make_error("Message sending failed");
    }
    
    return make_success();
}

Result<std::vector<SmtpExtension>> SmtpClient::get_extensions() {
    return server_extensions_;
}

bool SmtpClient::supports_extension(SmtpExtension extension) const {
    return std::find(server_extensions_.begin(), server_extensions_.end(), extension) 
           != server_extensions_.end();
}

Result<void> SmtpClient::perform_handshake() {
    return send_ehlo();
}

Result<void> SmtpClient::send_ehlo() {
    SmtpCommand_ ehlo_cmd(SmtpCommand::EHLO, client_hostname_);
    
    auto send_result = connection_->send_command(ehlo_cmd);
    if (!send_result) {
        return send_result;
    }
    
    auto response_result = connection_->receive_response(timeout_);
    if (!response_result) {
        return make_error("Failed to receive EHLO response: " + response_result.error().message);
    }
    
    auto response = response_result.value();
    if (!response.is_success()) {
        // Try HELO if EHLO fails
        SmtpCommand_ helo_cmd(SmtpCommand::HELO, client_hostname_);
        auto helo_result = connection_->send_command(helo_cmd);
        if (!helo_result) {
            return helo_result;
        }
        
        auto helo_response = connection_->receive_response(timeout_);
        if (!helo_response || !helo_response.value().is_success()) {
            return make_error("Both EHLO and HELO failed");
        }
        
        return make_success();
    }
    
    // Parse extensions from EHLO response
    server_extensions_.clear();
    for (const auto& message : response.messages()) {
        if (message.find("SIZE") != std::string::npos) {
            server_extensions_.push_back(SmtpExtension::SIZE);
        }
        if (message.find("PIPELINING") != std::string::npos) {
            server_extensions_.push_back(SmtpExtension::PIPELINING);
        }
        if (message.find("STARTTLS") != std::string::npos) {
            server_extensions_.push_back(SmtpExtension::STARTTLS);
        }
        if (message.find("AUTH") != std::string::npos) {
            server_extensions_.push_back(SmtpExtension::AUTH);
        }
        if (message.find("DSN") != std::string::npos) {
            server_extensions_.push_back(SmtpExtension::DELIVERYSTATUS);
        }
        if (message.find("ENHANCEDSTATUSCODES") != std::string::npos) {
            server_extensions_.push_back(SmtpExtension::ENHANCEDSTATUSCODES);
        }
        if (message.find("BINARYMIME") != std::string::npos) {
            server_extensions_.push_back(SmtpExtension::BINARYMIME);
        }
        if (message.find("CHUNKING") != std::string::npos) {
            server_extensions_.push_back(SmtpExtension::CHUNKING);
        }
    }
    
    return make_success();
}

Result<void> SmtpClient::send_mail_transaction(const SmtpMessage& message) {
    // Get all recipients
    std::vector<EmailAddress> all_recipients;
    all_recipients.insert(all_recipients.end(), message.to().begin(), message.to().end());
    all_recipients.insert(all_recipients.end(), message.cc().begin(), message.cc().end());
    all_recipients.insert(all_recipients.end(), message.bcc().begin(), message.bcc().end());
    
    if (all_recipients.empty()) {
        return make_error("No recipients specified");
    }
    
    std::string mime_message = message.to_mime_string();
    
    return send_raw_message(message.from(), all_recipients, mime_message);
}

std::string SmtpClient::encode_auth_plain(std::string_view username, std::string_view password) const {
    std::string auth_string = "\0";
    auth_string += username;
    auth_string += "\0";
    auth_string += password;
    
    return smtp_utils::base64_encode(auth_string);
}

std::string SmtpClient::encode_auth_login(std::string_view data) const {
    return smtp_utils::base64_encode(data);
}

} // namespace networkquests::smtp