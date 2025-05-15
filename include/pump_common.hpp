// 存储泵参数和泵状态
#ifndef PUMP_COMMON_HPP
#define PUMP_COMMON_HPP
#include <stdint.h>
#include <atomic>

enum PumpControlState
{
    IDLE = 100,     // 空闲状态
    VERIFY_PENDING, // 等待验证状态
    VERIFIED,       // 已验证状态
    PREPARING,      // 准备状态
    INFUSING,       // 输液状态
    PAUSED,         // 暂停状态
    EMERGENCY_STOP, // 紧急停止状态
    ERROR,          // 错误状态
};

struct PumpState
{
    std::atomic<double> current_flow_rate{0.0};
    std::atomic<double> current_speed{0.0};
    std::atomic<double> liquid_height{0.0};
    std::atomic<bool> direction{false};
    std::atomic<double> infusion_progress{0.0};
    std::atomic<int> remaining_time{0};
    std::atomic<PumpControlState> state{IDLE};
};

struct PumpParams
{
    std::atomic<double> target_flow_rate{0.0};
    std::atomic<double> target_rpm{0.0};
    std::atomic<bool> direction{false};
};

#endif // PUMP_COMMON_HPP