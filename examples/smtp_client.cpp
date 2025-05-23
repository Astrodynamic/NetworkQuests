#include "networkquests/smtp.hpp"

#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <thread>
#include <chrono>

using namespace networkquests;
using namespace networkquests::smtp;

class SmtpClientDemo {
public:
    void run() {
        std::cout << "=== NetworkQuests SMTP Client Demo ===" << std::endl;
        std::cout << "Simple Mail Transfer Protocol Implementation" << std::endl;
        std::cout << "Educational demonstration of email sending" << std::endl;
        std::cout << std::endl;

        while (true) {
            display_menu();
            int choice = get_user_choice();
            
            switch (choice) {
                case 1:
                    send_simple_email();
                    break;
                case 2:
                    send_email_with_attachment();
                    break;
                case 3:
                    send_authenticated_email();
                    break;
                case 4:
                    test_server_connection();
                    break;
                case 5:
                    send_bulk_emails();
                    break;
                case 6:
                    demonstrate_mime_features();
                    break;
                case 7:
                    test_error_handling();
                    break;
                case 8:
                    show_smtp_info();
                    break;
                case 0:
                    std::cout << "Goodbye!" << std::endl;
                    return;
                default:
                    std::cout << "Invalid choice. Please try again." << std::endl;
            }
            
            std::cout << std::endl;
        }
    }

private:
    void display_menu() {
        std::cout << "Choose an option:" << std::endl;
        std::cout << "1. Send Simple Email" << std::endl;
        std::cout << "2. Send Email with Attachment" << std::endl;
        std::cout << "3. Send Authenticated Email" << std::endl;
        std::cout << "4. Test Server Connection" << std::endl;
        std::cout << "5. Send Bulk Emails" << std::endl;
        std::cout << "6. Demonstrate MIME Features" << std::endl;
        std::cout << "7. Test Error Handling" << std::endl;
        std::cout << "8. Show SMTP Information" << std::endl;
        std::cout << "0. Exit" << std::endl;
        std::cout << "Enter choice: ";
    }

    int get_user_choice() {
        int choice;
        std::cin >> choice;
        std::cin.ignore(); // Clear the input buffer
        return choice;
    }

    void send_simple_email() {
        std::cout << "\n=== Simple Email Demo ===" << std::endl;
        
        std::string server, from, to, subject, body;
        int port;
        
        std::cout << "Enter SMTP server (e.g., localhost): ";
        std::getline(std::cin, server);
        
        std::cout << "Enter SMTP port (25, 587, or 2525): ";
        std::cin >> port;
        std::cin.ignore();
        
        std::cout << "Enter sender email: ";
        std::getline(std::cin, from);
        
        std::cout << "Enter recipient email: ";
        std::getline(std::cin, to);
        
        std::cout << "Enter subject: ";
        std::getline(std::cin, subject);
        
        std::cout << "Enter message body: ";
        std::getline(std::cin, body);
        
        try {
            SmtpClient client;
            
            std::cout << "\nConnecting to " << server << ":" << port << "..." << std::endl;
            auto connect_result = client.connect(server, port);
            if (!connect_result) {
                std::cout << "Connection failed: " << connect_result.error().message << std::endl;
                return;
            }
            
            std::cout << "Connected successfully!" << std::endl;
            
            // Create message
            SmtpMessage message;
            message.set_from(EmailAddress(from));
            message.add_to(EmailAddress(to));
            message.set_subject(subject);
            message.set_body(body);
            
            std::cout << "Sending email..." << std::endl;
            auto send_result = client.send_message(message);
            if (send_result) {
                std::cout << "✓ Email sent successfully!" << std::endl;
            } else {
                std::cout << "✗ Failed to send email: " << send_result.error().message << std::endl;
            }
            
            client.disconnect();
            
        } catch (const std::exception& e) {
            std::cout << "Exception: " << e.what() << std::endl;
        }
    }

    void send_email_with_attachment() {
        std::cout << "\n=== Email with Attachment Demo ===" << std::endl;
        
        std::string server, from, to, subject, body, filename;
        int port;
        
        std::cout << "Enter SMTP server: ";
        std::getline(std::cin, server);
        
        std::cout << "Enter port: ";
        std::cin >> port;
        std::cin.ignore();
        
        std::cout << "Enter sender email: ";
        std::getline(std::cin, from);
        
        std::cout << "Enter recipient email: ";
        std::getline(std::cin, to);
        
        std::cout << "Enter subject: ";
        std::getline(std::cin, subject);
        
        std::cout << "Enter message body: ";
        std::getline(std::cin, body);
        
        std::cout << "Enter attachment filename (or press Enter to create test file): ";
        std::getline(std::cin, filename);
        
        if (filename.empty()) {
            filename = "test_attachment.txt";
            create_test_file(filename);
        }
        
        try {
            SmtpClient client;
            
            auto connect_result = client.connect(server, port);
            if (!connect_result) {
                std::cout << "Connection failed: " << connect_result.error().message << std::endl;
                return;
            }
            
            // Create message with attachment
            SmtpMessage message;
            message.set_from(EmailAddress(from));
            message.add_to(EmailAddress(to));
            message.set_subject(subject);
            message.set_body(body);
            
            std::cout << "Adding attachment: " << filename << std::endl;
            message.add_attachment_from_file(filename);
            
            std::cout << "Estimated message size: " << message.estimated_size() << " bytes" << std::endl;
            
            auto send_result = client.send_message(message);
            if (send_result) {
                std::cout << "✓ Email with attachment sent successfully!" << std::endl;
            } else {
                std::cout << "✗ Failed to send email: " << send_result.error().message << std::endl;
            }
            
            client.disconnect();
            
        } catch (const std::exception& e) {
            std::cout << "Exception: " << e.what() << std::endl;
        }
    }

    void send_authenticated_email() {
        std::cout << "\n=== Authenticated Email Demo ===" << std::endl;
        
        std::string server, from, to, subject, body, username, password;
        int port;
        
        std::cout << "Enter SMTP server: ";
        std::getline(std::cin, server);
        
        std::cout << "Enter port (587 for STARTTLS, 465 for SSL): ";
        std::cin >> port;
        std::cin.ignore();
        
        std::cout << "Enter username: ";
        std::getline(std::cin, username);
        
        std::cout << "Enter password: ";
        std::getline(std::cin, password);
        
        std::cout << "Enter sender email: ";
        std::getline(std::cin, from);
        
        std::cout << "Enter recipient email: ";
        std::getline(std::cin, to);
        
        std::cout << "Enter subject: ";
        std::getline(std::cin, subject);
        
        std::cout << "Enter message body: ";
        std::getline(std::cin, body);
        
        try {
            SmtpClient client;
            
            auto connect_result = client.connect(server, port);
            if (!connect_result) {
                std::cout << "Connection failed: " << connect_result.error().message << std::endl;
                return;
            }
            
            // Check server capabilities
            auto extensions = client.get_extensions();
            if (extensions) {
                std::cout << "Server extensions: ";
                for (auto ext : extensions.value()) {
                    std::cout << smtp_utils::extension_to_string(ext) << " ";
                }
                std::cout << std::endl;
            }
            
            // Authenticate
            std::cout << "Authenticating..." << std::endl;
            auto auth_result = client.authenticate(username, password, SmtpAuthMethod::PLAIN);
            if (!auth_result) {
                std::cout << "Authentication failed: " << auth_result.error().message << std::endl;
                client.disconnect();
                return;
            }
            
            std::cout << "Authentication successful!" << std::endl;
            
            // Send message
            SmtpMessage message;
            message.set_from(EmailAddress(from));
            message.add_to(EmailAddress(to));
            message.set_subject(subject);
            message.set_body(body);
            message.set_priority(EmailPriority::High);
            
            auto send_result = client.send_message(message);
            if (send_result) {
                std::cout << "✓ Authenticated email sent successfully!" << std::endl;
            } else {
                std::cout << "✗ Failed to send email: " << send_result.error().message << std::endl;
            }
            
            client.disconnect();
            
        } catch (const std::exception& e) {
            std::cout << "Exception: " << e.what() << std::endl;
        }
    }

    void test_server_connection() {
        std::cout << "\n=== Server Connection Test ===" << std::endl;
        
        std::string server;
        int port;
        
        std::cout << "Enter SMTP server to test: ";
        std::getline(std::cin, server);
        
        std::cout << "Enter port: ";
        std::cin >> port;
        std::cin.ignore();
        
        try {
            SmtpClient client;
            client.set_timeout(std::chrono::seconds(10));
            
            std::cout << "Testing connection to " << server << ":" << port << "..." << std::endl;
            
            auto start_time = std::chrono::steady_clock::now();
            auto connect_result = client.connect(server, port);
            auto end_time = std::chrono::steady_clock::now();
            
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            
            if (connect_result) {
                std::cout << "✓ Connection successful! (took " << duration.count() << "ms)" << std::endl;
                
                // Get server capabilities
                auto extensions = client.get_extensions();
                if (extensions) {
                    std::cout << "\nServer capabilities:" << std::endl;
                    for (auto ext : extensions.value()) {
                        std::cout << "  - " << smtp_utils::extension_to_string(ext) << std::endl;
                    }
                }
                
                std::cout << "\nSupported features:" << std::endl;
                std::cout << "  - Authentication: " 
                          << (client.supports_extension(SmtpExtension::AUTH) ? "Yes" : "No") << std::endl;
                std::cout << "  - STARTTLS: " 
                          << (client.supports_extension(SmtpExtension::STARTTLS) ? "Yes" : "No") << std::endl;
                std::cout << "  - Pipelining: " 
                          << (client.supports_extension(SmtpExtension::PIPELINING) ? "Yes" : "No") << std::endl;
                
                client.disconnect();
            } else {
                std::cout << "✗ Connection failed: " << connect_result.error().message << std::endl;
            }
            
        } catch (const std::exception& e) {
            std::cout << "Exception: " << e.what() << std::endl;
        }
    }

    void send_bulk_emails() {
        std::cout << "\n=== Bulk Email Demo ===" << std::endl;
        
        std::string server, from, subject, body;
        int port, count;
        
        std::cout << "Enter SMTP server: ";
        std::getline(std::cin, server);
        
        std::cout << "Enter port: ";
        std::cin >> port;
        std::cin.ignore();
        
        std::cout << "Enter sender email: ";
        std::getline(std::cin, from);
        
        std::cout << "Enter subject: ";
        std::getline(std::cin, subject);
        
        std::cout << "Enter message body: ";
        std::getline(std::cin, body);
        
        std::cout << "Enter number of emails to send: ";
        std::cin >> count;
        std::cin.ignore();
        
        try {
            SmtpClient client;
            
            auto connect_result = client.connect(server, port);
            if (!connect_result) {
                std::cout << "Connection failed: " << connect_result.error().message << std::endl;
                return;
            }
            
            std::cout << "Sending " << count << " emails..." << std::endl;
            
            int success_count = 0;
            auto start_time = std::chrono::steady_clock::now();
            
            for (int i = 1; i <= count; ++i) {
                SmtpMessage message;
                message.set_from(EmailAddress(from));
                message.add_to(EmailAddress("test" + std::to_string(i) + "@example.com"));
                message.set_subject(subject + " #" + std::to_string(i));
                message.set_body(body + "\n\nEmail number: " + std::to_string(i));
                
                auto send_result = client.send_message(message);
                if (send_result) {
                    success_count++;
                    std::cout << "Email " << i << "/" << count << " sent ✓" << std::endl;
                } else {
                    std::cout << "Email " << i << "/" << count << " failed ✗: " 
                              << send_result.error().message << std::endl;
                }
                
                // Small delay to avoid overwhelming the server
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            
            auto end_time = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            
            std::cout << "\nBulk email summary:" << std::endl;
            std::cout << "  Total sent: " << success_count << "/" << count << std::endl;
            std::cout << "  Success rate: " << (100.0 * success_count / count) << "%" << std::endl;
            std::cout << "  Total time: " << duration.count() << "ms" << std::endl;
            std::cout << "  Average per email: " << (duration.count() / count) << "ms" << std::endl;
            
            client.disconnect();
            
        } catch (const std::exception& e) {
            std::cout << "Exception: " << e.what() << std::endl;
        }
    }

    void demonstrate_mime_features() {
        std::cout << "\n=== MIME Features Demo ===" << std::endl;
        
        try {
            SmtpMessage message;
            
            // Set basic fields
            message.set_from(EmailAddress("NetworkQuests Demo", "demo@networkquests.edu"));
            message.add_to(EmailAddress("Student", "student@university.edu"));
            message.add_cc(EmailAddress("Professor", "prof@university.edu"));
            message.set_subject("MIME Features Demonstration 📧");
            message.set_priority(EmailPriority::High);
            
            // Unicode content
            std::string body = "This email demonstrates MIME features:\n\n";
            body += "1. Unicode support: 你好, мир, 🌍\n";
            body += "2. HTML content (if supported)\n";
            body += "3. Multiple recipients (To + CC)\n";
            body += "4. Priority settings\n";
            body += "5. Custom headers\n\n";
            body += "Best regards,\nNetworkQuests Team";
            
            message.set_body(body);
            
            // Add custom headers
            message.set_header("X-Mailer", "NetworkQuests SMTP Client 1.0");
            message.set_header("X-Category", "Educational");
            message.set_header("Reply-To", "support@networkquests.edu");
            
            // Generate MIME representation
            std::string mime_content = message.to_mime_string();
            
            std::cout << "Generated MIME message:" << std::endl;
            std::cout << "=====================================";
            std::cout << std::endl << mime_content << std::endl;
            std::cout << "=====================================" << std::endl;
            
            std::cout << "\nMessage analysis:" << std::endl;
            std::cout << "  From: " << message.from().to_string() << std::endl;
            std::cout << "  To recipients: " << message.to().size() << std::endl;
            std::cout << "  CC recipients: " << message.cc().size() << std::endl;
            std::cout << "  Subject: " << message.subject() << std::endl;
            std::cout << "  Priority: " << static_cast<int>(message.priority()) << std::endl;
            std::cout << "  Estimated size: " << message.estimated_size() << " bytes" << std::endl;
            std::cout << "  Message ID: " << message.get_message_id() << std::endl;
            
        } catch (const std::exception& e) {
            std::cout << "Exception: " << e.what() << std::endl;
        }
    }

    void test_error_handling() {
        std::cout << "\n=== Error Handling Demo ===" << std::endl;
        
        std::cout << "Testing various error conditions..." << std::endl;
        
        // Test 1: Invalid server
        std::cout << "\n1. Testing connection to invalid server..." << std::endl;
        {
            SmtpClient client;
            client.set_timeout(std::chrono::seconds(5));
            auto result = client.connect("invalid.server.nonexistent", 25);
            std::cout << "Result: " << (result ? "Success" : "Failed as expected") << std::endl;
            if (!result) {
                std::cout << "Error: " << result.error().message << std::endl;
            }
        }
        
        // Test 2: Invalid email address
        std::cout << "\n2. Testing invalid email address validation..." << std::endl;
        {
            EmailAddress invalid_addr("not-an-email");
            std::cout << "Is 'not-an-email' valid? " << (invalid_addr.is_valid() ? "Yes" : "No") << std::endl;
            
            EmailAddress valid_addr("user@example.com");
            std::cout << "Is 'user@example.com' valid? " << (valid_addr.is_valid() ? "Yes" : "No") << std::endl;
        }
        
        // Test 3: Invalid message
        std::cout << "\n3. Testing invalid message validation..." << std::endl;
        {
            SmtpMessage invalid_msg;
            // No from address, no recipients
            std::cout << "Is empty message valid? " << (invalid_msg.is_valid() ? "Yes" : "No") << std::endl;
            
            invalid_msg.set_from(EmailAddress("sender@example.com"));
            invalid_msg.add_to(EmailAddress("recipient@example.com"));
            std::cout << "Is complete message valid? " << (invalid_msg.is_valid() ? "Yes" : "No") << std::endl;
        }
        
        // Test 4: SMTP command parsing
        std::cout << "\n4. Testing SMTP command parsing..." << std::endl;
        {
            auto valid_cmd = SmtpCommand_::from_string("EHLO localhost\r\n");
            if (valid_cmd) {
                std::cout << "Valid command parsed successfully" << std::endl;
            }
            
            auto invalid_cmd = SmtpCommand_::from_string("INVALID_COMMAND\r\n");
            std::cout << "Invalid command result: " << (invalid_cmd ? "Parsed" : "Failed as expected") << std::endl;
        }
        
        // Test 5: Base64 encoding/decoding
        std::cout << "\n5. Testing Base64 encoding..." << std::endl;
        {
            std::string original = "Hello, World! 🌍";
            std::string encoded = smtp_utils::base64_encode(original);
            auto decoded = smtp_utils::base64_decode(encoded);
            
            std::cout << "Original: " << original << std::endl;
            std::cout << "Encoded: " << encoded << std::endl;
            std::cout << "Decoded: " << (decoded ? decoded.value() : "Failed") << std::endl;
            std::cout << "Round-trip successful: " << (decoded && decoded.value() == original ? "Yes" : "No") << std::endl;
        }
    }

    void show_smtp_info() {
        std::cout << "\n=== SMTP Protocol Information ===" << std::endl;
        
        std::cout << "SMTP Commands:" << std::endl;
        std::vector<SmtpCommand> commands = {
            SmtpCommand::EHLO, SmtpCommand::HELO, SmtpCommand::MAIL,
            SmtpCommand::RCPT, SmtpCommand::DATA, SmtpCommand::QUIT,
            SmtpCommand::RSET, SmtpCommand::NOOP, SmtpCommand::VRFY,
            SmtpCommand::EXPN, SmtpCommand::HELP, SmtpCommand::AUTH,
            SmtpCommand::STARTTLS
        };
        
        for (auto cmd : commands) {
            std::cout << "  " << smtp_utils::command_to_string(cmd) << std::endl;
        }
        
        std::cout << "\nSMTP Response Codes:" << std::endl;
        std::vector<SmtpResponseCode> codes = {
            SmtpResponseCode::ServiceReady, SmtpResponseCode::OK,
            SmtpResponseCode::StartMailInput, SmtpResponseCode::ServiceNotAvailable,
            SmtpResponseCode::SyntaxError, SmtpResponseCode::MailboxUnavailable
        };
        
        for (auto code : codes) {
            std::cout << "  " << smtp_utils::response_code_to_string(code) 
                      << " - " << smtp_utils::response_description(code) << std::endl;
        }
        
        std::cout << "\nAuthentication Methods:" << std::endl;
        std::vector<SmtpAuthMethod> methods = {
            SmtpAuthMethod::NONE, SmtpAuthMethod::PLAIN,
            SmtpAuthMethod::LOGIN, SmtpAuthMethod::CRAM_MD5
        };
        
        for (auto method : methods) {
            std::cout << "  " << smtp_utils::auth_method_to_string(method) << std::endl;
        }
        
        std::cout << "\nSMTP Extensions:" << std::endl;
        std::vector<SmtpExtension> extensions = {
            SmtpExtension::SIZE, SmtpExtension::PIPELINING,
            SmtpExtension::STARTTLS, SmtpExtension::AUTH,
            SmtpExtension::ENHANCEDSTATUSCODES
        };
        
        for (auto ext : extensions) {
            std::cout << "  " << smtp_utils::extension_to_string(ext) << std::endl;
        }
        
        std::cout << "\nEmail Utilities:" << std::endl;
        std::cout << "  Current date: " << smtp_utils::format_email_date() << std::endl;
        std::cout << "  Sample Message ID: " << smtp_utils::generate_message_id("example.com") << std::endl;
        std::cout << "  Domain extraction (user@example.com): " << smtp_utils::extract_domain("user@example.com") << std::endl;
        std::cout << "  Local part extraction (user@example.com): " << smtp_utils::extract_local_part("user@example.com") << std::endl;
    }

    void create_test_file(const std::string& filename) {
        std::ofstream file(filename);
        file << "This is a test attachment file created by NetworkQuests SMTP Client.\n";
        file << "Created at: " << smtp_utils::format_email_date() << "\n";
        file << "\nThis file demonstrates file attachment capabilities in SMTP.\n";
        file << "The file is automatically base64-encoded when sent as an attachment.\n";
        file.close();
        std::cout << "Created test file: " << filename << std::endl;
    }
};

int main() {
    try {
        SmtpClientDemo demo;
        demo.run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}