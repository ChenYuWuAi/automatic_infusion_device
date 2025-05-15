#include "signal_handler.hpp"
#include "logger.hpp"

// 静态成员初始化
std::function<void(int)> SignalHandler::shutdownCallback_ = nullptr;

void SignalHandler::setup(std::function<void(int)> shutdownCallback) {
    // 保存回调函数
    shutdownCallback_ = shutdownCallback;
    
    // 设置信号处理器
    signal(SIGINT, SignalHandler::handleSignal);
    signal(SIGTERM, SignalHandler::handleSignal);
    
    InfusionLogger::info("信号处理已设置");
}

void SignalHandler::handleSignal(int signum) {
    InfusionLogger::info("接收到信号 ({}).", signum);
    
    // 调用回调函数
    if (shutdownCallback_) {
        shutdownCallback_(signum);
    }
}
