#include "networkquests/smtp.hpp"

#include <sstream>
#include <iomanip>
#include <algorithm>
#include <random>
#include <regex>
#include <chrono>
#include <array>
#include <cctype>

namespace networkquests::smtp::smtp_utils {

std::string_view command_to_string(SmtpCommand command) {
    switch (command) {
        case SmtpCommand::EHLO: return "EHLO";
        case SmtpCommand::HELO: return "HELO";
        case SmtpCommand::MAIL: return "MAIL";
        case SmtpCommand::RCPT: return "RCPT";
        case SmtpCommand::DATA: return "DATA";
        case SmtpCommand::RSET: return "RSET";
        case SmtpCommand::NOOP: return "NOOP";
        case SmtpCommand::QUIT: return "QUIT";
        case SmtpCommand::VRFY: return "VRFY";
        case SmtpCommand::EXPN: return "EXPN";
        case SmtpCommand::HELP: return "HELP";
        case SmtpCommand::AUTH: return "AUTH";
        case SmtpCommand::STARTTLS: return "STARTTLS";
        default: return "UNKNOWN";
    }
}

Result<SmtpCommand> string_to_command(std::string_view command) {
    std::string upper_cmd;
    upper_cmd.reserve(command.size());
    std::transform(command.begin(), command.end(), std::back_inserter(upper_cmd),
                   [](char c) { return std::toupper(c); });
    
    if (upper_cmd == "EHLO") return SmtpCommand::EHLO;
    if (upper_cmd == "HELO") return SmtpCommand::HELO;
    if (upper_cmd == "MAIL") return SmtpCommand::MAIL;
    if (upper_cmd == "RCPT") return SmtpCommand::RCPT;
    if (upper_cmd == "DATA") return SmtpCommand::DATA;
    if (upper_cmd == "RSET") return SmtpCommand::RSET;
    if (upper_cmd == "NOOP") return SmtpCommand::NOOP;
    if (upper_cmd == "QUIT") return SmtpCommand::QUIT;
    if (upper_cmd == "VRFY") return SmtpCommand::VRFY;
    if (upper_cmd == "EXPN") return SmtpCommand::EXPN;
    if (upper_cmd == "HELP") return SmtpCommand::HELP;
    if (upper_cmd == "AUTH") return SmtpCommand::AUTH;
    if (upper_cmd == "STARTTLS") return SmtpCommand::STARTTLS;
    
    return make_error("Unknown SMTP command: " + std::string(command));
}

std::string response_code_to_string(SmtpResponseCode code) {
    return std::to_string(static_cast<int>(code));
}

std::string_view response_description(SmtpResponseCode code) {
    switch (code) {
        case SmtpResponseCode::SystemStatus: return "System status, or system help reply";
        case SmtpResponseCode::HelpMessage: return "Help message";
        case SmtpResponseCode::ServiceReady: return "Service ready";
        case SmtpResponseCode::ServiceClosing: return "Service closing transmission channel";
        case SmtpResponseCode::AuthenticationSuccessful: return "Authentication successful";
        case SmtpResponseCode::OK: return "Requested mail action okay, completed";
        case SmtpResponseCode::UserNotLocal: return "User not local; will forward";
        case SmtpResponseCode::CannotVerify: return "Cannot verify user, but will accept message";
        case SmtpResponseCode::StartMailInput: return "Start mail input; end with <CRLF>.<CRLF>";
        case SmtpResponseCode::ServiceNotAvailable: return "Service not available";
        case SmtpResponseCode::PasswordTransition: return "Password transition needed";
        case SmtpResponseCode::MailboxBusy: return "Requested mail action not taken: mailbox busy";
        case SmtpResponseCode::LocalError: return "Requested action aborted: local error in processing";
        case SmtpResponseCode::InsufficientStorage: return "Requested action not taken: insufficient system storage";
        case SmtpResponseCode::TemporaryAuthFailure: return "Temporary authentication failure";
        case SmtpResponseCode::SyntaxError: return "Syntax error, command unrecognized";
        case SmtpResponseCode::ParameterError: return "Syntax error in parameters or arguments";
        case SmtpResponseCode::CommandNotImplemented: return "Command not implemented";
        case SmtpResponseCode::BadSequence: return "Bad sequence of commands";
        case SmtpResponseCode::ParameterNotImplemented: return "Command parameter not implemented";
        case SmtpResponseCode::MailboxUnavailable: return "Requested action not taken: mailbox unavailable";
        case SmtpResponseCode::UserNotLocal2: return "User not local; please try";
        case SmtpResponseCode::ExceededStorage: return "Requested mail action aborted: exceeded storage allocation";
        case SmtpResponseCode::InvalidMailbox: return "Requested action not taken: mailbox name not allowed";
        case SmtpResponseCode::TransactionFailed: return "Transaction failed";
        case SmtpResponseCode::ParametersNotRecognized: return "Parameters not recognized or not implemented";
        default: return "Unknown response code";
    }
}

std::string_view auth_method_to_string(SmtpAuthMethod method) {
    switch (method) {
        case SmtpAuthMethod::NONE: return "NONE";
        case SmtpAuthMethod::PLAIN: return "PLAIN";
        case SmtpAuthMethod::LOGIN: return "LOGIN";
        case SmtpAuthMethod::CRAM_MD5: return "CRAM-MD5";
        case SmtpAuthMethod::DIGEST_MD5: return "DIGEST-MD5";
        case SmtpAuthMethod::OAUTH2: return "OAUTH2";
        default: return "UNKNOWN";
    }
}

Result<SmtpAuthMethod> string_to_auth_method(std::string_view method) {
    std::string upper_method;
    upper_method.reserve(method.size());
    std::transform(method.begin(), method.end(), std::back_inserter(upper_method),
                   [](char c) { return std::toupper(c); });
    
    if (upper_method == "NONE") return SmtpAuthMethod::NONE;
    if (upper_method == "PLAIN") return SmtpAuthMethod::PLAIN;
    if (upper_method == "LOGIN") return SmtpAuthMethod::LOGIN;
    if (upper_method == "CRAM-MD5") return SmtpAuthMethod::CRAM_MD5;
    if (upper_method == "DIGEST-MD5") return SmtpAuthMethod::DIGEST_MD5;
    if (upper_method == "OAUTH2") return SmtpAuthMethod::OAUTH2;
    
    return make_error("Unknown authentication method: " + std::string(method));
}

std::string_view extension_to_string(SmtpExtension extension) {
    switch (extension) {
        case SmtpExtension::SIZE: return "SIZE";
        case SmtpExtension::PIPELINING: return "PIPELINING";
        case SmtpExtension::ENHANCEDSTATUSCODES: return "ENHANCEDSTATUSCODES";
        case SmtpExtension::STARTTLS: return "STARTTLS";
        case SmtpExtension::AUTH: return "AUTH";
        case SmtpExtension::DELIVERYSTATUS: return "DSN";
        case SmtpExtension::BINARYMIME: return "BINARYMIME";
        case SmtpExtension::CHUNKING: return "CHUNKING";
        default: return "UNKNOWN";
    }
}

Result<SmtpExtension> string_to_extension(std::string_view extension) {
    std::string upper_ext;
    upper_ext.reserve(extension.size());
    std::transform(extension.begin(), extension.end(), std::back_inserter(upper_ext),
                   [](char c) { return std::toupper(c); });
    
    if (upper_ext == "SIZE") return SmtpExtension::SIZE;
    if (upper_ext == "PIPELINING") return SmtpExtension::PIPELINING;
    if (upper_ext == "ENHANCEDSTATUSCODES") return SmtpExtension::ENHANCEDSTATUSCODES;
    if (upper_ext == "STARTTLS") return SmtpExtension::STARTTLS;
    if (upper_ext == "AUTH") return SmtpExtension::AUTH;
    if (upper_ext == "DSN") return SmtpExtension::DELIVERYSTATUS;
    if (upper_ext == "BINARYMIME") return SmtpExtension::BINARYMIME;
    if (upper_ext == "CHUNKING") return SmtpExtension::CHUNKING;
    
    return make_error("Unknown SMTP extension: " + std::string(extension));
}

bool is_valid_email_address(std::string_view address) {
    // RFC 5322 compliant email validation (simplified)
    static const std::regex email_regex(
        R"(^[a-zA-Z0-9.!#$%&'*+/=?^_`{|}~-]+@[a-zA-Z0-9](?:[a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?(?:\.[a-zA-Z0-9](?:[a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?)*$)"
    );
    
    return std::regex_match(address.begin(), address.end(), email_regex);
}

std::string extract_domain(std::string_view address) {
    auto at_pos = address.find('@');
    if (at_pos == std::string_view::npos || at_pos == address.length() - 1) {
        return "";
    }
    return std::string(address.substr(at_pos + 1));
}

std::string extract_local_part(std::string_view address) {
    auto at_pos = address.find('@');
    if (at_pos == std::string_view::npos || at_pos == 0) {
        return "";
    }
    return std::string(address.substr(0, at_pos));
}

std::string base64_encode(std::string_view input) {
    static const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    
    int val = 0;
    int valb = -6;
    
    for (unsigned char c : input) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            result.push_back(chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    
    if (valb > -6) {
        result.push_back(chars[((val << 8) >> (valb + 8)) & 0x3F]);
    }
    
    while (result.size() % 4) {
        result.push_back('=');
    }
    
    return result;
}

Result<std::string> base64_decode(std::string_view input) {
    static const std::array<int, 256> decode_table = []() {
        std::array<int, 256> table{};
        std::fill(table.begin(), table.end(), -1);
        
        for (int i = 0; i < 26; ++i) {
            table['A' + i] = i;
            table['a' + i] = i + 26;
        }
        for (int i = 0; i < 10; ++i) {
            table['0' + i] = i + 52;
        }
        table['+'] = 62;
        table['/'] = 63;
        table['='] = 0;
        
        return table;
    }();
    
    std::string result;
    int val = 0;
    int valb = -8;
    
    for (unsigned char c : input) {
        if (decode_table[c] == -1) break;
        val = (val << 6) + decode_table[c];
        valb += 6;
        if (valb >= 0) {
            result.push_back(char((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    
    return result;
}

std::string encode_mime_header(std::string_view text, std::string_view charset) {
    // Check if encoding is needed (non-ASCII characters)
    bool needs_encoding = false;
    for (char c : text) {
        if (static_cast<unsigned char>(c) > 127) {
            needs_encoding = true;
            break;
        }
    }
    
    if (!needs_encoding) {
        return std::string(text);
    }
    
    // RFC 2047 encoded-word format: =?charset?encoding?encoded_text?=
    std::string encoded = base64_encode(text);
    return "=?" + std::string(charset) + "?B?" + encoded + "?=";
}

Result<std::string> decode_mime_header(std::string_view encoded) {
    // Simple implementation for RFC 2047 encoded-words
    static const std::regex encoded_word_regex(R"(=\?([^?]+)\?([BQ])\?([^?]*)\?=)");
    
    std::string result(encoded);
    std::smatch match;
    
    while (std::regex_search(result, match, encoded_word_regex)) {
        std::string charset = match[1].str();
        std::string encoding = match[2].str();
        std::string encoded_text = match[3].str();
        
        std::string decoded;
        if (encoding == "B" || encoding == "b") {
            auto decode_result = base64_decode(encoded_text);
            if (!decode_result) {
                return make_error("Failed to decode Base64: " + decode_result.error().message);
            }
            decoded = decode_result.value();
        } else if (encoding == "Q" || encoding == "q") {
            // Quoted-printable decoding (simplified)
            for (size_t i = 0; i < encoded_text.length(); ++i) {
                if (encoded_text[i] == '=') {
                    if (i + 2 < encoded_text.length()) {
                        std::string hex = encoded_text.substr(i + 1, 2);
                        try {
                            int value = std::stoi(hex, nullptr, 16);
                            decoded += static_cast<char>(value);
                            i += 2;
                        } catch (...) {
                            decoded += encoded_text[i];
                        }
                    } else {
                        decoded += encoded_text[i];
                    }
                } else if (encoded_text[i] == '_') {
                    decoded += ' ';
                } else {
                    decoded += encoded_text[i];
                }
            }
        } else {
            return make_error("Unknown encoding type: " + encoding);
        }
        
        result = result.substr(0, match.position()) + decoded + result.substr(match.position() + match.length());
    }
    
    return result;
}

std::string format_email_date() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto tm = std::gmtime(&time_t);
    
    // RFC 5322 date format: Mon, 01 Jan 2024 12:00:00 +0000
    static const char* day_names[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    static const char* month_names[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                       "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    
    std::ostringstream oss;
    oss << day_names[tm->tm_wday] << ", "
        << std::setfill('0') << std::setw(2) << tm->tm_mday << " "
        << month_names[tm->tm_mon] << " "
        << (1900 + tm->tm_year) << " "
        << std::setw(2) << tm->tm_hour << ":"
        << std::setw(2) << tm->tm_min << ":"
        << std::setw(2) << tm->tm_sec << " +0000";
    
    return oss.str();
}

std::string generate_message_id(std::string_view domain) {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    
    // Generate random component
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    
    std::string random_part;
    for (int i = 0; i < 8; ++i) {
        random_part += "0123456789ABCDEF"[dis(gen)];
    }
    
    return std::to_string(timestamp) + "." + random_part + "@" + std::string(domain);
}

Result<MimeType> parse_mime_type(std::string_view content_type) {
    MimeType result;
    
    // Split by semicolon to separate type from parameters
    auto semicolon_pos = content_type.find(';');
    std::string_view type_part = content_type.substr(0, semicolon_pos);
    
    // Parse main type
    auto slash_pos = type_part.find('/');
    if (slash_pos == std::string_view::npos) {
        return make_error("Invalid MIME type format");
    }
    
    result.type = std::string(type_part.substr(0, slash_pos));
    result.subtype = std::string(type_part.substr(slash_pos + 1));
    
    // Trim whitespace
    result.type.erase(0, result.type.find_first_not_of(" \t"));
    result.type.erase(result.type.find_last_not_of(" \t") + 1);
    result.subtype.erase(0, result.subtype.find_first_not_of(" \t"));
    result.subtype.erase(result.subtype.find_last_not_of(" \t") + 1);
    
    // Parse parameters
    if (semicolon_pos != std::string_view::npos) {
        std::string_view params = content_type.substr(semicolon_pos + 1);
        std::istringstream iss(std::string(params));
        std::string param;
        
        while (std::getline(iss, param, ';')) {
            auto eq_pos = param.find('=');
            if (eq_pos != std::string::npos) {
                std::string name = param.substr(0, eq_pos);
                std::string value = param.substr(eq_pos + 1);
                
                // Trim whitespace
                name.erase(0, name.find_first_not_of(" \t"));
                name.erase(name.find_last_not_of(" \t") + 1);
                value.erase(0, value.find_first_not_of(" \t"));
                value.erase(value.find_last_not_of(" \t") + 1);
                
                // Remove quotes if present
                if (value.length() >= 2 && value.front() == '"' && value.back() == '"') {
                    value = value.substr(1, value.length() - 2);
                }
                
                result.parameters[name] = value;
            }
        }
    }
    
    return result;
}

std::string get_mime_type_for_file(std::string_view filename) {
    // Extract file extension
    auto dot_pos = filename.find_last_of('.');
    if (dot_pos == std::string_view::npos) {
        return "application/octet-stream";
    }
    
    std::string ext = std::string(filename.substr(dot_pos + 1));
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    // Common MIME types
    static const std::unordered_map<std::string, std::string> mime_types = {
        {"txt", "text/plain"},
        {"html", "text/html"},
        {"htm", "text/html"},
        {"css", "text/css"},
        {"js", "text/javascript"},
        {"json", "application/json"},
        {"xml", "text/xml"},
        {"pdf", "application/pdf"},
        {"zip", "application/zip"},
        {"png", "image/png"},
        {"jpg", "image/jpeg"},
        {"jpeg", "image/jpeg"},
        {"gif", "image/gif"},
        {"bmp", "image/bmp"},
        {"mp4", "video/mp4"},
        {"avi", "video/avi"},
        {"mp3", "audio/mpeg"},
        {"wav", "audio/wav"},
        {"doc", "application/msword"},
        {"docx", "application/vnd.openxmlformats-officedocument.wordprocessingml.document"},
        {"xls", "application/vnd.ms-excel"},
        {"xlsx", "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"},
        {"ppt", "application/vnd.ms-powerpoint"},
        {"pptx", "application/vnd.openxmlformats-officedocument.presentationml.presentation"}
    };
    
    auto it = mime_types.find(ext);
    return it != mime_types.end() ? it->second : "application/octet-stream";
}

Result<void> send_simple_email(std::string_view smtp_server, Port port,
                              std::string_view from, std::string_view to,
                              std::string_view subject, std::string_view body,
                              std::string_view username, std::string_view password) {
    try {
        SmtpClient client;
        
        // Connect to server
        auto connect_result = client.connect(smtp_server, port);
        if (!connect_result) {
            return connect_result;
        }
        
        // Authenticate if credentials provided
        if (!username.empty() && !password.empty()) {
            auto auth_result = client.authenticate(username, password, SmtpAuthMethod::PLAIN);
            if (!auth_result) {
                return auth_result;
            }
        }
        
        // Create message
        SmtpMessage message;
        message.set_from(EmailAddress(from));
        message.add_to(EmailAddress(to));
        message.set_subject(subject);
        message.set_body(body);
        
        // Send message
        auto send_result = client.send_message(message);
        if (!send_result) {
            return send_result;
        }
        
        client.disconnect();
        return make_success();
        
    } catch (const std::exception& e) {
        return make_error("Email sending failed: " + std::string(e.what()));
    }
}

} // namespace networkquests::smtp::smtp_utils