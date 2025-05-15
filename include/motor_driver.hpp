#ifndef MOTOR_DRIVER_HPP
#define MOTOR_DRIVER_HPP

#include <memory>
#include <atomic>
#include <gpiod.h>
#include <string>
#include "pump_common.hpp"

/**
 * @brief 合并电机控制和管理的类
 */
class MotorDriver {
public:
    /**
     * @brief 构造函数
     * @param chipname GPIO芯片名称
     * @param dirPin 方向控制引脚
     * @param microPins 细分控制引脚数组
     * @param motorPwmDevice 电机PWM设备路径
     * @param pumpState 泵状态引用
     */
    MotorDriver(const char* chipname, int dirPin, const int microPins[3], const char* motorPwmDevice, PumpState& pumpState);
    
    /**
     * @brief 析构函数
     */
    ~MotorDriver();
    
    /**
     * @brief 初始化电机
     * @return 是否初始化成功
     */
    bool initialize();
    
    /**
     * @brief 设置电机方向
     * @param direction 方向(0或1)
     */
    void setDirection(int direction);
    
    /**
     * @brief 设置电机细分控制
     * @param microstep 细分值(1,2,4,8,16,32)
     */
    void setMicrostep(int microstep);
    
    /**
     * @brief 获取当前方向
     * @return 方向值，0或1
     */
    int getDirection() const;
    
    /**
     * @brief 获取当前细分控制值
     * @return 细分控制值
     */
    int getMicrostep() const;
    
    /**
     * @brief 设置电机速度
     * @param speed 速度(rpm)
     */
    void setSpeed(double speed);
    
    /**
     * @brief 获取电机速度
     * @return 当前速度
     */
    double getSpeed() const;
    
    /**
     * @brief 设置电机状态
     * @param state 电机状态
     * @note 此方法只更新pumpState，不进行状态机转换，状态机处理在InfusionStateMachine中
     * @deprecated 直接使用状态机的setState方法
     */
    void setMotorState(PumpControlState state) {
        // 不能直接访问状态机，仅更新状态
        pumpState_.state.store(state);
    }
    
    /**
     * @brief 获取当前电机状态
     * @return 当前电机状态
     */
    PumpControlState getMotorState() const {
        return pumpState_.state.load();
    }
    
    /**
     * @brief 启动控制线程
     * @param pumpParams 泵参数引用
     * @param paramsUpdatedFlag 参数更新标志引用
     */
    void startControlThread(PumpParams& pumpParams, std::atomic<bool>& paramsUpdatedFlag);
    
    /**
     * @brief 停止控制线程
     */
    void stopControlThread();
    
    /**
     * @brief 控制线程是否在运行
     * @return 是否运行
     */
    bool isControlThreadRunning() const;
    
private:
    std::atomic<bool> control_thread_running_{false};
    std::atomic<double> current_speed_{0.0};
    
    // GPIO相关变量
    gpiod_chip* chip_ = nullptr;
    gpiod_line* dirLine_ = nullptr;
    gpiod_line* microLines_[3] = {nullptr, nullptr, nullptr};
    int currentDirection_ = 0;
    int currentMicrostep_ = 0;
    int motor_fd_ = -1;
    
    // 电机参数
    const char* chipname_;
    int dirPin_;
    int microPins_[3];
    const char* motorPwmDevice_;
    
    // 泵状态引用
    PumpState& pumpState_;
    
    /**
     * @brief 控制线程函数
     * @param pumpParams 泵参数引用
     * @param paramsUpdatedFlag 参数更新标志引用
     */
    void controlThread(PumpParams& pumpParams, std::atomic<bool>& paramsUpdatedFlag);
};

#endif // MOTOR_DRIVER_HPP
