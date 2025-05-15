#include "mqtt_thread_manager.hpp"
#include "logger.hpp"
#include <thread>
#include <chrono>

MQTTThreadManager::MQTTThreadManager(MQTTHandler &mqttHandler,
                                     BatteryMonitor &batteryMonitor,
                                     CameraManager &cameraManager,
                                     PumpParams &pumpParams,
                                     std::atomic<bool> &paramsUpdatedFlag)
        : mqttHandler_(mqttHandler),
          batteryMonitor_(batteryMonitor),
          cameraManager_(cameraManager),
          pumpParams_(pumpParams),
          paramsUpdatedFlag_(paramsUpdatedFlag) {
}

MQTTThreadManager::~MQTTThreadManager() {
    stop();
}

void MQTTThreadManager::start() {
    if (thread_running_.load()) {
        InfusionLogger::warn("MQTT线程已在运行!");
        return;
    }

    thread_running_ = true;
    std::thread thread(&MQTTThreadManager::mqttThread, this);
    thread.detach();
}

void MQTTThreadManager::stop() {
    // 停止前发送零流量和转速信息
    if (thread_running_.load() && mqttHandler_.isConnected()) {
        // 发送零流量和零转速
        mqttHandler_.sendPumpStateTelemetry(0.0, 0.0);
        InfusionLogger::info("已发送停止状态 (流量: 0, 转速: 0)");
    }

    thread_running_ = false;
    // 等待线程结束
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
}

bool MQTTThreadManager::isRunning() const {
    return thread_running_.load();
}

void MQTTThreadManager::mqttThread() {
    InfusionLogger::info("MQTT处理线程已启动");

    // 上次更新时间
    auto lastUpdateTime = std::chrono::steady_clock::now();

    while (thread_running_) {
        try {
            // 处理接收的MQTT消息
            mqtt::const_message_ptr msg;
            if (mqttHandler_.tryConsumeMessage(&msg)) {
                if (msg->get_topic().find("v1/devices/me/rpc/request/") != std::string::npos) {
                    mqttHandler_.handleRpcMessage(msg);
                } else if (msg->get_topic().find("v1/devices/me/attributes") != std::string::npos) {
                    mqttHandler_.handleAttributeMessage(msg, pumpParams_);
                    paramsUpdatedFlag_.store(true);

                    // 当参数更新时，使用泵数据库将流量转换为转速
                    double targetFlowRate = pumpParams_.target_flow_rate.load();
                    if (targetFlowRate >= 0 && pumpDatabase_ && !pumpName_.empty() && motorDriver_) {
                        // 计算目标转速
                        double targetRPM = pumpDatabase_->calculateRPM(pumpName_, targetFlowRate);

                        // 设置电机转速
                        InfusionLogger::info("将目标流量 {:.2f} ml/h 转换为转速 {:.2f} RPM",
                                             targetFlowRate, targetRPM);

                        pumpParams_.target_rpm.store(targetRPM);
                    }
                }
            }

            // 定时发送状态信息
            auto currentTime = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    currentTime - lastUpdateTime)
                    .count();

            if (elapsed >= UPDATE_INTERVAL) {
                // 更新电池状态
                if (batteryMonitor_.update() && batteryMonitor_.isBatteryPresent()) {
                    // 准备电池数据并由 MQTTHandler 发送遥测数据
                    json batteryTelemetry;
                    batteryTelemetry["battery"] = batteryMonitor_.getBatteryLevel();
                    batteryTelemetry["status"] = batteryMonitor_.getBatteryStatus();
                    batteryTelemetry["power"] = batteryMonitor_.getPower();
                    batteryTelemetry["current_state_remain_time"] = batteryMonitor_.getCurrentStateRemainTime();

                    mqttHandler_.sendTelemetry(batteryTelemetry);

                    InfusionLogger::info("电池状态更新成功: {}%", batteryMonitor_.getBatteryLevel());
                } else {
                    // Warn throttle
                    InfusionLogger::warn("电池状态更新失败");
                }

                // 发送液位百分比
                double liquidLevel = cameraManager_.getLiquidLevelPercentage();
                if (liquidLevel >= 0 && liquidLevel <= 100) {
                    json liquidTelemetry;
                    liquidTelemetry["progress"] = liquidLevel;
                    mqttHandler_.sendTelemetry(liquidTelemetry);
                }
                else {
                    InfusionLogger::warn("液位百分比无效: {}%", liquidLevel);
                }

                // 发送泵转速和流量
                double currentSpeed = 0.0;
                double currentFlowRate = 0.0;

                if (motorDriver_ && motorDriver_->isControlThreadRunning()) {
                    currentSpeed = motorDriver_->getSpeed();

                    // 如果有泵数据库，计算当前流量
                    if (pumpDatabase_ && !pumpName_.empty()) {
                        currentFlowRate = pumpDatabase_->calculateFlowRate(pumpName_, currentSpeed);
                    }
                }

                // 发送泵状态信息
                mqttHandler_.sendPumpStateTelemetry(currentFlowRate, currentSpeed);
                InfusionLogger::debug("已发送泵状态 - 流量: {:.2f} ml/h, 转速: {:.2f} RPM",
                                      currentFlowRate, currentSpeed);

                // 更新时间
                lastUpdateTime = currentTime;
            }

            // 控制消息处理频率
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        catch (const std::exception &e) {
            InfusionLogger::error("MQTT线程处理出错: {}", e.what());
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }

    InfusionLogger::info("MQTT处理线程已停止");
}
