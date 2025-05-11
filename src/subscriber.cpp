#include <iostream>
#include <mqtt/client.h>
#include <string>
#include <chrono>
#include <thread>
#include <motor_controller.hpp>
#include <linux_beep.h>
#include <signal.h>
#include <rpc.hpp>
#include <pump_common.hpp>
#include <opencv2/opencv.hpp>
#include "liquid_test.hpp"

#include <memory>

const std::string SERVER_ADDRESS("mqtt://tb.chenyuwuai.xyz:1883");
const std::string CLIENT_ID("cpp_subscriber");
const std::string TOPIC("v1/devices/me/rpc/request/+");
const std::string TOPIC_ATTR("v1/devices/me/attributes");
const std::string RESPONSE_TOPIC("v1/devices/me/rpc/response/");
const std::string USERNAME("exggelffk6ghaw2hqus8");

mqtt::client client(SERVER_ADDRESS, CLIENT_ID);
mqtt::client client_(SERVER_ADDRESS, CLIENT_ID + "_response");

PumpParams pumpParams;
PumpState pumpState;

std::shared_ptr<MotorController> motor;

bool running = true;

// 定义并注册 add 方法
std::string add1_fn(const json &params)
{
    // 返回只有一个数字3的json
    return "true";
}

std::string add2_fn(const json &params)
{
    // 返回只有一个数字3的json
    return "1";
}

std::string rpc_powerState_fn(const json &params)
{
    // 如果是OK则调用setSpeed
    if (params == true)
    {
        // 设置电机转速
        motor->setSpeed(pumpParams.target_flow_rate);
    }
    else
    {
        // 停止电机
        motor->setSpeed(0);
    }
    // 把params原封不动塞进新的json
    json response_json;
    response_json["params"] = params;
    response_json["result"] = "ok";
    return response_json.dump();
}

std::string rpc_startPumpState_fn(const json &params)
{
    // 将当前的泵状态从全局变量更新到电机中

    // 设置电机转速
    motor->setSpeed(pumpParams.target_flow_rate);

    // 把params原封不动塞进新的json
    json response_json;
    response_json["params"] = params;
    response_json["result"] = "ok";
    return response_json.dump();
}

std::string rpc_detectLiquid_fn(const json &params)   //添加
{
    std::string imagePath = params.value("image_path", "");
    if (imagePath.empty()) {
        return R"({"error": "No image path provided"})";
    }

    cv::Mat image = cv::imread(imagePath, cv::IMREAD_COLOR);
    if (image.empty()) {
        return R"({"error": "Image load failed"})";
    }

    double percentage = detectLiquidLevelPercentage(image);
    if (percentage < 0) {
        return R"({"error": "Liquid detection failed"})";
    }

    json response;
    response["percentage"] = percentage;
    response["result"] = "ok";
    return response.dump();
}

// 添加注册新的 RPC 方法
static FunctionRegisterer reg_detectLiquid("detectLiquidLevel", rpc_detectLiquid_fn);

static FunctionRegisterer reg_add("getPowerState", add1_fn);
static FunctionRegisterer reg_knobValue("getKnobValue", add2_fn);
static FunctionRegisterer reg_setPowerState("setPumpPower", rpc_powerState_fn);

void rpc_message_arrived(mqtt::const_message_ptr msg)
{
    std::string payload(msg->to_string());

    // 获取请求的ID，函数名称
    std::string requestId = msg->get_topic().substr(msg->get_topic().find_last_of('/') + 1);
    std::cout << "Received RPC request " << requestId << ": " << payload << std::endl;

    std::string response = dispatch_rpc(payload);
    std::cout << "Resp: " << response << std::endl;

    try
    {
        if (!client.is_connected())
        {
            std::cerr << "Client is not connected. Reconnecting..." << std::endl;
            client.reconnect();
        }
        // 使用全局的 client 实例发送响应
        std::cout << "Sending RPC response " << requestId << ": " << response << " to " << RESPONSE_TOPIC + requestId << std::endl;
        client.publish(RESPONSE_TOPIC + requestId, response.c_str(), response.length(), 0, false);
        std::cout << "RPC response sent!" << std::endl;
    }
    catch (const mqtt::exception &e)
    {
        std::cerr << "Error sending response: " << e.what() << std::endl;
    }
}

void attribute_message_arrived(mqtt::const_message_ptr msg)
{
    std::string payload(msg->to_string());

    
    // 获取参数名称并同步到全局变量
    json request_json = json::parse(payload);
    // 如果有"shared" 则进入"shared"
    if (request_json.contains("shared"))
    {
        request_json = request_json["shared"];
    }
    
    for (const auto &item : request_json.items())
    {
        std::string key = item.key();
        if (key == "pump_direction")
        {
            pumpParams.direction = item.value();
        }
        else if (key == "pump_flow_rate")
        {
            pumpParams.target_flow_rate = std::stod(std::string(item.value()));
        }
    }
}

void on_sigINT(int signum)
{
    std::cout << "Interrupt signal (" << signum << ") received.\n";
    motor->setSpeed(0);
    running = false;
}

int main()
{
    const int microPins[3] = {16, 17, 20};
    const char beep_device[] = "/dev/input/by-path/platform-1000120000.pcie:rp1:pwm_beeper_13-event";

    int beep_fd = getFD(beep_device);
    bool beep_stop = false;

    try
    {
        motor = std::make_shared<MotorController>("gpiochip4", 27, microPins, "/dev/input/by-path/platform-1000120000.pcie:rp1:pwm_beeper_19-event");
        motor->setDirection(0);
        motor->setSpeed(0);
        std::cout << "电机设置成功！" << std::endl;
    }
    catch (const std::exception &ex)
    {
        std::cerr << "错误：" << ex.what() << std::endl;
        return 1;
    }

    mqtt::connect_options connOpts;
    connOpts.set_user_name(USERNAME);

    signal(SIGINT, on_sigINT);
    signal(SIGTERM, on_sigINT);

    try
    {
        client.connect(connOpts);
        client.subscribe(TOPIC, 1);
        client.subscribe(TOPIC_ATTR, 1);
        client.subscribe("v1/devices/me/attributes/response/+", 1);

        std::cout << "Subscribed to RPC request topic!" << std::endl;
        // std::thread songThread(play_song_thread, beep_fd, buzzer_winxp, sizeof(buzzer_winxp) / sizeof(note_t), std::ref(beep_stop));
        // songThread.detach();

        // 请求同步泵的属性
        // 请求共享属性 pump_flow_rate 和 pump_direction
        const std::string attrRequestTopic = "v1/devices/me/attributes/request/1";
        const std::string attrRequestPayload = R"({"sharedKeys":"pump_flow_rate,pump_direction"})";
        client.publish(attrRequestTopic, attrRequestPayload.c_str(), attrRequestPayload.length(), 0, false);
        std::cout << "Requested shared attributes: pump_flow_rate, pump_direction" << std::endl;

        // 保持连接，等待消息
        while (running)
        {
            // 使用 try_consume_message() 方法来获取消息
            mqtt::const_message_ptr msg;
            if (client.try_consume_message(&msg))
            {
                std::cout << "Received message: " << msg->to_string() << std::endl;
                // 发送响应
                // 如果内容有"method" "params"字段则调用rpc_message_arrived
                if (msg->get_topic().find("v1/devices/me/rpc/request/") != std::string::npos)
                {
                    rpc_message_arrived(msg);
                }
                else if (msg->get_topic().find("v1/devices/me/attributes") != std::string::npos)
                {
                    // 处理属性消息
                    std::cout << "Received attribute message: " << msg->to_string() << std::endl;
                    attribute_message_arrived(msg);
                }
                else
                {
                    std::cout << "Unknown message topic: " << msg->get_topic() << std::endl;
                }
            }
            else
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    }
    catch (const mqtt::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    return 0;
}
