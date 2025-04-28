// 存储泵参数和泵状态

#include <stdint.h>

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
    // 0: 空闲 1: 等待认证 2: 排空气 3: 输液中 4: 停止/暂停输液 5: 输液完成 6: 故障
    uint8_t pump_state;
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