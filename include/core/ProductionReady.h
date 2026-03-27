#pragma once

#include <string>
#include <memory>
#include <vector>
#include <functional>
#include <mutex>
#include <fstream>
#include <sstream>
#include <chrono>
#include <queue>
#include <thread>
#include <atomic>
#include <map>
#include <iostream>

namespace bullet_detection {

// ===== STRUCTURED LOGGING =====

struct LogEntry {
    std::chrono::system_clock::time_point timestamp;
    std::string level;
    std::string module;
    std::string message;
    std::string thread_id;
    
    std::string toJSON() const;
    std::string toString() const;
};

// ===== ADVANCED LOGGER =====

class AdvancedLogger {
public:
    enum class Level { TRACE, DEBUG, INFO, WARN, ERROR, CRITICAL };
    
    static AdvancedLogger& getInstance() {
        static AdvancedLogger instance;
        return instance;
    }
    
    // Configuration
    void setLogFile(const std::string& filepath);
    void setLogLevel(Level level);
    void setMaxFileSize(size_t bytes);
    void setAsyncMode(bool async);
    void addModule(const std::string& module, Level level);
    
    // Logging
    void log(Level level, const std::string& module, const std::string& message);
    
    // Convenience macros will use these
    void trace(const std::string& module, const std::string& msg) { log(Level::TRACE, module, msg); }
    void debug(const std::string& module, const std::string& msg) { log(Level::DEBUG, module, msg); }
    void info(const std::string& module, const std::string& msg) { log(Level::INFO, module, msg); }
    void warn(const std::string& module, const std::string& msg) { log(Level::WARN, module, msg); }
    void error(const std::string& module, const std::string& msg) { log(Level::ERROR, module, msg); }
    void critical(const std::string& module, const std::string& msg) { log(Level::CRITICAL, module, msg); }
    
    // Statistics
    struct LogStatistics {
        uint64_t total_logs = 0;
        uint64_t trace_count = 0;
        uint64_t debug_count = 0;
        uint64_t info_count = 0;
        uint64_t warn_count = 0;
        uint64_t error_count = 0;
        uint64_t critical_count = 0;
    };
    LogStatistics getStatistics() const;
    
private:
AdvancedLogger();
~AdvancedLogger();
    
void flushIfNeeded();
void rotateLogFile();
void writeToFile(const LogEntry& entry);
void writeToConsole(const LogEntry& entry);
    
mutable std::mutex log_mutex_;
std::ofstream log_file_;
std::string log_filepath_;
Level min_level_ = Level::INFO;
size_t max_file_size_ = 10 * 1024 * 1024;  // 10MB
size_t current_file_size_ = 0;
bool async_enabled_ = false;
    
std::map<std::string, Level> module_levels_;
LogStatistics statistics_;
    
// For async logging - removed for now to fix compilation
// std::queue<LogEntry> log_queue_;
// std::thread log_thread_;
// std::atomic<bool> should_exit_ = false;
};

// ===== CONFIGURATION SYSTEM =====

class Configuration {
public:
    static Configuration& getInstance() {
        static Configuration instance;
        return instance;
    }
    
    // Load configuration
    bool loadFromFile(const std::string& filepath);
    bool loadFromEnvironment();
    bool loadDefaults();
    
    // Access values
    template<typename T>
    T get(const std::string& key, const T& default_value = T()) {
        auto it = values_.find(key);
        if (it != values_.end()) {
            return parseValue<T>(it->second);
        }
        return default_value;
    }
    
    // Set values
    void set(const std::string& key, const std::string& value);
    
    // Validation
    bool validate();
    std::vector<std::string> getValidationErrors() const;
    
    // Profiles (dev, test, prod)
    void setProfile(const std::string& profile);
    std::string getProfile() const { return profile_; }
    
    // Export/Import
    std::string toJSON() const;
    bool fromJSON(const std::string& json);
    
private:
    Configuration() = default;
    
    template<typename T>
    T parseValue(const std::string& value);
    
    std::map<std::string, std::string> values_;
    std::string profile_ = "production";
    std::vector<std::string> validation_errors_;
};

// ===== ERROR RECOVERY =====

class RetryPolicy {
public:
    enum class Strategy { IMMEDIATE, LINEAR, EXPONENTIAL };
    
    RetryPolicy(int max_attempts = 3, Strategy strategy = Strategy::EXPONENTIAL)
        : max_attempts(max_attempts), strategy(strategy) {}
    
    // Get wait time in milliseconds
    int getWaitTime(int attempt) const;
    
    int max_attempts;
    Strategy strategy;
};

// Execute function with retry
template<typename Func>
bool executeWithRetry(Func func, const RetryPolicy& policy = RetryPolicy()) {
    for (int attempt = 0; attempt < policy.max_attempts; ++attempt) {
        try {
            func();
            return true;
        } catch (const std::exception& e) {
            if (attempt < policy.max_attempts - 1) {
                int wait_ms = policy.getWaitTime(attempt);
                std::this_thread::sleep_for(std::chrono::milliseconds(wait_ms));
            }
        }
    }
    return false;
}

// ===== CIRCUIT BREAKER =====

class CircuitBreaker {
public:
    enum class State { CLOSED, OPEN, HALF_OPEN };
    
    explicit CircuitBreaker(
        int failure_threshold = 5,
        std::chrono::seconds timeout = std::chrono::seconds(60)
    );
    
    State getState() const;
    bool allowRequest();
    void recordSuccess();
    void recordFailure();
    void reset();
    
private:
    int failure_count_ = 0;
    int failure_threshold_;
    State state_ = State::CLOSED;
    std::chrono::system_clock::time_point last_failure_time_;
    std::chrono::seconds timeout_;
    mutable std::mutex mutex_;
};

// ===== HEALTH CHECKER =====

class HealthChecker {
public:
    struct HealthStatus {
        bool is_healthy;
        std::string status_message;
        std::map<std::string, std::string> components;
    };
    
    // Register check functions
    void registerCheck(const std::string& name, 
                      std::function<bool()> check);
    
    // Run all checks
    HealthStatus checkHealth();
    
    // Self-test
    bool runSelfTest();
    
private:
    std::map<std::string, std::function<bool()>> checks_;
    mutable std::mutex mutex_;
};

// ===== METRICS & MONITORING =====

class MetricsCollector {
public:
    static MetricsCollector& getInstance() {
        static MetricsCollector instance;
        return instance;
    }
    
    // Counter metrics
    void incrementCounter(const std::string& name, int value = 1);
    int getCounter(const std::string& name) const;
    
    // Gauge metrics
    void setGauge(const std::string& name, double value);
    double getGauge(const std::string& name) const;
    
    // Histogram metrics
    void recordHistogram(const std::string& name, double value);
    std::vector<double> getHistogram(const std::string& name) const;
    
    // Timer metrics
    void startTimer(const std::string& name);
    void stopTimer(const std::string& name);
    double getTimerMs(const std::string& name) const;
    
    // Export metrics
    std::string exportJSON() const;
    
private:
    MetricsCollector() = default;
    
    std::map<std::string, int> counters_;
    std::map<std::string, double> gauges_;
    std::map<std::string, std::vector<double>> histograms_;
    std::map<std::string, std::chrono::high_resolution_clock::time_point> timers_;
    mutable std::mutex mutex_;
};

// ===== VERSION INFO =====

struct VersionInfo {
    static const char* VERSION;
    static const char* BUILD_DATE;
    static const char* BUILD_TYPE;
    static const char* GIT_COMMIT;
    static const char* COMPILER;
    
    static std::string getFullVersionString();
};

// ===== DEPENDENCY INJECTION =====

class DIContainer {
public:
    static DIContainer& getInstance() {
        static DIContainer instance;
        return instance;
    }
    
    // Register services
    template<typename Interface, typename Implementation>
    void registerSingleton() {
        // Registration logic
    }
    
    // Resolve services
    template<typename Interface>
    std::shared_ptr<Interface> resolve() {
        // Resolution logic
        return nullptr;
    }
    
private:
    DIContainer() = default;
    std::map<std::string, std::function<void*()>> factories_;
};

// ===== PRODUCTION SHORTCUTS =====

#define LOG_TRACE(module, msg) AdvancedLogger::getInstance().trace(module, msg)
#define LOG_DEBUG(module, msg) AdvancedLogger::getInstance().debug(module, msg)
#define LOG_INFO(module, msg) AdvancedLogger::getInstance().info(module, msg)
#define LOG_WARN(module, msg) AdvancedLogger::getInstance().warn(module, msg)
#define LOG_ERROR(module, msg) AdvancedLogger::getInstance().error(module, msg)
#define LOG_CRITICAL(module, msg) AdvancedLogger::getInstance().critical(module, msg)

#define CONFIG(key, default_val) Configuration::getInstance().get(key, default_val)
#define METRICS AdvancedLogger::getInstance()

} // namespace bullet_detection
