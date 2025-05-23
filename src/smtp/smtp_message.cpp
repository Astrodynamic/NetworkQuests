#include "networkquests/smtp.hpp"

#include <sstream>
#include <algorithm>
#include <regex>
#include <fstream>
#include <iomanip>

namespace networkquests::smtp {

// EmailAddress implementation

EmailAddress::EmailAddress(std::string_view address) : address_(address) {
    // Extract name if in "Name <email@domain>" format
    std::regex name_email_regex(R"(^(.+)\s*<([^>]+)>$)");
    std::smatch match;
    std::string addr_str(address);
    
    if (std::regex_match(addr_str, match, name_email_regex)) {
        name_ = match[1].str();
        address_ = match[2].str();
        
        // Trim whitespace and quotes from name
        name_.erase(0, name_.find_first_not_of(" \t\""));
        name_.erase(name_.find_last_not_of(" \t\"") + 1);
    }
}

EmailAddress::EmailAddress(std::string_view name, std::string_view address) 
    : name_(name), address_(address) {}

bool EmailAddress::is_valid() const {
    return smtp_utils::is_valid_email_address(address_);
}

std::string EmailAddress::to_string() const {
    if (name_.empty()) {
        return address_;
    }
    
    // Check if name needs quoting
    bool needs_quotes = name_.find(',') != std::string::npos ||
                       name_.find(';') != std::string::npos ||
                       name_.find('"') != std::string::npos;
    
    if (needs_quotes) {
        std::string quoted_name = "\"";
        for (char c : name_) {
            if (c == '"') {
                quoted_name += "\\\"";
            } else {
                quoted_name += c;
            }
        }
        quoted_name += "\"";
        return quoted_name + " <" + address_ + ">";
    }
    
    return name_ + " <" + address_ + ">";
}

std::string EmailAddress::to_address_only() const {
    return address_;
}

Result<EmailAddress> EmailAddress::from_string(std::string_view str) {
    EmailAddress result(str);
    if (!result.is_valid()) {
        return make_error("Invalid email address format: " + std::string(str));
    }
    return result;
}

bool EmailAddress::operator==(const EmailAddress& other) const {
    return address_ == other.address_;
}

bool EmailAddress::operator!=(const EmailAddress& other) const {
    return !(*this == other);
}

// SmtpCommand_ implementation

SmtpCommand_::SmtpCommand_(SmtpCommand command) : command_(command) {}

SmtpCommand_::SmtpCommand_(SmtpCommand command, std::string_view argument) : command_(command) {
    arguments_.emplace_back(argument);
}

SmtpCommand_::SmtpCommand_(SmtpCommand command, const std::vector<std::string>& arguments)
    : command_(command), arguments_(arguments) {}

void SmtpCommand_::add_argument(std::string_view argument) {
    arguments_.emplace_back(argument);
}

void SmtpCommand_::set_arguments(const std::vector<std::string>& arguments) {
    arguments_ = arguments;
}

std::string SmtpCommand_::to_string() const {
    std::string result(smtp_utils::command_to_string(command_));
    
    for (const auto& arg : arguments_) {
        result += " " + arg;
    }
    
    result += "\r\n";
    return result;
}

Result<SmtpCommand_> SmtpCommand_::from_string(std::string_view data) {
    // Remove trailing CRLF
    std::string line(data);
    if (line.ends_with("\r\n")) {
        line = line.substr(0, line.length() - 2);
    } else if (line.ends_with("\n")) {
        line = line.substr(0, line.length() - 1);
    }
    
    // Parse command and arguments
    std::istringstream iss(line);
    std::string cmd_str;
    if (!std::getline(iss, cmd_str, ' ')) {
        return make_error("Invalid command format");
    }
    
    auto command_result = smtp_utils::string_to_command(cmd_str);
    if (!command_result) {
        return command_result.error();
    }
    
    SmtpCommand_ result(command_result.value());
    
    // Parse arguments
    std::string arg;
    while (std::getline(iss, arg, ' ')) {
        if (!arg.empty()) {
            result.add_argument(arg);
        }
    }
    
    return result;
}

// SmtpResponse implementation

SmtpResponse::SmtpResponse(SmtpResponseCode code) : code_(code) {}

SmtpResponse::SmtpResponse(SmtpResponseCode code, std::string_view message) : code_(code) {
    messages_.emplace_back(message);
}

SmtpResponse::SmtpResponse(SmtpResponseCode code, const std::vector<std::string>& messages)
    : code_(code), messages_(messages) {}

bool SmtpResponse::is_success() const {
    int code = static_cast<int>(code_);
    return code >= 200 && code < 300;
}

bool SmtpResponse::is_intermediate() const {
    int code = static_cast<int>(code_);
    return code >= 300 && code < 400;
}

bool SmtpResponse::is_temporary_failure() const {
    int code = static_cast<int>(code_);
    return code >= 400 && code < 500;
}

bool SmtpResponse::is_permanent_failure() const {
    int code = static_cast<int>(code_);
    return code >= 500 && code < 600;
}

void SmtpResponse::add_message(std::string_view message) {
    messages_.emplace_back(message);
}

void SmtpResponse::set_messages(const std::vector<std::string>& messages) {
    messages_ = messages;
}

std::string SmtpResponse::to_string() const {
    std::string result;
    std::string code_str = smtp_utils::response_code_to_string(code_);
    
    if (messages_.empty()) {
        result = code_str + " " + std::string(smtp_utils::response_description(code_)) + "\r\n";
    } else if (messages_.size() == 1) {
        result = code_str + " " + messages_[0] + "\r\n";
    } else {
        // Multi-line response
        for (size_t i = 0; i < messages_.size(); ++i) {
            result += code_str;
            if (i < messages_.size() - 1) {
                result += "-";  // Continuation
            } else {
                result += " ";  // Final line
            }
            result += messages_[i] + "\r\n";
        }
    }
    
    return result;
}

Result<SmtpResponse> SmtpResponse::from_string(std::string_view data) {
    std::istringstream iss(std::string(data));
    std::string line;
    std::vector<std::string> lines;
    
    while (std::getline(iss, line)) {
        // Remove trailing CR if present
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (!line.empty()) {
            lines.push_back(line);
        }
    }
    
    if (lines.empty()) {
        return make_error("Empty response");
    }
    
    // Parse first line to get response code
    const std::string& first_line = lines[0];
    if (first_line.length() < 3) {
        return make_error("Invalid response format");
    }
    
    std::string code_str = first_line.substr(0, 3);
    try {
        int code_int = std::stoi(code_str);
        SmtpResponseCode code = static_cast<SmtpResponseCode>(code_int);
        
        SmtpResponse result(code);
        
        // Parse messages
        for (const auto& line : lines) {
            if (line.length() > 4) {
                std::string message = line.substr(4);  // Skip "XXX " or "XXX-"
                result.add_message(message);
            }
        }
        
        return result;
        
    } catch (const std::exception&) {
        return make_error("Invalid response code: " + code_str);
    }
}

// SmtpMessage implementation

void SmtpMessage::set_header(std::string_view name, std::string_view value) {
    headers_[std::string(name)] = std::string(value);
}

std::optional<std::string> SmtpMessage::get_header(std::string_view name) const {
    auto it = headers_.find(std::string(name));
    if (it != headers_.end()) {
        return it->second;
    }
    return std::nullopt;
}

void SmtpMessage::remove_header(std::string_view name) {
    headers_.erase(std::string(name));
}

void SmtpMessage::add_attachment(const Attachment& attachment) {
    attachments_.push_back(attachment);
}

void SmtpMessage::add_attachment_from_file(std::string_view filename, std::string_view content_type) {
    std::ifstream file(std::string(filename), std::ios::binary);
    if (!file) {
        return; // Could throw or return error in production code
    }
    
    Attachment attachment;
    attachment.filename = std::string(filename);
    
    // Extract just the filename without path
    auto slash_pos = attachment.filename.find_last_of("/\\");
    if (slash_pos != std::string::npos) {
        attachment.filename = attachment.filename.substr(slash_pos + 1);
    }
    
    if (content_type.empty()) {
        attachment.content_type = smtp_utils::get_mime_type_for_file(attachment.filename);
    } else {
        attachment.content_type = std::string(content_type);
    }
    
    // Read file data
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    attachment.data.resize(size);
    file.read(reinterpret_cast<char*>(attachment.data.data()), size);
    
    attachments_.push_back(attachment);
}

std::string SmtpMessage::to_mime_string() const {
    std::ostringstream oss;
    
    // Generate Message-ID if not present
    std::string message_id = get_message_id();
    
    // Standard headers
    oss << "From: " << from_.to_string() << "\r\n";
    
    if (!to_.empty()) {
        oss << "To: ";
        for (size_t i = 0; i < to_.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << to_[i].to_string();
        }
        oss << "\r\n";
    }
    
    if (!cc_.empty()) {
        oss << "Cc: ";
        for (size_t i = 0; i < cc_.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << cc_[i].to_string();
        }
        oss << "\r\n";
    }
    
    if (!subject_.empty()) {
        oss << "Subject: " << encode_header(subject_) << "\r\n";
    }
    
    oss << "Date: " << smtp_utils::format_email_date() << "\r\n";
    oss << "Message-ID: <" << message_id << ">\r\n";
    oss << "MIME-Version: 1.0\r\n";
    
    // Priority header
    if (priority_ != EmailPriority::Normal) {
        oss << "X-Priority: " << static_cast<int>(priority_) << "\r\n";
    }
    
    // Custom headers
    for (const auto& [name, value] : headers_) {
        oss << name << ": " << value << "\r\n";
    }
    
    if (attachments_.empty()) {
        // Simple message
        oss << "Content-Type: text/plain; charset=UTF-8\r\n";
        oss << "Content-Transfer-Encoding: 8bit\r\n";
        oss << "\r\n";
        oss << body_;
    } else {
        // Multipart message
        std::string boundary = generate_boundary();
        oss << "Content-Type: multipart/mixed; boundary=\"" << boundary << "\"\r\n";
        oss << "\r\n";
        oss << "This is a multi-part message in MIME format.\r\n";
        oss << "\r\n";
        
        // Message body part
        oss << "--" << boundary << "\r\n";
        oss << "Content-Type: text/plain; charset=UTF-8\r\n";
        oss << "Content-Transfer-Encoding: 8bit\r\n";
        oss << "\r\n";
        oss << body_ << "\r\n";
        
        // Attachment parts
        for (const auto& attachment : attachments_) {
            oss << "--" << boundary << "\r\n";
            oss << "Content-Type: " << attachment.content_type << "\r\n";
            
            if (attachment.inline_attachment) {
                oss << "Content-Disposition: inline; filename=\"" << attachment.filename << "\"\r\n";
            } else {
                oss << "Content-Disposition: attachment; filename=\"" << attachment.filename << "\"\r\n";
            }
            
            oss << "Content-Transfer-Encoding: base64\r\n";
            oss << "\r\n";
            
            // Encode attachment data as base64
            std::string encoded_data = smtp_utils::base64_encode(
                std::string_view(reinterpret_cast<const char*>(attachment.data.data()), attachment.data.size())
            );
            
            // Break into lines of 76 characters
            for (size_t i = 0; i < encoded_data.length(); i += 76) {
                oss << encoded_data.substr(i, 76) << "\r\n";
            }
        }
        
        oss << "--" << boundary << "--\r\n";
    }
    
    return oss.str();
}

std::string SmtpMessage::get_message_id() const {
    auto existing_id = get_header("Message-ID");
    if (existing_id) {
        return existing_id.value();
    }
    
    std::string domain = "localhost";
    if (!from_.address().empty()) {
        domain = smtp_utils::extract_domain(from_.address());
    }
    
    return smtp_utils::generate_message_id(domain);
}

bool SmtpMessage::is_valid() const {
    if (!from_.is_valid()) {
        return false;
    }
    
    if (to_.empty() && cc_.empty() && bcc_.empty()) {
        return false;
    }
    
    for (const auto& addr : to_) {
        if (!addr.is_valid()) return false;
    }
    for (const auto& addr : cc_) {
        if (!addr.is_valid()) return false;
    }
    for (const auto& addr : bcc_) {
        if (!addr.is_valid()) return false;
    }
    
    return true;
}

void SmtpMessage::clear() {
    from_ = EmailAddress();
    to_.clear();
    cc_.clear();
    bcc_.clear();
    subject_.clear();
    body_.clear();
    priority_ = EmailPriority::Normal;
    headers_.clear();
    attachments_.clear();
}

size_t SmtpMessage::estimated_size() const {
    size_t size = subject_.length() + body_.length() + 1000; // Headers overhead
    
    for (const auto& attachment : attachments_) {
        size += attachment.data.size() * 4 / 3; // Base64 encoding overhead
        size += 500; // MIME headers overhead
    }
    
    return size;
}

std::string SmtpMessage::generate_boundary() const {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 61);
    
    const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    
    std::string boundary = "----=_Part_";
    for (int i = 0; i < 16; ++i) {
        boundary += chars[dis(gen)];
    }
    
    return boundary;
}

std::string SmtpMessage::encode_header(std::string_view header) const {
    return smtp_utils::encode_mime_header(header);
}

} // namespace networkquests::smtp