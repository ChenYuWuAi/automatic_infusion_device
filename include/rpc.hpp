// rpc.hpp
#ifndef RPC_HPP
#define RPC_HPP

#include <string>
#include <map>
#include <functional>
#include <nlohmann/json.hpp>
#include "infusion_state_machine.hpp"

using json = nlohmann::json;
using RpcFunction = std::function<std::string(const json&)>;

// 获取全局函数注册表
static std::map<std::string, RpcFunction>& get_registry();

// 注册器，静态构造时完成注册
struct FunctionRegisterer {
    FunctionRegisterer(const std::string& name, RpcFunction fn);
};

// 调度 RPC 调用
std::string dispatch_rpc(const std::string& request_json);

// 全局变量声明
extern MotorDriver *g_motorDriver;
extern PumpParams g_pumpParams;
extern InfusionStateMachine *g_stateMachine;

#endif // RPC_HPP