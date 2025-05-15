// rpc.cpp
#include "rpc.hpp"
#include "logger.hpp"
#include <nlohmann/json.hpp>
#include <motor_driver.hpp>
#include <pump_common.hpp>

using json = nlohmann::json;

// 全局变量
extern MotorDriver *g_motorDriver;
extern PumpParams g_pumpParams;
extern InfusionStateMachine *g_stateMachine;

// 设置泵电源状态
std::string rpc_powerState_fn(const json &params) {
    if (!g_motorDriver || !g_stateMachine) {
        InfusionLogger::error("输液系统未完全初始化");
        json error_json;
        error_json["error"] = "Infusion system not fully initialized";
        return error_json.dump();
    }

    // 根据参数设置泵状态
    if (params == true) {
        // 设置泵状态为输液状态
        g_stateMachine->setState(INFUSING);
        InfusionLogger::info("泵状态设置为输液状态，目标流量: {}ml/h", g_pumpParams.target_flow_rate.load());
    } else {
        // 设置泵状态为暂停状态
        g_stateMachine->setState(PAUSED);
        InfusionLogger::info("泵状态设置为暂停状态");
    }

    // 把params原封不动塞进新的json
    json response_json;
    response_json["params"] = params;
    response_json["result"] = "ok";
    return response_json.dump();
}

// 启动泵
std::string rpc_startPumpState_fn(const json &params) {
    if (!g_motorDriver || !g_stateMachine) {
        InfusionLogger::error("输液系统未完全初始化");
        json error_json;
        error_json["error"] = "Infusion system not fully initialized";
        return error_json.dump();
    }

    // 计算流量
    if (params.size() != 1) {
        InfusionLogger::error("参数错误，期望一个参数");
        json error_json;
        error_json["error"] = "Invalid parameters";
        return error_json.dump();
    }
    if (!params[0].is_number()) {
        InfusionLogger::error("参数类型错误，期望数字");
        json error_json;
        error_json["error"] = "Invalid parameter type";
        return error_json.dump();
    }
    
    // 设置目标流量，状态机将负责计算转速和控制电机
    double flow_rate = abs(params[0].get<double>());
    g_pumpParams.target_flow_rate.store(flow_rate);
    
    // 仅设置状态，不直接操作电机
    g_stateMachine->setState(PREPARING);
    InfusionLogger::info("泵设置为准备状态，目标流量: {}ml/h", flow_rate);

    // 把params原封不动塞进新的json
    json response_json;
    response_json["params"] = params;
    response_json["result"] = "ok";
    return response_json.dump();
}

// 设置泵状态
std::string rpc_setPumpState_fn(const json &params) {
    if (!g_motorDriver || !g_stateMachine) {
        InfusionLogger::error("输液系统未完全初始化");
        json error_json;
        error_json["error"] = "Infusion system not fully initialized";
        return error_json.dump();
    }

    // 检查参数
    if (!params.is_string()) {
        InfusionLogger::error("参数类型错误，期望字符串");
        json error_json;
        error_json["error"] = "Invalid parameter type";
        return error_json.dump();
    }

    std::string state_str = params.get<std::string>();
    PumpControlState state;

    // 将字符串转换为状态枚举
    if (state_str == "IDLE") {
        state = IDLE;
    } else if (state_str == "VERIFY_PENDING") {
        state = VERIFY_PENDING;
    } else if (state_str == "VERIFIED") {
        state = VERIFIED;
    } else if (state_str == "PREPARING") {
        state = PREPARING;
    } else if (state_str == "INFUSING") {
        state = INFUSING;
    } else if (state_str == "PAUSED") {
        state = PAUSED;
    } else if (state_str == "EMERGENCY_STOP") {
        state = EMERGENCY_STOP;
    } else if (state_str == "ERROR") {
        state = ERROR;
    } else {
        InfusionLogger::error("未知的泵状态: {}", state_str);
        json error_json;
        error_json["error"] = "Unknown pump state";
        return error_json.dump();
    }

    // 设置泵状态（使用状态机）
    g_stateMachine->setState(state);
    InfusionLogger::info("泵状态已设置为: {}", state_str);

    // 构建响应
    json response_json;
    response_json["params"] = params;
    response_json["result"] = "ok";
    return response_json.dump();
}

// 紧急停止泵
std::string rpc_emergencyStop_fn(const json &params) {
    if (!g_motorDriver || !g_stateMachine) {
        InfusionLogger::error("输液系统未完全初始化");
        json error_json;
        error_json["error"] = "Infusion system not fully initialized";
        return error_json.dump();
    }
    
    // 设置泵状态为紧急停止
    g_stateMachine->setState(EMERGENCY_STOP);
    InfusionLogger::warn("泵紧急停止命令已执行");

    // 构建响应
    json response_json;
    response_json["params"] = params;
    response_json["result"] = "ok";
    return response_json.dump();
}

// 获取泵当前状态
std::string rpc_getPumpState_fn(const json &params) {
    if (!g_stateMachine) {
        InfusionLogger::error("输液系统未完全初始化");
        json error_json;
        error_json["error"] = "Infusion system not fully initialized";
        return error_json.dump();
    }

    // 获取当前状态
    PumpControlState state = g_stateMachine->getState();
    std::string state_str;

    // 将状态枚举转换为字符串
    switch (state) {
        case IDLE:
            state_str = "IDLE";
            break;
        case VERIFY_PENDING:
            state_str = "VERIFY_PENDING";
            break;
        case VERIFIED:
            state_str = "VERIFIED";
            break;
        case PREPARING:
            state_str = "PREPARING";
            break;
        case INFUSING:
            state_str = "INFUSING";
            break;
        case PAUSED:
            state_str = "PAUSED";
            break;
        case EMERGENCY_STOP:
            state_str = "EMERGENCY_STOP";
            break;
        case ERROR:
            state_str = "ERROR";
            break;
        default:
            state_str = "UNKNOWN";
            break;
    }

    // 构建响应
    json response_json;
    response_json["state"] = state_str;
    response_json["params"] = params;
    response_json["result"] = "ok";
    return response_json.dump();
}

// 验证状态转换是否有效
std::string rpc_validateStateTransition_fn(const json &params) {
    if (!g_stateMachine) {
        InfusionLogger::error("输液系统未完全初始化");
        json error_json;
        error_json["error"] = "Infusion system not fully initialized";
        return error_json.dump();
    }
    
    // 检查参数
    if (params.size() != 2 || !params[0].is_string() || !params[1].is_string()) {
        InfusionLogger::error("参数错误，期望两个字符串参数");
        json error_json;
        error_json["error"] = "Invalid parameters";
        return error_json.dump();
    }
    
    std::string from_str = params[0].get<std::string>();
    std::string to_str = params[1].get<std::string>();
    
    PumpControlState from_state;
    PumpControlState to_state;
    
    // 解析起始状态
    if (from_str == "IDLE") {
        from_state = IDLE;
    } else if (from_str == "VERIFY_PENDING") {
        from_state = VERIFY_PENDING;
    } else if (from_str == "VERIFIED") {
        from_state = VERIFIED;
    } else if (from_str == "PREPARING") {
        from_state = PREPARING;
    } else if (from_str == "INFUSING") {
        from_state = INFUSING;
    } else if (from_str == "PAUSED") {
        from_state = PAUSED;
    } else if (from_str == "EMERGENCY_STOP") {
        from_state = EMERGENCY_STOP;
    } else if (from_str == "ERROR") {
        from_state = ERROR;
    } else {
        InfusionLogger::error("未知的泵起始状态: {}", from_str);
        json error_json;
        error_json["error"] = "Unknown pump state";
        return error_json.dump();
    }
    
    // 解析目标状态
    if (to_str == "IDLE") {
        to_state = IDLE;
    } else if (to_str == "VERIFY_PENDING") {
        to_state = VERIFY_PENDING;
    } else if (to_str == "VERIFIED") {
        to_state = VERIFIED;
    } else if (to_str == "PREPARING") {
        to_state = PREPARING;
    } else if (to_str == "INFUSING") {
        to_state = INFUSING;
    } else if (to_str == "PAUSED") {
        to_state = PAUSED;
    } else if (to_str == "EMERGENCY_STOP") {
        to_state = EMERGENCY_STOP;
    } else if (to_str == "ERROR") {
        to_state = ERROR;
    } else {
        InfusionLogger::error("未知的泵目标状态: {}", to_str);
        json error_json;
        error_json["error"] = "Unknown pump state";
        return error_json.dump();
    }
    
    // 验证状态转换是否合法
    bool isValid = g_stateMachine->isValidStateTransition(from_state, to_state);
    
    // 构建响应
    json response_json;
    response_json["valid"] = isValid;
    response_json["from"] = from_str;
    response_json["to"] = to_str;
    response_json["params"] = params;
    response_json["result"] = "ok";
    return response_json.dump();
}

// 获取系统诊断信息
std::string rpc_getSystemDiagnostics_fn(const json &params) {
    if (!g_motorDriver || !g_stateMachine) {
        InfusionLogger::error("输液系统未完全初始化");
        json error_json;
        error_json["error"] = "Infusion system not fully initialized";
        return error_json.dump();
    }
    
    // 获取泵状态
    PumpControlState state = g_stateMachine->getState();
    std::string state_str;
    
    // 将状态枚举转换为字符串
    switch (state) {
        case IDLE: state_str = "IDLE"; break;
        case VERIFY_PENDING: state_str = "VERIFY_PENDING"; break;
        case VERIFIED: state_str = "VERIFIED"; break;
        case PREPARING: state_str = "PREPARING"; break;
        case INFUSING: state_str = "INFUSING"; break;
        case PAUSED: state_str = "PAUSED"; break;
        case EMERGENCY_STOP: state_str = "EMERGENCY_STOP"; break;
        case ERROR: state_str = "ERROR"; break;
        default: state_str = "UNKNOWN"; break;
    }
    
    // 获取流量和电机信息
    double targetFlowRate = g_pumpParams.target_flow_rate.load();
    double targetRPM = g_pumpParams.target_rpm.load();
    bool direction = g_pumpParams.direction.load();
    double currentSpeed = g_motorDriver->getSpeed();
    
    // 构建诊断信息响应
    json diagnostics;
    diagnostics["state"] = state_str;
    diagnostics["target_flow_rate"] = targetFlowRate;
    diagnostics["target_rpm"] = targetRPM;
    diagnostics["direction"] = direction ? "forward" : "reverse";
    diagnostics["current_speed"] = currentSpeed;
    diagnostics["timestamp"] = static_cast<uint64_t>(std::chrono::system_clock::now().time_since_epoch().count() / 1000000);
    
    // 构建响应
    json response_json;
    response_json["diagnostics"] = diagnostics;
    response_json["params"] = params;
    response_json["result"] = "ok";
    return response_json.dump();
}

// 静态注册器，用于注册RPC函数
static FunctionRegisterer reg_setPumpPower("setPumpPower", rpc_powerState_fn);
static FunctionRegisterer reg_startPump("startPump", rpc_startPumpState_fn);
static FunctionRegisterer reg_setPumpState("setPumpState", rpc_setPumpState_fn);
static FunctionRegisterer reg_emergencyStop("emergencyStop", rpc_emergencyStop_fn);
static FunctionRegisterer reg_getPumpState("getPumpState", rpc_getPumpState_fn);
static FunctionRegisterer reg_validateStateTransition("validateStateTransition", rpc_validateStateTransition_fn);
static FunctionRegisterer reg_getSystemDiagnostics("getSystemDiagnostics", rpc_getSystemDiagnostics_fn);

// 全局变量定义
MotorDriver *g_motorDriver = nullptr;
PumpParams g_pumpParams;
InfusionStateMachine *g_stateMachine = nullptr;

std::map<std::string, RpcFunction> &get_registry() {
    static std::map<std::string, RpcFunction> registry;
    return registry;
}

FunctionRegisterer::FunctionRegisterer(const std::string &name, RpcFunction fn) {
    get_registry()[name] = fn;
}

std::string dispatch_rpc(const std::string &request_json) {
    try {
        json req = json::parse(request_json);
        std::string method = req.at("method").get<std::string>();
        json params = req.value("params", json::array());

        InfusionLogger::debug("接收到RPC请求: {}", method);

        auto &registry = get_registry();
        auto it = registry.find(method);
        if (it == registry.end()) {
            InfusionLogger::warn("未知的RPC方法: {}", method);
            json err = {{"code",    -32601},
                        {"message", "Method not found"}};
            json resp = {{"result", nullptr},
                         {"error",  err}};
            return resp.dump();
        }

        std::string result = it->second(params);
        return result;
    } catch (const std::exception &e) {
        InfusionLogger::error("处理RPC请求时出错: {}", e.what());
        json err = {{"code",    -32603},
                    {"message", "Internal error"}};
        json resp = {{"result", nullptr},
                     {"error",  err}};
        return resp.dump();
    }
}