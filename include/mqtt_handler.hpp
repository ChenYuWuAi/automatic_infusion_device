#ifndef MQTT_HANDLER_HPP
#define MQTT_HANDLER_HPP

#include <string>
#include <mqtt/client.h>
#include <functional>
#include <nlohmann/json.hpp>
#include <memory>
#include "pump_common.hpp"
#include "motor_driver.hpp"
#include "telemetry_interface.hpp"

using json = nlohmann::json;

/**
 * @brief MQTT处理器类，负责处理与MQTT相关的通信
 */
class MQTTHandler : public TelemetryInterface {
public:
    /**
     * @brief 构造函数
     * @param serverAddress MQTT服务器地址
     * @param clientId 客户端ID
     * @param username 用户名
     * @param topic 主题
     */
    MQTTHandler(const std::string& serverAddress, const std::string& clientId, const std::string& username);
    
    /**
     * @brief 析构函数
     */
    ~MQTTHandler();
    
    /**
     * @brief 连接到MQTT服务器
     * @return 连接是否成功
     */
    bool connect();
    
    /**
     * @brief 订阅主题
     * @param topic 主题
     * @param qos 服务质量
     * @return 订阅是否成功
     */
    bool subscribe(const std::string& topic, int qos = 1);
    
    /**
     * @brief 发布消息到主题
     * @param topic 主题
     * @param payload 消息内容
     * @param qos 服务质量
     * @return 发布是否成功
     */
    bool publish(const std::string& topic, const std::string& payload, int qos = 0);
    
    /**
     * @brief 检查是否连接
     * @return 是否连接
     */
    bool isConnected() const;
    
    /**
     * @brief 重新连接
     * @return 重连是否成功
     */
    bool reconnect();
    
    /**
     * @brief 消费消息
     * @param msg 接收的消息
     * @return 是否有消息
     */
    bool tryConsumeMessage(mqtt::const_message_ptr* msg);

    /**
     * @brief 处理RPC消息
     * @param msg RPC消息
     */
    void handleRpcMessage(mqtt::const_message_ptr msg);
    
    /**
     * @brief 处理属性消息
     * @param msg 属性消息
     * @param pumpParams 泵参数引用，用于更新
     */
    void handleAttributeMessage(mqtt::const_message_ptr msg, PumpParams& pumpParams);
    
    /**
     * @brief 发送液位百分比数据
     * @param percentage 液位百分比
     * @return 发送是否成功
     */
    bool sendLiquidLevelTelemetry(double percentage);
    
    /**
     * @brief 发送电池信息
     * @param capacity 容量
     * @param status 状态
     * @param power 功率
     * @param remainTime 剩余时间
     * @return 发送是否成功
     */
    bool sendBatteryTelemetry(int capacity, const std::string& status, double power, long long remainTime);

    /**
     * @brief 发送泵速信息
     * @param speed 泵速
     * @return 发送是否成功
     */
    bool sendPumpSpeedTelemetry(double speed);

    /**
     * @brief 发送泵状态信息
     * @param flowRate 当前流量
     * @param speed 当前转速
     * @return 发送是否成功
     */
    bool sendPumpStateTelemetry(double flowRate, double speed);

    /**
     * @brief 设置电机驱动器引用
     * @param motorDriver 电机驱动器指针
     */
    void setMotorDriver(MotorDriver* motorDriver) {
        motorDriver_ = motorDriver;
    }
    
    /**
     * @brief 设置泵参数引用
     * @param pumpParams 泵参数指针
     */
    void setPumpParams(PumpParams* pumpParams) {
        pumpParams_ = pumpParams;
    }
    
    /**
     * @brief 获取电机驱动器
     * @return 电机驱动器
     */
    MotorDriver* getMotorDriver() const {
        return motorDriver_;
    }

    /**
     * @brief 实现 TelemetryInterface::sendTelemetry
     * @param data 遥测数据JSON对象
     * @return 发送是否成功
     */
    virtual bool sendTelemetry(const json& data) override;
    
    /**
     * @brief 实现 TelemetryInterface::isReady
     * @return 是否就绪
     */
    virtual bool isReady() const override;
    
private:
    std::string serverAddress_;
    std::string clientId_;
    std::string username_;
    
    mqtt::client client_;
    
    // RPC响应主题前缀
    const std::string RESPONSE_TOPIC = "v1/devices/me/rpc/response/";
    // 遥测数据主题
    const std::string TELEMETRY_TOPIC = "v1/devices/me/telemetry";
    
    // 电机驱动器引用 - 供RPC函数调用
    MotorDriver* motorDriver_ = nullptr;
    // 泵参数引用 - 供RPC函数调用
    PumpParams* pumpParams_ = nullptr;
};

#endif // MQTT_HANDLER_HPP
