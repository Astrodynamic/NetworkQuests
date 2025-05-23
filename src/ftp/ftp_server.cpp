#include "networkquests/ftp.hpp"
#include <filesystem>

namespace NetworkQuests::Ftp {

// FtpServer implementation
FtpServer::FtpServer(uint16_t port)
    : port_(port), root_directory_("/tmp/ftp"), max_connections_(50), 
      session_timeout_(std::chrono::minutes(30)), 
      welcome_message_("NetworkQuests FTP Server") {
    
    // Create default anonymous user
    FtpUserPermissions anon_perms;
    anon_perms.can_read = true;
    anon_perms.can_list = true;
    anon_perms.root_directory = root_directory_;
    
    anonymous_permissions_ = anon_perms;
}

FtpServer::~FtpServer() {
    stop();
}

Result<void> FtpServer::start() {
    if (running_) {
        return Error("Server is already running");
    }
    
    // Create root directory if it doesn't exist
    try {
        std::filesystem::create_directories(root_directory_);
    } catch (const std::exception& e) {
        return Error("Failed to create root directory: " + std::string(e.what()));
    }
    
    // Initialize TCP server
    tcp_server_ = std::make_unique<TcpServer>();
    auto start_result = tcp_server_->start(port_);
    if (!start_result) {
        return start_result.error();
    }
    
    running_ = true;
    should_stop_ = false;
    
    // Start connection acceptance thread
    accept_thread_ = std::thread(&FtpServer::accept_connections, this);
    
    return {};
}

void FtpServer::stop() {
    if (!running_) {
        return;
    }
    
    should_stop_ = true;
    running_ = false;
    
    // Stop TCP server
    if (tcp_server_) {
        tcp_server_->stop();
        tcp_server_.reset();
    }
    
    // Wait for accept thread to finish
    if (accept_thread_.joinable()) {
        accept_thread_.join();
    }
    
    // Clean up all active sessions
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    for (auto& session : active_sessions_) {
        if (session) {
            session->close();
        }
    }
    active_sessions_.clear();
}

void FtpServer::add_user(const FtpUser& user) {
    std::unique_lock<std::shared_mutex> lock(users_mutex_);
    users_[user.username] = user;
}

void FtpServer::remove_user(const std::string& username) {
    std::unique_lock<std::shared_mutex> lock(users_mutex_);
    users_.erase(username);
}

void FtpServer::enable_anonymous_access(bool enable, const FtpUserPermissions& perms) {
    std::unique_lock<std::shared_mutex> lock(users_mutex_);
    
    anonymous_enabled_ = enable;
    anonymous_permissions_ = perms;
    
    if (enable) {
        FtpUser anon_user;
        anon_user.username = "anonymous";
        anon_user.password = ""; // No password required
        anon_user.permissions = perms;
        anon_user.is_anonymous = true;
        
        users_["anonymous"] = anon_user;
        users_["ftp"] = anon_user; // Common alias
    } else {
        users_.erase("anonymous");
        users_.erase("ftp");
    }
}

bool FtpServer::authenticate_user(const std::string& username, const std::string& password) const {
    std::shared_lock<std::shared_mutex> lock(users_mutex_);
    
    auto user_it = users_.find(username);
    if (user_it == users_.end()) {
        return false;
    }
    
    const FtpUser& user = user_it->second;
    
    // For anonymous users, any password is accepted
    if (user.is_anonymous) {
        return true;
    }
    
    // For regular users, check password (simplified - in production, use proper hashing)
    return user.password == password;
}

void FtpServer::accept_connections() {
    while (running_ && !should_stop_) {
        try {
            auto connection_result = tcp_server_->accept_connection();
            if (!connection_result) {
                if (should_stop_) {
                    break;
                }
                // Log error and continue
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
            
            auto socket = std::move(*connection_result);
            
            // Check connection limit
            {
                std::lock_guard<std::mutex> lock(sessions_mutex_);
                if (active_sessions_.size() >= max_connections_) {
                    // Reject connection - send error and close
                    FtpResponse error(FtpResponseCode::SERVICE_UNAVAILABLE, 
                                    "Maximum connections exceeded");
                    std::string error_str = error.to_string() + "\r\n";
                    socket->send(Utils::string_to_bytes(error_str));
                    socket->close();
                    continue;
                }
            }
            
            // Create new session
            std::shared_lock<std::shared_mutex> users_lock(users_mutex_);
            auto session = std::make_unique<FtpSession>(std::move(socket), users_, root_directory_);
            
            // Start session in a new thread
            std::thread session_thread([session = std::move(session)]() mutable {
                session->run();
            });
            session_thread.detach();
            
            // Track session (simplified - in production, implement proper session tracking)
            total_connections_++;
            
            // Cleanup old sessions periodically
            cleanup_sessions();
            
        } catch (const std::exception& e) {
            if (should_stop_) {
                break;
            }
            // Log error and continue
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

void FtpServer::cleanup_sessions() {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    
    // Remove inactive sessions
    active_sessions_.erase(
        std::remove_if(active_sessions_.begin(), active_sessions_.end(),
                      [](const std::unique_ptr<FtpSession>& session) {
                          return session && !session->is_active();
                      }),
        active_sessions_.end()
    );
}

void FtpServer::session_cleanup_thread() {
    while (running_ && !should_stop_) {
        cleanup_sessions();
        std::this_thread::sleep_for(std::chrono::seconds(30));
    }
}

} // namespace NetworkQuests::Ftp