#include "core/ProductionReady.h"
#include <iostream>
#include <iomanip>
#include <ctime>

namespace bullet_detection {

// ===== LOG ENTRY =====

std::string LogEntry::toJSON() const {
    auto t = std::chrono::system_clock::to_time_t(timestamp);
    std::stringstream ss;
    ss << "{"
       << R"("timestamp":")" << std::put_time(std::localtime(&t), "%Y-%m-%dT%H:%M:%S") << R"(",")"
       << R"("level":")" << level << R"(",")"
       << R"("module":")" << module << R"(",")"
       << R"("message":")" << message << R"(",")"
       << R"("thread":")" << thread_id << R"(")"
       << "}";
    return ss.str();
}

std::string LogEntry::toString() const {
    auto t = std::chrono::system_clock::to_time_t(timestamp);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S")
       << " [" << level << "] [" << module << "] [" << thread_id << "] "
       << message;
    return ss.str();
}

// ===== ADVANCED LOGGER =====

AdvancedLogger::AdvancedLogger() {
}

AdvancedLogger::~AdvancedLogger() {
    if (log_file_.is_open()) {
        log_file_.close();
    }
}

void AdvancedLogger::setLogFile(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    log_filepath_ = filepath;
    if (log_file_.is_open()) {
        log_file_.close();
    }
    log_file_.open(filepath, std::ios::app);
}

void AdvancedLogger::setLogLevel(Level level) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    min_level_ = level;
}

void AdvancedLogger::setMaxFileSize(size_t bytes) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    max_file_size_ = bytes;
}

void AdvancedLogger::setAsyncMode(bool async) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    async_enabled_ = async;
}

void AdvancedLogger::addModule(const std::string& module, Level level) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    module_levels_[module] = level;
}

void AdvancedLogger::log(Level level, const std::string& module, const std::string& message) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    
    // Check if module-specific level allows this
    auto it = module_levels_.find(module);
    if (it != module_levels_.end() && level < it->second) {
        return;  // Skip this log
    }
    
    // Check global minimum level
    if (level < min_level_) {
        return;
    }
    
    LogEntry entry;
    entry.timestamp = std::chrono::system_clock::now();
    
    switch (level) {
        case Level::TRACE:    entry.level = "TRACE";    statistics_.trace_count++; break;
        case Level::DEBUG:    entry.level = "DEBUG";    statistics_.debug_count++; break;
        case Level::INFO:     entry.level = "INFO";     statistics_.info_count++; break;
        case Level::WARN:     entry.level = "WARN";     statistics_.warn_count++; break;
        case Level::ERROR:    entry.level = "ERROR";    statistics_.error_count++; break;
        case Level::CRITICAL: entry.level = "CRITICAL"; statistics_.critical_count++; break;
    }
    
    entry.module = module;
    entry.message = message;
    std::stringstream ss;
    ss << std::this_thread::get_id();
    entry.thread_id = ss.str();
    
    statistics_.total_logs++;
    
    writeToFile(entry);
    writeToConsole(entry);
    flushIfNeeded();
}

void AdvancedLogger::writeToFile(const LogEntry& entry) {
    if (log_file_.is_open()) {
        log_file_ << entry.toJSON() << "\n";
        current_file_size_ += entry.toJSON().length() + 1;
    }
}

void AdvancedLogger::writeToConsole(const LogEntry& entry) {
    std::cout << entry.toString() << std::endl;
}

void AdvancedLogger::flushIfNeeded() {
    if (current_file_size_ >= max_file_size_) {
        rotateLogFile();
    }
}

void AdvancedLogger::rotateLogFile() {
    if (log_file_.is_open()) {
        log_file_.close();
    }
    
    // Rename current log file
    std::string backup_name = log_filepath_ + ".1";
    std::rename(log_filepath_.c_str(), backup_name.c_str());
    
    // Open new log file
    log_file_.open(log_filepath_, std::ios::app);
    current_file_size_ = 0;
}

AdvancedLogger::LogStatistics AdvancedLogger::getStatistics() const {
    std::lock_guard<std::mutex> lock(log_mutex_);
    return statistics_;
}

// ===== CONFIGURATION =====

bool Configuration::loadFromFile(const std::string& filepath) {
    // JSON parsing would go here
    return true;
}

bool Configuration::loadFromEnvironment() {
    // Load from environment variables
    // Typically all caps with underscore, e.g., BULLET_DETECTION_LOG_LEVEL
    return true;
}

bool Configuration::loadDefaults() {
    set("log_level", "INFO");
    set("max_track_distance", "50.0");
    set("kalman_pos_noise", "1.0");
    set("kalman_vel_noise", "0.5");
    return true;
}

void Configuration::set(const std::string& key, const std::string& value) {
    values_[key] = value;
}

template<>
int Configuration::parseValue<int>(const std::string& value) {
    return std::stoi(value);
}

template<>
float Configuration::parseValue<float>(const std::string& value) {
    return std::stof(value);
}

template<>
double Configuration::parseValue<double>(const std::string& value) {
    return std::stod(value);
}

template<>
std::string Configuration::parseValue<std::string>(const std::string& value) {
    return value;
}

template<>
bool Configuration::parseValue<bool>(const std::string& value) {
    return value == "true" || value == "1" || value == "yes";
}

bool Configuration::validate() {
    validation_errors_.clear();
    
    // Validate critical parameters
    auto log_level = get("log_level", std::string("INFO"));
    if (log_level != "DEBUG" && log_level != "INFO" && log_level != "WARN" && 
        log_level != "ERROR" && log_level != "CRITICAL") {
        validation_errors_.push_back("Invalid log_level: " + log_level);
    }
    
    return validation_errors_.empty();
}

std::vector<std::string> Configuration::getValidationErrors() const {
    return validation_errors_;
}

void Configuration::setProfile(const std::string& profile) {
    profile_ = profile;
}

std::string Configuration::toJSON() const {
    std::stringstream ss;
    ss << "{";
    bool first = true;
    for (const auto& [key, value] : values_) {
        if (!first) ss << ",";
        ss << R"(")" << key << R"(":")" << value << R"(")";
        first = false;
    }
    ss << "}";
    return ss.str();
}

bool Configuration::fromJSON(const std::string& json) {
    // Simple JSON parsing would go here
    return true;
}

// ===== RETRY POLICY =====

int RetryPolicy::getWaitTime(int attempt) const {
    switch (strategy) {
        case Strategy::IMMEDIATE:
            return 0;
        case Strategy::LINEAR:
            return 100 * (attempt + 1);
        case Strategy::EXPONENTIAL:
            return 100 * (1 << attempt);  // 100, 200, 400, 800ms
        default:
            return 100;
    }
}

// ===== CIRCUIT BREAKER =====

CircuitBreaker::CircuitBreaker(int failure_threshold, std::chrono::seconds timeout)
    : failure_threshold_(failure_threshold), timeout_(timeout) {}

CircuitBreaker::State CircuitBreaker::getState() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (state_ == State::OPEN) {
        auto now = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_failure_time_);
        if (elapsed > timeout_) {
            return State::HALF_OPEN;
        }
    }
    
    return state_;
}

bool CircuitBreaker::allowRequest() {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_ != State::OPEN;
}

void CircuitBreaker::recordSuccess() {
    std::lock_guard<std::mutex> lock(mutex_);
    failure_count_ = 0;
    state_ = State::CLOSED;
}

void CircuitBreaker::recordFailure() {
    std::lock_guard<std::mutex> lock(mutex_);
    failure_count_++;
    last_failure_time_ = std::chrono::system_clock::now();
    
    if (failure_count_ >= failure_threshold_) {
        state_ = State::OPEN;
    }
}

void CircuitBreaker::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    failure_count_ = 0;
    state_ = State::CLOSED;
}

// ===== HEALTH CHECKER =====

void HealthChecker::registerCheck(const std::string& name, 
                                  std::function<bool()> check) {
    std::lock_guard<std::mutex> lock(mutex_);
    checks_[name] = check;
}

HealthChecker::HealthStatus HealthChecker::checkHealth() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    HealthStatus status;
    status.is_healthy = true;
    
    for (const auto& [name, check] : checks_) {
        bool result = check();
        status.components[name] = result ? "OK" : "FAILED";
        if (!result) {
            status.is_healthy = false;
        }
    }
    
    status.status_message = status.is_healthy ? "All systems operational" : "Some checks failed";
    return status;
}

bool HealthChecker::runSelfTest() {
    return checkHealth().is_healthy;
}

// ===== METRICS COLLECTOR =====

void MetricsCollector::incrementCounter(const std::string& name, int value) {
    std::lock_guard<std::mutex> lock(mutex_);
    counters_[name] += value;
}

int MetricsCollector::getCounter(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = counters_.find(name);
    return it != counters_.end() ? it->second : 0;
}

void MetricsCollector::setGauge(const std::string& name, double value) {
    std::lock_guard<std::mutex> lock(mutex_);
    gauges_[name] = value;
}

double MetricsCollector::getGauge(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = gauges_.find(name);
    return it != gauges_.end() ? it->second : 0.0;
}

void MetricsCollector::recordHistogram(const std::string& name, double value) {
    std::lock_guard<std::mutex> lock(mutex_);
    histograms_[name].push_back(value);
}

std::vector<double> MetricsCollector::getHistogram(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = histograms_.find(name);
    return it != histograms_.end() ? it->second : std::vector<double>();
}

void MetricsCollector::startTimer(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    timers_[name] = std::chrono::high_resolution_clock::now();
}

void MetricsCollector::stopTimer(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = timers_.find(name);
    if (it != timers_.end()) {
        auto elapsed = std::chrono::high_resolution_clock::now() - it->second;
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
        recordHistogram(name + "_ms", ms);
    }
}

double MetricsCollector::getTimerMs(const std::string& name) const {
    auto hist = getHistogram(name + "_ms");
    if (hist.empty()) return 0.0;
    
    double sum = 0.0;
    for (double v : hist) sum += v;
    return sum / hist.size();
}

std::string MetricsCollector::exportJSON() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::stringstream ss;
    ss << "{\"counters\":{";
    
    bool first = true;
    for (const auto& [key, val] : counters_) {
        if (!first) ss << ",";
        ss << R"(")" << key << R"(":)" << val;
        first = false;
    }
    
    ss << "},\"gauges\":{";
    first = true;
    for (const auto& [key, val] : gauges_) {
        if (!first) ss << ",";
        ss << R"(")" << key << R"(":)" << val;
        first = false;
    }
    ss << "}}";
    return ss.str();
}

// ===== VERSION INFO =====

const char* VersionInfo::VERSION = "1.0.0";
const char* VersionInfo::BUILD_DATE = __DATE__;
const char* VersionInfo::BUILD_TYPE = "Release";
const char* VersionInfo::GIT_COMMIT = "production";
const char* VersionInfo::COMPILER = 
#ifdef _MSC_VER
    "MSVC " __DATE__;
#else
    "GCC " __DATE__;
#endif

std::string VersionInfo::getFullVersionString() {
    std::stringstream ss;
    ss << "Bullet Hole Detection System v" << VERSION
       << " (" << BUILD_TYPE << ")"
       << " Built: " << BUILD_DATE
       << " Commit: " << GIT_COMMIT;
    return ss.str();
}

} // namespace bullet_detection
