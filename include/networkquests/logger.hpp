#pragma once

#include <string>
#include <string_view>
#include <iostream>
#include <fstream>
#include <sstream>
#include <mutex>
#include <memory>
#include <chrono>
#include <iomanip>

namespace networkquests {

enum class LogLevel {
    Debug = 0,
    Info,
    Warning,
    Error,
    Critical
};

class Logger {
public:
    static Logger& instance() {
        static Logger logger;
        return logger;
    }

    // Delete copy constructor and assignment operator
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void set_level(LogLevel level) {
        std::lock_guard<std::mutex> lock(mutex_);
        current_level_ = level;
    }

    void set_output_file(const std::string& filename) {
        std::lock_guard<std::mutex> lock(mutex_);
        file_stream_ = std::make_unique<std::ofstream>(filename, std::ios::app);
        use_file_ = file_stream_ && file_stream_->is_open();
    }

    void log(LogLevel level, const std::string& message, 
             const char* file = nullptr, 
             int line = 0) {
        
        if (level < current_level_) {
            return;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        
        std::stringstream ss;
        ss << "[" << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") << "] ";
        ss << "[" << level_to_string(level) << "] ";
        if (file && line > 0) {
            ss << "[" << extract_filename(file) << ":" << line << "] ";
        }
        ss << message << "\n";
        
        std::string log_entry = ss.str();

        if (use_file_ && file_stream_) {
            *file_stream_ << log_entry;
            file_stream_->flush();
        } else {
            auto& stream = (level >= LogLevel::Error) ? std::cerr : std::cout;
            stream << log_entry;
            stream.flush();
        }
    }

    // Template-based logging with string formatting
    template<typename... Args>
    void log_formatted(LogLevel level, const std::string& format_str, Args&&... args) {
        if (level < current_level_) {
            return;
        }
        
        std::string message = format_string(format_str, std::forward<Args>(args)...);
        log(level, message);
    }

private:
    Logger() = default;
    ~Logger() = default;

    static std::string_view level_to_string(LogLevel level) {
        switch (level) {
            case LogLevel::Debug:    return "DEBUG";
            case LogLevel::Info:     return "INFO";
            case LogLevel::Warning:  return "WARNING";
            case LogLevel::Error:    return "ERROR";
            case LogLevel::Critical: return "CRITICAL";
            default:                 return "UNKNOWN";
        }
    }

    static std::string extract_filename(const char* filepath) {
        if (!filepath) return "unknown";
        std::string path(filepath);
        auto pos = path.find_last_of("/\\");
        return (pos != std::string::npos) ? path.substr(pos + 1) : path;
    }

    // Simple string formatting function
    template<typename T>
    void format_arg(std::ostringstream& ss, const T& arg) {
        ss << arg;
    }

    template<typename T, typename... Args>
    std::string format_string(const std::string& format_str, T&& first, Args&&... rest) {
        std::ostringstream ss;
        format_string_impl(ss, format_str, 0, std::forward<T>(first), std::forward<Args>(rest)...);
        return ss.str();
    }

    std::string format_string(const std::string& format_str) {
        return format_str;
    }

    template<typename T, typename... Args>
    void format_string_impl(std::ostringstream& ss, const std::string& format_str, 
                           std::size_t pos, T&& first, Args&&... rest) {
        auto brace_pos = format_str.find("{}", pos);
        if (brace_pos != std::string::npos) {
            ss << format_str.substr(pos, brace_pos - pos);
            format_arg(ss, first);
            if constexpr (sizeof...(rest) > 0) {
                format_string_impl(ss, format_str, brace_pos + 2, std::forward<Args>(rest)...);
            } else {
                ss << format_str.substr(brace_pos + 2);
            }
        } else {
            ss << format_str.substr(pos);
        }
    }

    std::mutex mutex_;
    LogLevel current_level_ = LogLevel::Info;
    std::unique_ptr<std::ofstream> file_stream_;
    bool use_file_ = false;
};

// Convenience macros for logging
#define LOG_DEBUG(msg, ...) \
    networkquests::Logger::instance().log_formatted(networkquests::LogLevel::Debug, msg, ##__VA_ARGS__)
#define LOG_INFO(msg, ...) \
    networkquests::Logger::instance().log_formatted(networkquests::LogLevel::Info, msg, ##__VA_ARGS__)
#define LOG_WARNING(msg, ...) \
    networkquests::Logger::instance().log_formatted(networkquests::LogLevel::Warning, msg, ##__VA_ARGS__)
#define LOG_ERROR(msg, ...) \
    networkquests::Logger::instance().log_formatted(networkquests::LogLevel::Error, msg, ##__VA_ARGS__)
#define LOG_CRITICAL(msg, ...) \
    networkquests::Logger::instance().log_formatted(networkquests::LogLevel::Critical, msg, ##__VA_ARGS__)

} // namespace networkquests