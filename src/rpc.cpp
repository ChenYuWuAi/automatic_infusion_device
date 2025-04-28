// rpc.cpp
#include "rpc.hpp"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

std::map<std::string, RpcFunction> &get_registry()
{
    static std::map<std::string, RpcFunction> registry;
    return registry;
}

FunctionRegisterer::FunctionRegisterer(const std::string &name, RpcFunction fn)
{
    get_registry()[name] = fn;
}

std::string dispatch_rpc(const std::string &request_json)
{
    json req = json::parse(request_json);
    std::string method = req.at("method").get<std::string>();
    json params = req.value("params", json::array());

    auto &registry = get_registry();
    auto it = registry.find(method);
    if (it == registry.end())
    {
        json err = {{"code", -32601}, {"message", "Method not found"}};
        json resp = {{"result", nullptr}, {"error", err}};
        return resp.dump();
    }

    std::string result = it->second(params);
    return result;
}