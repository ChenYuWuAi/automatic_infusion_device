#include <iostream>
#include "logger.hpp"
#include "infusion_app.hpp"
#include <fstream>
#include <string>
#include <unordered_map>

// 显示帮助信息
void showHelp(const char *programName) {
    std::cout << "用法: " << programName << " [选项]" << std::endl;
    std::cout << "选项:" << std::endl;
    std::cout << "  --log-level=LEVEL   设置日志级别 (trace, debug, info, warn, error, critical)" << std::endl;
    std::cout << "  --log-file=FILE     设置日志文件名 (默认: infusion_device.log)" << std::endl;
    std::cout << "  --console-only      只输出日志到控制台" << std::endl;
    std::cout << "  --file-only         只输出日志到文件" << std::endl;
    std::cout << "  --pump-data=FILE    指定泵数据文件路径 (默认: pump_data.json)" << std::endl;
    std::cout << "  --pump-name=NAME    指定泵名称 (默认: auto-infusion-01)" << std::endl;
    std::cout << "  --help, -h          显示帮助信息" << std::endl;
}

int main(int argc, char *argv[]) {
    // 日志配置默认值
    InfusionLogger::LogLevel logLevel = InfusionLogger::LogLevel::INFO;
    std::string logFile = "infusion_device.log";
    bool consoleOnly = false;
    bool fileOnly = false;

    // 泵配置默认值
    std::string pumpDataFile = "pump_data.json";
    std::string pumpName = "auto-infusion-01";

    // 解析命令行参数
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        // 帮助选项
        if (arg == "--help" || arg == "-h") {
            showHelp(argv[0]);
            return 0;
        }

        // 日志级别选项
        if (arg.find("--log-level=") == 0) {
            std::string levelStr = arg.substr(12); // 提取级别字符串

            static const std::unordered_map<std::string, InfusionLogger::LogLevel> levelMap = {
                    {"trace",    InfusionLogger::LogLevel::TRACE},
                    {"debug",    InfusionLogger::LogLevel::DEBUG},
                    {"info",     InfusionLogger::LogLevel::INFO},
                    {"warn",     InfusionLogger::LogLevel::WARN},
                    {"error",    InfusionLogger::LogLevel::ERROR},
                    {"critical", InfusionLogger::LogLevel::CRITICAL}
            };

            auto it = levelMap.find(levelStr);
            if (it != levelMap.end()) {
                logLevel = it->second;
            } else {
                std::cerr << "无效的日志级别: " << levelStr << std::endl;
                showHelp(argv[0]);
                return 1;
            }
        }
            // 日志文件选项
        else if (arg.find("--log-file=") == 0) {
            logFile = arg.substr(11); // 提取文件名
        }
            // 控制台输出选项
        else if (arg == "--console-only") {
            consoleOnly = true;
            fileOnly = false;
        }
            // 文件输出选项
        else if (arg == "--file-only") {
            fileOnly = true;
            consoleOnly = false;
        }
            // 泵数据文件选项
        else if (arg.find("--pump-data=") == 0) {
            pumpDataFile = arg.substr(12); // 提取文件名
        }
            // 泵名称选项
        else if (arg.find("--pump-name=") == 0) {
            pumpName = arg.substr(12); // 提取泵名称
        }
            // 未知选项
        else {
            std::cerr << "未知选项: " << arg << std::endl;
            showHelp(argv[0]);
            return 1;
        }
    }

    // 初始化日志系统
    InfusionLogger::init(logFile, logLevel, 1048576 * 5, 3, consoleOnly, fileOnly);
    InfusionLogger::info("自动输液设备启动");
    InfusionLogger::info("当前日志级别: {}", InfusionLogger::levelToString(logLevel));
    InfusionLogger::debug("日志配置 - 文件: {}, 仅控制台: {}, 仅文件: {}",
                          logFile, consoleOnly ? "是" : "否", fileOnly ? "是" : "否");
    InfusionLogger::info("泵配置 - 数据文件: {}, 泵名称: {}", pumpDataFile, pumpName);

    try {
        // 创建并初始化应用程序
        InfusionApp app(pumpDataFile, pumpName);

        if (!app.initialize()) {
            InfusionLogger::error("初始化应用程序失败！");
            return 1;
        }

        // 启动应用程序并等待退出
        if (!app.start()) {
            InfusionLogger::error("启动应用程序失败！");
            return 2;
        }

        // 应用程序正常退出
        InfusionLogger::info("应用程序已退出");
    }
    catch (const std::exception &e) {
        InfusionLogger::error("程序运行时出现异常: {}", e.what());
        return 3;
    }

    return 0;
}
