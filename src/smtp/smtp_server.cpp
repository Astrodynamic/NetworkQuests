#include "networkquests/smtp.hpp"

#include <thread>
#include <sstream>
#include <algorithm>

namespace networkquests::smtp {

SmtpServer::SmtpServer(Port port) : bind_addr_(SocketAddress::any_ipv4(port)) {
    hostname_ = "localhost";
    supported_extensions_.push_back(SmtpExtension::SIZE);
    supported_extensions_.push_back(SmtpExtension::ENHANCEDSTATUSCODES);
}

SmtpServer::SmtpServer(const SocketAddress& bind_addr) : bind_addr_(bind_addr) {
    hostname_ = "localhost";
    supported_extensions_.push_back(SmtpExtension::SIZE);
    supported_extensions_.push_back(SmtpExtension::ENHANCEDSTATUSCODES);
}

Result<void> SmtpServer::start() {
    if (running_.load()) {
        return make_error("Server is already running");
    }
    
    tcp_server_ = std::make_unique<tcp::TcpServer>(bind_addr_);
    auto start_result = tcp_server_->start();
    if (!start_result) {
        return make_error("Failed to start TCP server: " + start_result.error().message);
    }
    
    running_.store(true);
    
    // Start accepting connections in separate thread
    std::thread accept_thread([this]() {
        while (running_.load()) {
            try {
                auto connection_result = tcp_server_->accept(std::chrono::milliseconds(1000));
                if (connection_result) {
                    auto connection = connection_result.value();
                    
                    // Check connection limit
                    if (active_connections_.load() >= max_connections_) {
                        // Send "too many connections" response and close
                        SmtpConnection smtp_conn(std::move(connection));
                        SmtpResponse busy_response(SmtpResponseCode::ServiceNotAvailable, 
                                                 "Too many connections, try again later");
                        smtp_conn.send_response(busy_response);
                        smtp_conn.close();
                        continue;
                    }
                    
                    // Handle client in separate thread
                    std::thread client_thread([this, conn = std::move(connection)]() mutable {
                        handle_client(std::move(conn));
                    });
                    client_thread.detach();
                }
            } catch (const std::exception& e) {
                // Log error and continue
            }
        }
    });
    accept_thread.detach();
    
    return make_success();
}

void SmtpServer::stop() {
    running_.store(false);
    if (tcp_server_) {
        tcp_server_->stop();
    }
}

bool SmtpServer::is_running() const {
    return running_.load();
}

void SmtpServer::add_supported_extension(SmtpExtension extension) {
    auto it = std::find(supported_extensions_.begin(), supported_extensions_.end(), extension);
    if (it == supported_extensions_.end()) {
        supported_extensions_.push_back(extension);
    }
}

SocketAddress SmtpServer::local_address() const {
    if (tcp_server_) {
        return tcp_server_->local_address();
    }
    return bind_addr_;
}

void SmtpServer::handle_client(tcp::TcpConnection connection) {
    active_connections_.fetch_add(1);
    
    try {
        SmtpConnection smtp_connection(std::move(connection));
        process_session(smtp_connection);
    } catch (const std::exception& e) {
        // Log error
    }
    
    active_connections_.fetch_sub(1);
}

void SmtpServer::process_session(SmtpConnection& connection) {
    SessionState state;
    state.last_activity = std::chrono::steady_clock::now();
    
    // Send greeting
    SmtpResponse greeting(SmtpResponseCode::ServiceReady, hostname_ + " SMTP Ready");
    auto greeting_result = connection.send_response(greeting);
    if (!greeting_result) {
        return;
    }
    
    // Main command loop
    while (connection.is_connected()) {
        auto command_result = connection.receive_command(std::chrono::seconds(300)); // 5 min timeout
        if (!command_result) {
            break;
        }
        
        auto command = command_result.value();
        state.last_activity = std::chrono::steady_clock::now();
        
        auto response = handle_command(command, state, connection);
        
        auto response_result = connection.send_response(response);
        if (!response_result) {
            break;
        }
        
        // Check if client sent QUIT
        if (command.command() == SmtpCommand::QUIT) {
            break;
        }
    }
    
    connection.close();
}

SmtpResponse SmtpServer::handle_command(const SmtpCommand_& command, SessionState& state, SmtpConnection& connection) {
    try {
        switch (command.command()) {
            case SmtpCommand::EHLO:
                return handle_ehlo(command, state);
            case SmtpCommand::HELO:
                return handle_helo(command, state);
            case SmtpCommand::MAIL:
                return handle_mail(command, state);
            case SmtpCommand::RCPT:
                return handle_rcpt(command, state);
            case SmtpCommand::DATA:
                return handle_data(command, state, connection);
            case SmtpCommand::AUTH:
                return handle_auth(command, state, connection);
            case SmtpCommand::RSET:
                return handle_rset(command, state);
            case SmtpCommand::NOOP:
                return handle_noop(command, state);
            case SmtpCommand::QUIT:
                return handle_quit(command, state);
            case SmtpCommand::VRFY:
                return handle_vrfy(command, state);
            case SmtpCommand::EXPN:
                return handle_expn(command, state);
            case SmtpCommand::HELP:
                return handle_help(command, state);
            default:
                return SmtpResponse(SmtpResponseCode::CommandNotImplemented, "Command not implemented");
        }
    } catch (const std::exception& e) {
        return SmtpResponse(SmtpResponseCode::LocalError, "Internal server error");
    }
}

SmtpResponse SmtpServer::handle_ehlo(const SmtpCommand_& command, SessionState& state) {
    if (command.arguments().empty()) {
        return SmtpResponse(SmtpResponseCode::ParameterError, "EHLO requires hostname argument");
    }
    
    reset_session_state(state);
    
    std::vector<std::string> messages;
    messages.push_back(hostname_ + " Hello " + command.arguments()[0]);
    
    // Add supported extensions
    for (auto ext : supported_extensions_) {
        std::string ext_str(smtp_utils::extension_to_string(ext));
        
        if (ext == SmtpExtension::SIZE) {
            ext_str += " " + std::to_string(max_message_size_);
        } else if (ext == SmtpExtension::AUTH) {
            ext_str += " PLAIN LOGIN";
        }
        
        messages.push_back(ext_str);
    }
    
    return SmtpResponse(SmtpResponseCode::OK, messages);
}

SmtpResponse SmtpServer::handle_helo(const SmtpCommand_& command, SessionState& state) {
    if (command.arguments().empty()) {
        return SmtpResponse(SmtpResponseCode::ParameterError, "HELO requires hostname argument");
    }
    
    reset_session_state(state);
    
    return SmtpResponse(SmtpResponseCode::OK, hostname_ + " Hello " + command.arguments()[0]);
}

SmtpResponse SmtpServer::handle_mail(const SmtpCommand_& command, SessionState& state) {
    if (require_auth_ && !state.authenticated) {
        return SmtpResponse(SmtpResponseCode::BadSequence, "Authentication required");
    }
    
    if (command.arguments().empty()) {
        return SmtpResponse(SmtpResponseCode::ParameterError, "MAIL FROM requires address");
    }
    
    // Parse FROM argument
    std::string from_arg = command.arguments()[0];
    if (!from_arg.starts_with("FROM:")) {
        return SmtpResponse(SmtpResponseCode::ParameterError, "Invalid MAIL FROM syntax");
    }
    
    std::string address_part = from_arg.substr(5);
    
    // Remove angle brackets if present
    if (address_part.starts_with("<") && address_part.ends_with(">")) {
        address_part = address_part.substr(1, address_part.length() - 2);
    }
    
    // Handle null reverse path for bounce messages
    if (address_part.empty()) {
        state.mail_from = EmailAddress("", "");
    } else {
        if (!is_valid_email_address(address_part)) {
            return SmtpResponse(SmtpResponseCode::InvalidMailbox, "Invalid sender address");
        }
        state.mail_from = EmailAddress(address_part);
    }
    
    // Clear recipients from previous transaction
    state.rcpt_to.clear();
    
    return SmtpResponse(SmtpResponseCode::OK, "Sender ok");
}

SmtpResponse SmtpServer::handle_rcpt(const SmtpCommand_& command, SessionState& state) {
    if (state.mail_from.address().empty() && !state.mail_from.name().empty()) {
        return SmtpResponse(SmtpResponseCode::BadSequence, "Need MAIL command");
    }
    
    if (command.arguments().empty()) {
        return SmtpResponse(SmtpResponseCode::ParameterError, "RCPT TO requires address");
    }
    
    // Parse TO argument
    std::string to_arg = command.arguments()[0];
    if (!to_arg.starts_with("TO:")) {
        return SmtpResponse(SmtpResponseCode::ParameterError, "Invalid RCPT TO syntax");
    }
    
    std::string address_part = to_arg.substr(3);
    
    // Remove angle brackets if present
    if (address_part.starts_with("<") && address_part.ends_with(">")) {
        address_part = address_part.substr(1, address_part.length() - 2);
    }
    
    if (!is_valid_email_address(address_part)) {
        return SmtpResponse(SmtpResponseCode::InvalidMailbox, "Invalid recipient address");
    }
    
    EmailAddress recipient(address_part);
    
    // Validate recipient if validator is set
    if (recipient_validator_ && !recipient_validator_(recipient)) {
        return SmtpResponse(SmtpResponseCode::MailboxUnavailable, "Recipient not accepted");
    }
    
    state.rcpt_to.push_back(recipient);
    
    return SmtpResponse(SmtpResponseCode::OK, "Recipient ok");
}

SmtpResponse SmtpServer::handle_data(const SmtpCommand_& command, SessionState& state, SmtpConnection& connection) {
    if (state.rcpt_to.empty()) {
        return SmtpResponse(SmtpResponseCode::BadSequence, "Need RCPT command");
    }
    
    // Send intermediate response
    SmtpResponse data_response(SmtpResponseCode::StartMailInput, "End data with <CR><LF>.<CR><LF>");
    auto send_result = connection.send_response(data_response);
    if (!send_result) {
        return SmtpResponse(SmtpResponseCode::LocalError, "Failed to send data response");
    }
    
    // Receive message data
    auto data_result = connection.receive_data_block(std::chrono::minutes(10));
    if (!data_result) {
        return SmtpResponse(SmtpResponseCode::LocalError, "Failed to receive message data");
    }
    
    std::string message_data = data_result.value();
    
    // Check message size
    if (message_data.length() > max_message_size_) {
        return SmtpResponse(SmtpResponseCode::ExceededStorage, "Message too large");
    }
    
    // Create message object for handler
    SmtpMessage message;
    message.set_from(state.mail_from);
    for (const auto& recipient : state.rcpt_to) {
        message.add_to(recipient);
    }
    
    // Parse message headers and body (simplified)
    auto header_end = message_data.find("\r\n\r\n");
    if (header_end == std::string::npos) {
        header_end = message_data.find("\n\n");
        if (header_end != std::string::npos) {
            header_end += 2;
        }
    } else {
        header_end += 4;
    }
    
    if (header_end != std::string::npos) {
        std::string headers = message_data.substr(0, header_end);
        std::string body = message_data.substr(header_end);
        message.set_body(body);
        
        // Parse some headers
        std::istringstream header_stream(headers);
        std::string line;
        while (std::getline(header_stream, line)) {
            auto colon_pos = line.find(':');
            if (colon_pos != std::string::npos) {
                std::string name = line.substr(0, colon_pos);
                std::string value = line.substr(colon_pos + 1);
                
                // Trim whitespace
                name.erase(0, name.find_first_not_of(" \t"));
                name.erase(name.find_last_not_of(" \t") + 1);
                value.erase(0, value.find_first_not_of(" \t"));
                value.erase(value.find_last_not_of(" \t") + 1);
                
                if (name == "Subject") {
                    message.set_subject(value);
                } else {
                    message.set_header(name, value);
                }
            }
        }
    } else {
        message.set_body(message_data);
    }
    
    // Call message handler if set
    if (message_handler_) {
        try {
            message_handler_(message, connection.remote_address());
        } catch (const std::exception& e) {
            return SmtpResponse(SmtpResponseCode::LocalError, "Message processing failed");
        }
    }
    
    // Reset session state
    reset_session_state(state);
    
    return SmtpResponse(SmtpResponseCode::OK, "Message accepted for delivery");
}

SmtpResponse SmtpServer::handle_auth(const SmtpCommand_& command, SessionState& state, SmtpConnection& connection) {
    if (!std::count(supported_extensions_.begin(), supported_extensions_.end(), SmtpExtension::AUTH)) {
        return SmtpResponse(SmtpResponseCode::CommandNotImplemented, "AUTH not supported");
    }
    
    if (command.arguments().empty()) {
        return SmtpResponse(SmtpResponseCode::ParameterError, "AUTH requires mechanism");
    }
    
    std::string mechanism = command.arguments()[0];
    std::transform(mechanism.begin(), mechanism.end(), mechanism.begin(), ::toupper);
    
    if (mechanism == "PLAIN") {
        std::string auth_data;
        if (command.arguments().size() > 1) {
            auth_data = command.arguments()[1];
        } else {
            // Send challenge and wait for response
            SmtpResponse challenge(SmtpResponseCode::AuthenticationSuccessful, "");
            auto send_result = connection.send_response(challenge);
            if (!send_result) {
                return SmtpResponse(SmtpResponseCode::LocalError, "Failed to send challenge");
            }
            
            auto data_result = connection.receive_command(std::chrono::seconds(60));
            if (!data_result) {
                return SmtpResponse(SmtpResponseCode::TemporaryAuthFailure, "Authentication timeout");
            }
            
            if (!data_result.value().arguments().empty()) {
                auth_data = data_result.value().arguments()[0];
            }
        }
        
        // Decode base64 auth data
        auto decoded_result = smtp_utils::base64_decode(auth_data);
        if (!decoded_result) {
            return SmtpResponse(SmtpResponseCode::TemporaryAuthFailure, "Invalid authentication data");
        }
        
        std::string decoded = decoded_result.value();
        
        // Parse PLAIN auth: \0username\0password
        size_t first_null = decoded.find('\0');
        size_t second_null = decoded.find('\0', first_null + 1);
        
        if (first_null == std::string::npos || second_null == std::string::npos) {
            return SmtpResponse(SmtpResponseCode::TemporaryAuthFailure, "Invalid authentication format");
        }
        
        std::string username = decoded.substr(first_null + 1, second_null - first_null - 1);
        std::string password = decoded.substr(second_null + 1);
        
        // Validate credentials
        if (auth_validator_ && auth_validator_(username, password)) {
            state.authenticated = true;
            return SmtpResponse(SmtpResponseCode::AuthenticationSuccessful, "Authentication successful");
        } else {
            return SmtpResponse(SmtpResponseCode::TemporaryAuthFailure, "Authentication failed");
        }
    }
    
    return SmtpResponse(SmtpResponseCode::ParameterNotImplemented, "Authentication mechanism not supported");
}

SmtpResponse SmtpServer::handle_rset(const SmtpCommand_& command, SessionState& state) {
    reset_session_state(state);
    return SmtpResponse(SmtpResponseCode::OK, "Reset state");
}

SmtpResponse SmtpServer::handle_noop(const SmtpCommand_& command, SessionState& state) {
    return SmtpResponse(SmtpResponseCode::OK, "Ok");
}

SmtpResponse SmtpServer::handle_quit(const SmtpCommand_& command, SessionState& state) {
    return SmtpResponse(SmtpResponseCode::ServiceClosing, hostname_ + " closing connection");
}

SmtpResponse SmtpServer::handle_vrfy(const SmtpCommand_& command, SessionState& state) {
    if (command.arguments().empty()) {
        return SmtpResponse(SmtpResponseCode::ParameterError, "VRFY requires address");
    }
    
    std::string address = command.arguments()[0];
    if (is_valid_email_address(address)) {
        if (recipient_validator_ && recipient_validator_(EmailAddress(address))) {
            return SmtpResponse(SmtpResponseCode::OK, address);
        } else {
            return SmtpResponse(SmtpResponseCode::MailboxUnavailable, "User unknown");
        }
    } else {
        return SmtpResponse(SmtpResponseCode::InvalidMailbox, "Invalid address");
    }
}

SmtpResponse SmtpServer::handle_expn(const SmtpCommand_& command, SessionState& state) {
    return SmtpResponse(SmtpResponseCode::CommandNotImplemented, "EXPN not implemented");
}

SmtpResponse SmtpServer::handle_help(const SmtpCommand_& command, SessionState& state) {
    std::vector<std::string> help_messages = {
        "Commands supported:",
        "EHLO HELO MAIL RCPT DATA",
        "RSET NOOP QUIT VRFY HELP"
    };
    
    if (std::count(supported_extensions_.begin(), supported_extensions_.end(), SmtpExtension::AUTH)) {
        help_messages.push_back("AUTH");
    }
    
    return SmtpResponse(SmtpResponseCode::HelpMessage, help_messages);
}

void SmtpServer::reset_session_state(SessionState& state) {
    state.mail_from = EmailAddress();
    state.rcpt_to.clear();
    state.message_buffer.clear();
    // Don't reset authenticated state
}

bool SmtpServer::is_valid_email_address(std::string_view address) const {
    return smtp_utils::is_valid_email_address(address);
}

} // namespace networkquests::smtp