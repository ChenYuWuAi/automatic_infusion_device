#ifndef MQTT_THREAD_MANAGER_HPP
#define MQTT_THREAD_MANAGER_HPP

#include <atomic>
#include <functional>
#include "mqtt_handler.hpp"
#include "battery_monitor.hpp"
#include "camera_manager.hpp"
#include "pump_common.hpp"
#include "motor_driver.hpp"
#include "pump_database.hpp"

/**
 * @brief MQTT线程管理器类，管理MQTT消息处理线程
 */
class MQTTThreadManager {
public:
    /**
     * @brief 构造函数
     * @param mqttHandler MQTT处理器
     * @param batteryMonitor 电池监控器
     * @param cameraManager 相机管理器
     * @param pumpParams 泵参数引用
     * @param paramsUpdatedFlag 参数更新标志引用
     */
    MQTTThreadManager(MQTTHandler& mqttHandler, 
                      BatteryMonitor& batteryMonitor,
                      CameraManager& cameraManager,
                      PumpParams& pumpParams, 
                      std::atomic<bool>& paramsUpdatedFlag);
    
    /**
     * @brief 析构函数
     */
    ~MQTTThreadManager();
    
    /**
     * @brief 启动MQTT线程
     */
    void start();
    
    /**
     * @brief 停止MQTT线程
     */
    void stop();
    
    /**
     * @brief 线程是否在运行
     * @return 是否在运行
     */
    bool isRunning() const;

    /**
     * @brief 设置电机驱动器
     * @param motorDriver 电机驱动器指针
     */
    void setMotorDriver(MotorDriver* motorDriver) {
        motorDriver_ = motorDriver;
    }

    /**
     * @brief 设置泵数据库
     * @param pumpDatabase 泵数据库指针
     * @param pumpName 泵名称
     */
    void setPumpDatabase(PumpDatabase* pumpDatabase, const std::string& pumpName) {
        pumpDatabase_ = pumpDatabase;
        pumpName_ = pumpName;
    }
    
private:
    MQTTHandler& mqttHandler_;
    BatteryMonitor& batteryMonitor_;
    CameraManager& cameraManager_;
    PumpParams& pumpParams_;
    std::atomic<bool>& paramsUpdatedFlag_;
    
    // 电机驱动引用
    MotorDriver* motorDriver_ = nullptr;

    // 泵数据库
    PumpDatabase* pumpDatabase_ = nullptr;
    std::string pumpName_;
    
    // 线程运行标志
    std::atomic<bool> thread_running_{false};
    
    // 更新周期控制（单位：毫秒）
    const int UPDATE_INTERVAL = 1000;  // 信息更新间隔
    
    /**
     * @brief MQTT消息处理线程
     */
    void mqttThread();
};

#endif // MQTT_THREAD_MANAGER_HPP
