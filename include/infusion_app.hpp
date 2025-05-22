#ifndef INFUSION_APP_HPP
#define INFUSION_APP_HPP

#include <atomic>
#include <memory>
#include "mqtt_handler.hpp"
#include "battery_monitor.hpp"
#include "camera_manager.hpp"
#include "motor_driver.hpp"
#include "mqtt_thread_manager.hpp"
#include "pump_common.hpp"
#include "pump_database.hpp"
#include "infusion_state_machine.hpp"
#include "sound_effect_manager.hpp"

/**
 * @brief 输液应用程序类，整合所有组件
 */
class InfusionApp {
public:
    /**
     * @brief 构造函数
     */
    InfusionApp();
    
    /**
     * @brief 构造函数(带命令行参数)
     * @param pumpDataFile 泵参数文件路径
     * @param pumpName 泵名称
     */
    InfusionApp(const std::string& pumpDataFile, const std::string& pumpName);
    
    /**
     * @brief 析构函数
     */
    ~InfusionApp();
    
    /**
     * @brief 初始化应用程序
     * @return 是否初始化成功
     */
    bool initialize();
    
    /**
     * @brief 启动应用程序
     * @return 是否启动成功
     */
    bool start();
    
    /**
     * @brief 停止应用程序
     */
    void stop();
    
    /**
     * @brief 设置需要退出的标志
     * @param value 是否需要退出
     */
    void setNeedExit(bool value);
    
    /**
     * @brief 是否需要退出
     * @return 是否需要退出
     */
    bool needExit() const;
    
    /**
     * @brief 信号处理回调函数
     * @param signum 信号编号
     */
    void handleSignal(int signum);

private:
    // MQTT配置
    const std::string SERVER_ADDRESS = "mqtt://tb.chenyuwuai.xyz:1883";
    const std::string CLIENT_ID = "cpp_subscriber";
    const std::string USERNAME = "exggelffk6ghaw2hqus8";
    
    // 运行状态标志
    std::atomic<bool> running_{true};
    
    // 泵参数和状态
    PumpParams pumpParams_;
    PumpState pumpState_;
    std::atomic<bool> pump_params_updated_{false};
    
    // 液位传感百分比
    std::atomic<double> liquid_level_percentage_{-1.0};

    // 电机控制参数
    const char* GPIO_CHIPNAME = "gpiochip4";
    const int DIR_PIN = 27;
    int microPins_[3] = {16, 17, 20};
    const char* MOTOR_PWM_DEVICE = "/dev/input/by-path/platform-1000120000.pcie:rp1:pwm_beeper_19-event";
    const char* BEEP_DEVICE = "/dev/input/by-path/platform-1000120000.pcie:rp1:pwm_beeper_13-event";
    
    // 组件
    std::unique_ptr<MQTTHandler> mqttHandler_;
    std::unique_ptr<BatteryMonitor> batteryMonitor_;
    std::unique_ptr<CameraManager> cameraManager_;
    std::unique_ptr<MotorDriver> motorDriver_;
    std::unique_ptr<MQTTThreadManager> mqttThreadManager_;
    std::unique_ptr<InfusionStateMachine> stateMachine_;
    
    // 泵数据库和名称
    std::unique_ptr<PumpDatabase> pumpDatabase_;
    std::string pumpName_;
    std::string pumpDataFile_;
    
    /**
     * @brief 初始化声音管理器
     * @return 是否初始化成功
     */
    bool initializeSoundManager();
    
    /**
     * @brief 播放启动音效
     */
    void playStartupSound();
    
    /**
     * @brief 初始化泵数据库
     * @return 是否初始化成功
     */
    bool initializePumpDatabase();
    
    /**
     * @brief 初始化状态机
     * @return 是否初始化成功
     */
    bool initializeStateMachine();

    void playShutdownSound();
};

#endif // INFUSION_APP_HPP
