#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <iostream>
#include <memory>

namespace InfusionLogger {

// 日志级别
enum LogLevel {
    TRACE = spdlog::level::trace,
    DEBUG = spdlog::level::debug,
    INFO = spdlog::level::info,
    WARN = spdlog::level::warn,
    ERROR = spdlog::level::err,
    CRITICAL = spdlog::level::critical,
    OFF = spdlog::level::off
};

// 将日志级别转换为字符串
inline std::string levelToString(LogLevel level) {
    switch (level) {
        case LogLevel::TRACE: return "trace";
        case LogLevel::DEBUG: return "debug";
        case LogLevel::INFO: return "info";
        case LogLevel::WARN: return "warn";
        case LogLevel::ERROR: return "error";
        case LogLevel::CRITICAL: return "critical";
        case LogLevel::OFF: return "off";
        default: return "unknown";
    }
}

// 初始化日志系统
inline void init(const std::string& log_file = "infusion_device.log", 
                LogLevel level = LogLevel::INFO,
                size_t max_file_size = 1048576 * 5,
                size_t max_files = 3,
                bool console_only = false,
                bool file_only = false) {
    try {
        std::vector<spdlog::sink_ptr> sinks;
        
        // 根据配置添加日志接收器
        if (!file_only) {
            // 添加控制台接收器
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink->set_level(static_cast<spdlog::level::level_enum>(level));
            sinks.push_back(console_sink);
        }
        
        if (!console_only) {
            // 添加文件接收器
            auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                log_file, max_file_size, max_files);
            file_sink->set_level(static_cast<spdlog::level::level_enum>(level));
            sinks.push_back(file_sink);
        }
        
        // 如果没有接收器（这种情况不应该发生），添加一个控制台接收器作为默认
        if (sinks.empty()) {
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink->set_level(static_cast<spdlog::level::level_enum>(level));
            sinks.push_back(console_sink);
        }
        
        // 创建并注册自定义日志记录器
        auto logger = std::make_shared<spdlog::logger>("infusion_logger", sinks.begin(), sinks.end());
        logger->set_level(static_cast<spdlog::level::level_enum>(level));
        logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v");
        
        // 设置为默认日志记录器
        spdlog::set_default_logger(logger);
        spdlog::flush_on(spdlog::level::info);
        
    } catch (const spdlog::spdlog_ex& ex) {
        std::cerr << "日志初始化失败: " << ex.what() << std::endl;
    }
}

// 提供方便的日志接口
inline void trace(const std::string& msg) { spdlog::trace(msg); }
inline void debug(const std::string& msg) { spdlog::debug(msg); }
inline void info(const std::string& msg) { spdlog::info(msg); }
inline void warn(const std::string& msg) { spdlog::warn(msg); }
inline void error(const std::string& msg) { spdlog::error(msg); }
inline void critical(const std::string& msg) { spdlog::critical(msg); }

// 支持格式化日志
template<typename... Args>
inline void trace(const std::string& fmt, Args&&... args) { 
    spdlog::trace(fmt, std::forward<Args>(args)...); 
}

template<typename... Args>
inline void debug(const std::string& fmt, Args&&... args) { 
    spdlog::debug(fmt, std::forward<Args>(args)...); 
}

template<typename... Args>
inline void info(const std::string& fmt, Args&&... args) { 
    spdlog::info(fmt, std::forward<Args>(args)...); 
}

template<typename... Args>
inline void warn(const std::string& fmt, Args&&... args) { 
    spdlog::warn(fmt, std::forward<Args>(args)...); 
}

template<typename... Args>
inline void error(const std::string& fmt, Args&&... args) { 
    spdlog::error(fmt, std::forward<Args>(args)...); 
}

template<typename... Args>
inline void critical(const std::string& fmt, Args&&... args) { 
    spdlog::critical(fmt, std::forward<Args>(args)...); 
}

// 设置全局日志级别
inline void setLevel(LogLevel level) {
    spdlog::set_level(static_cast<spdlog::level::level_enum>(level));
}

// 刷新日志
inline void flush() {
    spdlog::default_logger()->flush();
}

} // namespace InfusionLogger
