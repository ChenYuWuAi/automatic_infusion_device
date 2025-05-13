// 存储泵参数和泵状态
#ifndef PUMP_COMMON_HPP
#define PUMP_COMMON_HPP
#include <stdint.h>

enum PumpControlState
{
    IDLE=100,       // 空闲状态
    VERIFY_PENDING, // 等待验证状态
    VERIFIED, // 已验证状态
    PREPARING, // 准备状态
    INFUSING, // 输液状态
    PAUSED, // 暂停状态
    EMERGENCY_STOP // 紧急停止状态
    ERROR, // 错误状态
};

typedef struct
{
    // 当前流量(估计) ml/mh
    // 当前转速 rpm
    // 液位高度 %
    // 方向
    // 输液进度 %
    // 目标流量 ml/h
    // 剩余时间 s
    double current_flow_rate;
    double current_speed;
    double liquid_height;
    bool direction;
    double infusion_progress;
    int remaining_time;
    PumpControlState state;
} PumpState;

typedef struct
{
    // 目标流量 ml/h
    // 目标转速 rpm
    // 方向
    // 输液进度 %
    double target_flow_rate;
    bool direction;
} PumpParams;


#endif // PUMP_COMMON_HPP