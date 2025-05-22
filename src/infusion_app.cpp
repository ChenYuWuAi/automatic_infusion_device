#include "infusion_app.hpp"
#include "logger.hpp"
#include "sound_effect_manager.hpp"
#include "signal_handler.hpp"
#include "pn532.h"
#include "pn532_rpi.h"
#include <thread>
#include <functional>
#include <memory>

// 外部声明RPC全局变量
extern InfusionStateMachine *g_stateMachine;

InfusionApp::InfusionApp(const std::string &pumpDataFile, const std::string &pumpName)
    : pumpDataFile_(pumpDataFile), pumpName_(pumpName)
{
    // 初始化默认泵参数
    pumpParams_.direction.store(false);
    pumpParams_.target_flow_rate.store(0.0);
    pumpParams_.target_rpm.store(0.0);

    // 初始化默认泵状态
    pumpState_.state.store(IDLE);
    pumpState_.current_flow_rate.store(0.0);
    pumpState_.current_speed.store(0.0);
}

InfusionApp::~InfusionApp()
{
    stop();
}

bool InfusionApp::initialize()
{
    InfusionLogger::info("正在初始化输液应用程序...");

    try
    {
        // 初始化泵数据库
        if (!initializePumpDatabase())
        {
            InfusionLogger::error("初始化泵数据库失败!");
            return false;
        }

        // 初始化声音管理器
        if (!initializeSoundManager())
        {
            InfusionLogger::warn("初始化声音管理失败，继续执行...");
        }

        // 初始化电机驱动器 (必须在状态机之前)
        motorDriver_ = std::make_unique<MotorDriver>(GPIO_CHIPNAME, DIR_PIN, microPins_, MOTOR_PWM_DEVICE, pumpState_);
        if (!motorDriver_->initialize())
        {
            InfusionLogger::error("初始化电机驱动失败!");
            return false;
        }

        // 初始化状态机
        if (!initializeStateMachine())
        {
            InfusionLogger::error("初始化状态机失败!");
            return false;
        }

        // 初始化MQTT处理器
        mqttHandler_ = std::make_unique<MQTTHandler>(SERVER_ADDRESS, CLIENT_ID, USERNAME);
        if (!mqttHandler_->connect())
        {
            InfusionLogger::error("连接MQTT服务器失败!");
            return false;
        }

        // 订阅必要的主题
        mqttHandler_->subscribe("v1/devices/me/rpc/request/+");
        mqttHandler_->subscribe("v1/devices/me/attributes");
        mqttHandler_->subscribe("v1/devices/me/attributes/response/+");

        // 初始化电池监控器
        batteryMonitor_ = std::make_unique<BatteryMonitor>();

        // 初始化相机管理器
        cameraManager_ = std::make_unique<CameraManager>();
        if (!cameraManager_->initialize())
        {
            InfusionLogger::warn("初始化相机失败，继续执行...");
        }

        // 更新全局泵参数，供RPC使用
        extern PumpParams g_pumpParams;
        // 由于std::atomic不支持拷贝赋值，因此需要逐个成员赋值
        g_pumpParams.direction.store(pumpParams_.direction.load());
        g_pumpParams.target_flow_rate.store(pumpParams_.target_flow_rate.load());
        g_pumpParams.target_rpm.store(pumpParams_.target_rpm.load());

        // 设置MQTT处理器可以访问电机和泵参数
        mqttHandler_->setMotorDriver(motorDriver_.get());
        mqttHandler_->setPumpParams(&pumpParams_);

        // 初始化MQTT线程管理器
        mqttThreadManager_ = std::make_unique<MQTTThreadManager>(
            *mqttHandler_, *batteryMonitor_, *cameraManager_, pumpParams_, pumpState_, pump_params_updated_);
        // 设置电机驱动器到MQTT线程管理器
        mqttThreadManager_->setMotorDriver(motorDriver_.get());
        // 设置泵数据库到MQTT线程管理器
        mqttThreadManager_->setPumpDatabase(pumpDatabase_.get(), pumpName_);

        // 设置信号处理
        std::function<void(int)> signalCallback = std::bind(&InfusionApp::handleSignal, this, std::placeholders::_1);
        SignalHandler::setup(signalCallback);

        // 发送请求获取当前泵参数
        const std::string attrRequestTopic = "v1/devices/me/attributes/request/1";
        const std::string attrRequestPayload = R"({"sharedKeys":"pump_flow_rate,pump_direction"})";
        mqttHandler_->publish(attrRequestTopic, attrRequestPayload);

        InfusionLogger::info("应用程序初始化成功");
        return true;
    }
    catch (const std::exception &e)
    {
        InfusionLogger::error("初始化应用程序时发生错误: {}", e.what());
        return false;
    }
}

bool InfusionApp::start()
{
    InfusionLogger::info("正在启动输液应用程序...");

    try
    {
        // 播放启动音效
        playStartupSound();

        // 启动相机处理
        cameraManager_->startProcessing();

        // 启动电机控制线程
        motorDriver_->startControlThread(pumpParams_, pump_params_updated_);

        // 启动MQTT消息处理线程
        mqttThreadManager_->start();

        InfusionLogger::info("所有组件已成功启动");

        // 主循环作为监视器
        while (running_)
        {
            // 检查各个线程是否正常运行
            if (!cameraManager_->isRunning() || !motorDriver_->isControlThreadRunning() ||
                !mqttThreadManager_->isRunning())
            {
                InfusionLogger::error("错误：一个或多个线程意外停止！");
                // 尝试重新启动
                if (!cameraManager_->isRunning())
                {
                    cameraManager_->startProcessing();
                    InfusionLogger::info("相机处理线程已重新启动");
                }

                if (!motorDriver_->isControlThreadRunning())
                {
                    motorDriver_->startControlThread(pumpParams_, pump_params_updated_);
                    InfusionLogger::info("电机控制线程已重新启动");
                }

                if (!mqttThreadManager_->isRunning())
                {
                    mqttThreadManager_->start();
                    InfusionLogger::info("MQTT处理线程已重新启动");
                }
            }

            // 更新状态机
            if (stateMachine_)
            {
                stateMachine_->update();
            }

            // 使用较短的睡眠周期提高响应速度
            for (int i = 0; i < 10 && running_; ++i)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }

        return true;
    }
    catch (const std::exception &e)
    {
        InfusionLogger::error("启动应用程序时发生错误: {}", e.what());
        return false;
    }
}

void InfusionApp::stop()
{
    InfusionLogger::info("正在关闭输液应用程序...");

    // 停止所有音效
    g_soundEffectManager->stopAll();

    //    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // 播放关闭音效
    playShutdownSound();

    // 首先通过状态机将泵状态设置为IDLE
    if (stateMachine_)
    {
        stateMachine_->setState(IDLE);
        // 给状态机一点时间处理状态变化
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // 停止电机
    if (motorDriver_)
    {
        motorDriver_->setSpeed(0);
        motorDriver_->stopControlThread();
    }

    // 清空全局状态机指针
    g_stateMachine = nullptr;

    // 停止相机
    if (cameraManager_)
    {
        cameraManager_->stopProcessing();
    }

    // 停止MQTT线程
    if (mqttThreadManager_)
    {
        mqttThreadManager_->stop();
    }

    InfusionLogger::info("应用程序已关闭");
}

void InfusionApp::setNeedExit(bool value)
{
    running_ = !value;
}

bool InfusionApp::needExit() const
{
    return !running_;
}

void InfusionApp::handleSignal(int signum)
{
    InfusionLogger::info("接收到信号 ({})，准备退出程序。", signum);

    // 停止电机
    if (motorDriver_)
    {
        motorDriver_->setSpeed(0);
    }

    // 设置退出标志
    running_ = false;

    // 停止所有音效
    g_soundEffectManager->stopAll();
}

// 初始化声音管理器
bool InfusionApp::initializeSoundManager()
{
    try
    {
        g_soundEffectManager = std::make_shared<SoundEffectManager>();
        if (!g_soundEffectManager->initialize(BEEP_DEVICE))
        {
            InfusionLogger::error("初始化声音管理器失败!");
            return false;
        }
        return true;
    }
    catch (const std::exception &e)
    {
        InfusionLogger::error("初始化声音管理时出错: {}", e.what());
        return false;
    }
}

void InfusionApp::playStartupSound()
{
    try
    {
        g_soundEffectManager->playSound(buzzer_win10_plug_in, sizeof(buzzer_win10_plug_in) / sizeof(note_t));
        InfusionLogger::debug("启动音效已播放");
    }
    catch (const std::exception &e)
    {
        InfusionLogger::error("播放启动音效时出错: {}", e.what());
    }
}

void InfusionApp::playShutdownSound()
{
    try
    {
        g_soundEffectManager->playSound(buzzer_win10_remove, sizeof(buzzer_win10_remove) / sizeof(note_t));
        InfusionLogger::debug("停止音效已播放");
    }
    catch (const std::exception &e)
    {
        InfusionLogger::error("播放停止音效时出错: {}", e.what());
    }
}

bool InfusionApp::initializePumpDatabase()
{
    InfusionLogger::info("正在初始化泵数据库，数据文件：{}，泵名称：{}", pumpDataFile_, pumpName_);

    try
    {
        // 创建并加载泵数据库
        pumpDatabase_ = std::make_unique<PumpDatabase>(pumpDataFile_);

        // 检查指定的泵是否存在
        const PumpData *pumpData = pumpDatabase_->getPump(pumpName_);
        if (!pumpData)
        {
            InfusionLogger::error("无法找到泵名称: {}", pumpName_);
            return false;
        }

        InfusionLogger::info("泵数据库初始化成功，找到泵: {}", pumpName_);
        return true;
    }
    catch (const std::exception &e)
    {
        InfusionLogger::error("初始化泵数据库时出错: {}", e.what());
        return false;
    }
}

bool InfusionApp::initializeStateMachine()
{
    InfusionLogger::info("正在初始化输液状态机...");

    try
    {
        // 确保电机驱动已初始化
        if (!motorDriver_)
        {
            InfusionLogger::error("无法初始化状态机：电机驱动未初始化");
            return false;
        }

        // 创建状态机实例
        stateMachine_ = std::make_unique<InfusionStateMachine>(*motorDriver_, pumpParams_, pumpState_);

        // 初始化状态机
        if (!stateMachine_->initialize())
        {
            InfusionLogger::error("状态机初始化失败");
            return false;
        }

        // 设置全局状态机指针（用于RPC调用）
        g_stateMachine = stateMachine_.get();

        InfusionLogger::info("正在初始化PN532 NFC模块...");

        // 初始化PN532 NFC模块
        PN532_UART_Init(&g_stateMachine->fsmContext_.pn532);
        {
            uint8_t buff[255];
            if (PN532_GetFirmwareVersion(&g_stateMachine->fsmContext_.pn532, buff) == PN532_STATUS_OK)
            {
                PN532_SamConfiguration(&g_stateMachine->fsmContext_.pn532);
                g_stateMachine->fsmContext_.pn532Initialized = true;
                InfusionLogger::info("PN532模块初始化成功，固件版本: {}.{}", buff[1], buff[2]);
            }
            else
            {
                InfusionLogger::warn("PN532模块初始化失败");
                // 退出程序
                return false;
            }
        }

        InfusionLogger::info("状态机初始化成功");

        return true;
    }
    catch (const std::exception &e)
    {
        InfusionLogger::error("初始化状态机时出错: {}", e.what());
        return false;
    }
}
