#ifndef INFUSION_STATE_MACHINE_HPP
#define INFUSION_STATE_MACHINE_HPP

#include "openfsm.h"
#include "pump_common.hpp"
#include "motor_driver.hpp"
#include <memory>
#include <nlohmann/json.hpp>
#include "pn532.h"
#include "pn532_rpi.h"
#include "linux_beep.h"

using json = nlohmann::json;

/**
 * @brief 输液状态机，管理泵的状态转换和行为
 */
class InfusionStateMachine
{
public:
    /**
     * @brief 构造函数
     * @param motorDriver 电机驱动器引用
     * @param pumpParams 泵参数引用
     * @param pumpState 泵状态引用
     */
    InfusionStateMachine(MotorDriver &motorDriver, PumpParams &pumpParams, PumpState &pumpState);

    /**
     * @brief 析构函数
     */
    ~InfusionStateMachine();

    /**
     * @brief 初始化状态机
     * @return 是否初始化成功
     */
    bool initialize();

    /**
     * @brief 更新状态机，调用状态机的update方法
     */
    void update();

    /**
     * @brief 设置泵状态
     * @param state 泵状态
     */
    void setState(PumpControlState state);

    /**
     * @brief 获取泵状态
     * @return 泵状态
     */
    PumpControlState getState() const;

    /**
     * @brief 验证状态转换是否合法
     * @param from 初始状态
     * @param to 目标状态
     * @return 如果转换合法则返回true，否则返回false
     */
    bool isValidStateTransition(PumpControlState from, PumpControlState to) const;

    // 状态机自定义数据结构 - 必须公开以便Action类访问
    struct FSMContext
    {
        MotorDriver *motorDriver;
        PumpParams *pumpParams;
        PumpState *pumpState;

        // 状态转换相关计时器（毫秒）
        int preparingTimer{0};
        int emergencyStopTimer{0};

        // 最后更新时间
        std::chrono::steady_clock::time_point lastUpdateTime;

        // 在FSMContext中添加PN532实例和初始化标志
        PN532 pn532;
        bool pn532Initialized{false};
    };

private:
    // 状态机实例
    std::unique_ptr<openfsm::OpenFSM> fsm_;

    // 组件引用
    MotorDriver &motorDriver_;
    PumpParams &pumpParams_;
    PumpState &pumpState_;

    // 状态机上下文数据
    FSMContext fsmContext_;

    friend class InfusionApp;
};

#endif // INFUSION_STATE_MACHINE_HPP
