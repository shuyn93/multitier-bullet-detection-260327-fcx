#pragma once

#include <exception>
#include <string>
#include <sstream>
#include <iostream>
#include <memory>
#include <functional>

namespace bullet_detection {

// ===== Error Codes =====
enum class ErrorCode {
    SUCCESS = 0,
    INVALID_INPUT = 1,
    INVALID_STATE = 2,
    COMPUTATION_FAILURE = 3,
    MEMORY_ERROR = 4,
    IO_ERROR = 5,
    TRAINING_ERROR = 6,
    SYNCHRONIZATION_ERROR = 7,
    UNKNOWN_ERROR = 99
};

// ===== Exception Types =====
class BulletDetectionException : public std::exception {
public:
    explicit BulletDetectionException(ErrorCode code, const std::string& message)
        : code_(code), message_(message) {}

    ErrorCode getErrorCode() const { return code_; }
    const char* what() const noexcept override { return message_.c_str(); }

private:
    ErrorCode code_;
    std::string message_;
};

// ===== Result Type for Safe Error Propagation =====
template <typename T>
class Result {
public:
    static Result<T> success(const T& value) {
        Result<T> r;
        r.value_ = value;
        r.is_success_ = true;
        r.error_code_ = ErrorCode::SUCCESS;
        return r;
    }

    static Result<T> failure(ErrorCode code, const std::string& message) {
        Result<T> r;
        r.is_success_ = false;
        r.error_code_ = code;
        r.error_message_ = message;
        return r;
    }

    bool isSuccess() const { return is_success_; }
    bool isFailure() const { return !is_success_; }

    const T& getValue() const {
        if (!is_success_) {
            throw BulletDetectionException(error_code_, error_message_);
        }
        return value_;
    }

    T& getMutableValue() {
        if (!is_success_) {
            throw BulletDetectionException(error_code_, error_message_);
        }
        return value_;
    }

    ErrorCode getErrorCode() const { return error_code_; }
    const std::string& getErrorMessage() const { return error_message_; }

    // Optional-style access
    explicit operator bool() const { return is_success_; }

private:
    T value_;
    bool is_success_ = false;
    ErrorCode error_code_ = ErrorCode::UNKNOWN_ERROR;
    std::string error_message_;
};

// ===== Error Logger =====
class ErrorLogger {
public:
    enum class LogLevel { DEBUG, INFO, WARNING, ERROR, CRITICAL };

    static ErrorLogger& getInstance() {
        static ErrorLogger instance;
        return instance;
    }

    void log(LogLevel level, const std::string& message) {
        std::string prefix;
        switch (level) {
            case LogLevel::DEBUG:    prefix = "[DEBUG]"; break;
            case LogLevel::INFO:     prefix = "[INFO]"; break;
            case LogLevel::WARNING:  prefix = "[WARN]"; break;
            case LogLevel::ERROR:    prefix = "[ERROR]"; break;
            case LogLevel::CRITICAL: prefix = "[CRIT]"; break;
        }
        std::cout << prefix << " " << message << std::endl;
    }

    void setCallback(std::function<void(LogLevel, const std::string&)> callback) {
        callback_ = callback;
        use_callback_ = true;
    }

private:
    ErrorLogger() = default;
    std::function<void(LogLevel, const std::string&)> callback_;
    bool use_callback_ = false;
};

// ===== Guard for Exception Safety =====
class ExceptionGuard {
public:
    explicit ExceptionGuard(const std::string& context)
        : context_(context) {}

    ~ExceptionGuard() noexcept {
        if (std::uncaught_exceptions() > 0) {
            ErrorLogger::getInstance().log(
                ErrorLogger::LogLevel::CRITICAL,
                "Exception in context: " + context_
            );
        }
    }

private:
    std::string context_;
};

#define SAFE_CONTEXT() ExceptionGuard __guard(__FUNCTION__)

} // namespace bullet_detection
