// rpc.hpp
#ifndef RPC_HPP
#define RPC_HPP

#include <string>
#include <map>
#include <functional>
#include <nlohmann/json.hpp>

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

#endif // RPC_HPP