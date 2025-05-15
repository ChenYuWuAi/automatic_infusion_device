#ifndef TELEMETRY_INTERFACE_HPP
#define TELEMETRY_INTERFACE_HPP

#include <string>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

/**
 * @brief 遥测接口基类，定义数据发送的通用接口
 */
class TelemetryInterface {
public:
    virtual ~TelemetryInterface() = default;
    
    /**
     * @brief 发送遥测数据
     * @param data 遥测数据JSON对象
     * @return 发送是否成功
     */
    virtual bool sendTelemetry(const json& data) = 0;
    
    /**
     * @brief 检查是否连接就绪
     * @return 是否就绪
     */
    virtual bool isReady() const = 0;
};

#endif // TELEMETRY_INTERFACE_HPP
