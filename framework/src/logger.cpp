#include <blaze/logger.h>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <iostream>
#include <filesystem>

namespace blaze {

Logger& Logger::instance() {
    static Logger instance;
    return instance;
}

Logger::Logger() {
    worker_ = std::thread(&Logger::process_queue, this);
}

Logger::~Logger() {
    running_ = false;
    cv_.notify_all();
    if (worker_.joinable()) {
        worker_.join();
    }
    if (file_stream_.is_open()) {
        file_stream_.flush();
        file_stream_.close();
    }
}

std::string Logger::get_timestamp() {
    const auto now = std::chrono::system_clock::now();
    const auto now_time_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
    
    localtime_r(&now_time_t, &tm_buf);

    std::stringstream ss;
    ss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

void Logger::process_queue() {
    while (running_ || !queue_.empty()) {
        std::string msg;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            cv_.wait(lock, [this] { return !queue_.empty() || !running_; });
            
            if (queue_.empty() && !running_) {
                break;
            }

            msg = std::move(queue_.front());
            queue_.pop();
        }

        std::stringstream output;
        output << "[" << get_timestamp() << "] " << msg << "\n";
        std::string out_str = output.str();

        std::lock_guard<std::mutex> config_lock(config_mutex_);
        if (use_stdout_) {
            if (msg.find("ERROR") != std::string::npos) {
                std::cerr << out_str;
            } else {
                std::cout << out_str;
            }
        } else if (!log_path_.empty()) {
            if (!file_stream_.is_open()) {
                file_stream_.open(log_path_, std::ios::out | std::ios::app);
            }
            if (file_stream_.is_open()) {
                file_stream_ << out_str;
                if (msg.find("ERROR") != std::string::npos) {
                    file_stream_.flush();
                }
            }
        }
    }
}

void Logger::configure(const std::string& path) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    if (path == "/dev/null") {
        enabled_ = false;
        return;
    }

    enabled_ = true;

    if (path == "stdout" || path.empty()) {
        use_stdout_ = true;
        log_path_ = "";
        return;
    }

    use_stdout_ = false;
    
    // Close existing if the path changed
    if (log_path_ != path && file_stream_.is_open()) {
        file_stream_.close();
    }
    
    log_path_ = path;

    std::filesystem::path p(path);
    if (p.has_parent_path()) {
        std::filesystem::create_directories(p.parent_path());
    }
}

void Logger::log(LogLevel level, std::string_view message) {
    {
        std::lock_guard<std::mutex> config_lock(config_mutex_);
        if (!enabled_ || level < level_) return;
    }

    std::string level_str;
    switch (level) {
        case LogLevel::DEBUG: level_str = "DEBUG"; break;
        case LogLevel::INFO:  level_str = "INFO";  break;
        case LogLevel::WARN:  level_str = "WARN";  break;
        case LogLevel::ERROR: level_str = "ERROR"; break;
    }

    std::string msg = level_str + ": " + std::string(message);
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        queue_.push(std::move(msg));
    }
    cv_.notify_one();
}

void Logger::log_access(std::string_view client_ip,
                       std::string_view method,
                       std::string_view path,
                       int status_code,
                       long long response_time_ms) {
    {
        std::lock_guard<std::mutex> config_lock(config_mutex_);
        if (!enabled_ || LogLevel::INFO < level_) return;
    }

    std::stringstream ss;
    ss << "ACCESS: " << client_ip << " " << method << " " << path << " " 
       << status_code << " " << response_time_ms << "ms";
    
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        queue_.push(ss.str());
    }
    cv_.notify_one();
}

void Logger::log_error(const std::string& message) {
    log(LogLevel::ERROR, message);
}

} // namespace blaze
