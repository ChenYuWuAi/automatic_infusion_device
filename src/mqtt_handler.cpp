#include "mqtt_handler.hpp"
#include "logger.hpp"
#include "rpc.hpp"

MQTTHandler::MQTTHandler(const std::string& serverAddress, const std::string& clientId, const std::string& username)
    : serverAddress_(serverAddress), clientId_(clientId), username_(username), client_(serverAddress, clientId) {
}

MQTTHandler::~MQTTHandler() {
    // 断开连接
    try {
        if (client_.is_connected()) {
            client_.disconnect();
        }
    } catch (const mqtt::exception& e) {
        InfusionLogger::error("MQTT断开连接时出错：{}", e.what());
    }
}

bool MQTTHandler::connect() {
    try {
        mqtt::connect_options connOpts;
        connOpts.set_user_name(username_);
        client_.connect(connOpts);
        return true;
    } catch (const mqtt::exception& e) {
        InfusionLogger::error("MQTT连接错误：{}", e.what());
        return false;
    }
}

bool MQTTHandler::subscribe(const std::string& topic, int qos) {
    try {
        client_.subscribe(topic, qos);
        return true;
    } catch (const mqtt::exception& e) {
        InfusionLogger::error("MQTT订阅错误：{}", e.what());
        return false;
    }
}

bool MQTTHandler::publish(const std::string& topic, const std::string& payload, int qos) {
    try {
        client_.publish(topic, payload.c_str(), payload.length(), qos, false);
        return true;
    } catch (const mqtt::exception& e) {
        InfusionLogger::error("MQTT发布错误：{}", e.what());
        return false;
    }
}

bool MQTTHandler::isConnected() const {
    return client_.is_connected();
}

bool MQTTHandler::reconnect() {
    try {
        client_.reconnect();
        return true;
    } catch (const mqtt::exception& e) {
        InfusionLogger::error("MQTT重连错误：{}", e.what());
        return false;
    }
}

bool MQTTHandler::tryConsumeMessage(mqtt::const_message_ptr* msg) {
    return client_.try_consume_message(msg);
}

void MQTTHandler::handleRpcMessage(mqtt::const_message_ptr msg) {
    try {
        std::string payload(msg->to_string());

        // 获取请求的ID
        std::string requestId = msg->get_topic().substr(msg->get_topic().find_last_of('/') + 1);
        InfusionLogger::info("收到RPC请求 {}: {}", requestId, payload);

        // 确保全局变量已更新
        extern MotorDriver* g_motorDriver;
        extern PumpParams g_pumpParams;
        
        // 如果我们有本地引用，则更新全局变量
        if (motorDriver_ && pumpParams_) {
            g_motorDriver = motorDriver_;
            // 由于std::atomic不支持拷贝赋值，因此需要逐个成员赋值
            g_pumpParams.direction.store(pumpParams_->direction.load());
            g_pumpParams.target_flow_rate.store(pumpParams_->target_flow_rate.load());
            g_pumpParams.target_rpm.store(pumpParams_->target_rpm.load());
        }

        // 派发RPC请求
        std::string response = dispatch_rpc(payload);
        InfusionLogger::debug("响应: {}", response);

        if (!isConnected()) {
            InfusionLogger::warn("MQTT客户端未连接，正在重新连接...");
            reconnect();
        }
        
        // 发送RPC响应
        std::string responseTopic = RESPONSE_TOPIC + requestId;
        InfusionLogger::info("发送RPC响应 {} 到 {}", response, responseTopic);
        publish(responseTopic, response);
        InfusionLogger::info("RPC响应已发送！");
    } catch (const mqtt::exception& e) {
        InfusionLogger::error("发送MQTT响应时出错：{}", e.what());
    } catch (const std::exception& e) {
        InfusionLogger::error("处理RPC请求时出错：{}", e.what());
    }
}

void MQTTHandler::handleAttributeMessage(mqtt::const_message_ptr msg, PumpParams& pumpParams) {
    std::string payload(msg->to_string());

    // 解析JSON消息
    json request_json = json::parse(payload);
    
    // 如果有"shared"属性，则使用其中的值
    if (request_json.contains("shared")) {
        request_json = request_json["shared"];
    }

    // 更新泵参数
    for (const auto& item : request_json.items()) {
        std::string key = item.key();
        if (key == "pump_direction") {
            pumpParams.direction.store(item.value());
        } else if (key == "pump_flow_rate") {
            pumpParams.target_flow_rate.store(std::stod(std::string(item.value())));
        }
    }
}

bool MQTTHandler::sendLiquidLevelTelemetry(double percentage) {
    try {
        json telemetry;
        telemetry["liquid_level"] = percentage;
        std::string telemetryStr = telemetry.dump();

        if (isConnected()) {
            publish(TELEMETRY_TOPIC, telemetryStr);
            InfusionLogger::debug("发送液位百分比到远程: {}%", percentage);
            return true;
        } else {
            InfusionLogger::warn("MQTT客户端未连接，无法发送液位百分比");
            return false;
        }
    } catch (const std::exception& e) {
        InfusionLogger::error("发送液位百分比时出错: {}", e.what());
        return false;
    }
}

bool MQTTHandler::sendBatteryTelemetry(int capacity, const std::string& status, double power, long long remainTime) {
    try {
        json batteryTelemetry;
        batteryTelemetry["battery"] = capacity;
        batteryTelemetry["status"] = status;
        batteryTelemetry["power"] = power;
        batteryTelemetry["current_state_remain_time"] = remainTime;

        std::string batteryTelemetryStr = batteryTelemetry.dump();
        
        if (isConnected()) {
            publish(TELEMETRY_TOPIC, batteryTelemetryStr);
            InfusionLogger::debug("发送电池信息到远程: 电量={}%, 状态={}", capacity, status);
            return true;
        } else {
            InfusionLogger::warn("MQTT客户端未连接，无法发送电池信息");
            return false;
        }
    } catch (const std::exception& e) {
        InfusionLogger::error("发送电池信息时出错: {}", e.what());
        return false;
    }
}

bool MQTTHandler::sendPumpSpeedTelemetry(double speed) {
    try {
        json telemetry;
        telemetry["pumpSpeed"] = speed;
        std::string telemetryStr = telemetry.dump();

        if (isConnected()) {
            publish(TELEMETRY_TOPIC, telemetryStr);
            InfusionLogger::debug("发送泵转速到远程: {} RPM", speed);
            return true;
        } else {
            InfusionLogger::warn("MQTT客户端未连接，无法发送泵转速");
            return false;
        }
    } catch (const std::exception& e) {
        InfusionLogger::error("发送泵转速时出错: {}", e.what());
        return false;
    }
}

bool MQTTHandler::sendTelemetry(const json& data) {
    try {
        std::string telemetryStr = data.dump();
        
        if (isConnected()) {
            publish(TELEMETRY_TOPIC, telemetryStr);
            return true;
        } else {
            InfusionLogger::warn("MQTT客户端未连接，无法发送遥测数据");
            return false;
        }
    } catch (const std::exception& e) {
        InfusionLogger::error("发送遥测数据时出错: {}", e.what());
        return false;
    }
}

bool MQTTHandler::sendPumpStateTelemetry(double flowRate, double speed) {
    try {
        json telemetry = {
            {"flowRate", flowRate},
            {"speed", speed}
        };
        return sendTelemetry(telemetry);
    } catch (const std::exception& e) {
        InfusionLogger::error("发送泵状态遥测数据时出错: {}", e.what());
        return false;
    }
}

bool MQTTHandler::isReady() const {
    return isConnected();
}
