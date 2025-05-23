#pragma once

#include <string>
#include <string_view>
#include <system_error>
#include <chrono>
#include <concepts>
#include <memory>
#include <variant>
#include <optional>

namespace networkquests {

// Version information
constexpr std::string_view VERSION = "1.0.0";
constexpr int VERSION_MAJOR = 1;
constexpr int VERSION_MINOR = 0;
constexpr int VERSION_PATCH = 0;

// Common types
using Port = std::uint16_t;
using BufferSize = std::size_t;
using Timeout = std::chrono::milliseconds;

// Default values
constexpr Port DEFAULT_PORT = 8080;
constexpr BufferSize DEFAULT_BUFFER_SIZE = 1024;
constexpr Timeout DEFAULT_TIMEOUT{5000};

// Error handling
enum class NetworkError {
    Success = 0,
    ConnectionFailed,
    BindFailed,
    ListenFailed,
    AcceptFailed,
    SendFailed,
    ReceiveFailed,
    TimeoutExpired,
    InvalidAddress,
    ProtocolError,
    SecurityError
};

// Make NetworkError compatible with std::error_code
class NetworkErrorCategory : public std::error_category {
public:
    const char* name() const noexcept override {
        return "networkquests";
    }

    std::string message(int ev) const override {
        switch (static_cast<NetworkError>(ev)) {
            case NetworkError::Success:
                return "Success";
            case NetworkError::ConnectionFailed:
                return "Connection failed";
            case NetworkError::BindFailed:
                return "Bind failed";
            case NetworkError::ListenFailed:
                return "Listen failed";
            case NetworkError::AcceptFailed:
                return "Accept failed";
            case NetworkError::SendFailed:
                return "Send failed";
            case NetworkError::ReceiveFailed:
                return "Receive failed";
            case NetworkError::TimeoutExpired:
                return "Timeout expired";
            case NetworkError::InvalidAddress:
                return "Invalid address";
            case NetworkError::ProtocolError:
                return "Protocol error";
            case NetworkError::SecurityError:
                return "Security error";
            default:
                return "Unknown error";
        }
    }
};

inline const NetworkErrorCategory& network_error_category() {
    static NetworkErrorCategory instance;
    return instance;
}

inline std::error_code make_error_code(NetworkError e) {
    return {static_cast<int>(e), network_error_category()};
}

// Result type for operations that can fail - using variant instead of std::expected
template<typename T>
class Result {
private:
    std::variant<T, std::error_code> data_;

public:
    // Constructor for move-only types using perfect forwarding
    template<typename U, typename = std::enable_if_t<std::is_same_v<std::decay_t<U>, T>>>
    Result(U&& value) : data_(std::in_place_type<T>, std::forward<U>(value)) {}
    
    // Constructor for errors
    Result(std::error_code error) : data_(error) {}
    
    bool has_value() const noexcept {
        return std::holds_alternative<T>(data_);
    }
    
    explicit operator bool() const noexcept {
        return has_value();
    }
    
    T& value() & {
        if (!has_value()) {
            throw std::runtime_error("Result contains error, not value");
        }
        return std::get<T>(data_);
    }
    
    const T& value() const & {
        if (!has_value()) {
            throw std::runtime_error("Result contains error, not value");
        }
        return std::get<T>(data_);
    }
    
    T&& value() && {
        if (!has_value()) {
            throw std::runtime_error("Result contains error, not value");
        }
        return std::get<T>(std::move(data_));
    }
    
    template<typename U>
    T value_or(U&& default_value) const {
        if constexpr (std::is_copy_constructible_v<T>) {
            return has_value() ? std::get<T>(data_) : static_cast<T>(std::forward<U>(default_value));
        } else {
            static_assert(std::is_copy_constructible_v<T>, "value_or() requires copyable type");
        }
    }
    
    std::error_code error() const {
        if (has_value()) {
            throw std::runtime_error("Result contains value, not error");
        }
        return std::get<std::error_code>(data_);
    }
};

// Specialization for void
template<>
class Result<void> {
private:
    std::optional<std::error_code> error_;

public:
    Result() = default;
    Result(std::error_code error) : error_(error) {}
    
    bool has_value() const noexcept {
        return !error_.has_value();
    }
    
    explicit operator bool() const noexcept {
        return has_value();
    }
    
    void value() const {
        if (!has_value()) {
            throw std::runtime_error("Result contains error");
        }
    }
    
    std::error_code error() const {
        if (has_value()) {
            throw std::runtime_error("Result contains no error");
        }
        return error_.value();
    }
};

// Helper function for creating error results
template<typename T>
Result<T> make_error_result(std::error_code ec) {
    return Result<T>(ec);
}

// Concepts for type safety
template<typename T>
concept NetworkAddress = requires(T t) {
    { t.to_string() } -> std::convertible_to<std::string>;
    { t.port() } -> std::convertible_to<Port>;
};

template<typename T>
concept NetworkProtocol = requires(T t) {
    typename T::ClientType;
    typename T::ServerType;
    { T::protocol_name() } -> std::convertible_to<std::string_view>;
};

// Utility functions
namespace utils {

[[nodiscard]] std::string current_timestamp();
[[nodiscard]] std::string format_bytes(std::size_t bytes);
[[nodiscard]] bool is_valid_port(Port port);
[[nodiscard]] bool is_valid_ipv4(std::string_view ip);

} // namespace utils

} // namespace networkquests

// Enable std::error_code support for NetworkError
namespace std {
template<>
struct is_error_code_enum<networkquests::NetworkError> : true_type {};
}