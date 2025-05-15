#ifndef SIGNAL_HANDLER_HPP
#define SIGNAL_HANDLER_HPP

#include <functional>
#include <signal.h>

/**
 * @brief 信号处理器类，管理系统信号处理
 */
class SignalHandler {
public:
    /**
     * @brief 设置信号处理函数
     * @param shutdownCallback 关闭回调函数
     */
    static void setup(std::function<void(int)> shutdownCallback);
    
    /**
     * @brief 内部信号处理函数，不要直接调用
     * @param signum 信号编号
     */
    static void handleSignal(int signum);
    
private:
    static std::function<void(int)> shutdownCallback_;
};

#endif // SIGNAL_HANDLER_HPP
