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
#include "liquid_detector.hpp"
#include <lccv.hpp>
#include <camera_lccv.hpp>

#include <memory>
#include <thread>
#include <atomic>
#include <vector>

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

std::atomic<bool> motor_thread_running{true};
std::atomic<bool> liquid_detector_thread_running{true};
std::atomic<bool> mqtt_thread_running{true};
std::atomic<bool> camera_thread_running{true};
std::atomic<double> liquid_level_percentage{-1.0}; // 无锁变量，用于线程间通信
std::atomic<bool> pump_params_updated{false};      // 标记泵参数是否更新

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

// std::string rpc_detectLiquid_fn(const json &params)   //添加
// {
//     std::string imagePath = params.value("image_path", "");
//     if (imagePath.empty()) {
//         return R"({"error": "No image path provided"})";
//     }

//     cv::Mat image = cv::imread(imagePath, cv::IMREAD_COLOR);
//     if (image.empty()) {
//         return R"({"error": "Image load failed"})";
//     }

//     double percentage = detectLiquidLevelPercentage(image);
//     if (percentage < 0) {
//         return R"({"error": "Liquid detection failed"})";
//     }

//     json response;
//     response["percentage"] = percentage;
//     response["result"] = "ok";
//     return response.dump();
// }

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
    pump_params_updated.store(true);
}

void on_sigINT(int signum)
{
    std::cout << "Interrupt signal (" << signum << ") received.\n";
    motor->setSpeed(0);
    running = false;
}

void motor_control_thread()
{
    while (motor_thread_running)
    {
        // 调用 pump_calibration 的流量计算逻辑并控制电机
        // motor->setSpeed(calculated_speed);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void liquid_detector_thread()
{
    while (liquid_detector_thread_running)
    {
        // 调用液位检测逻辑并处理结果
        // 推送到 MQTT 服务器
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void mqtt_message_thread()
{
    while (mqtt_thread_running)
    {
        mqtt::const_message_ptr msg;
        if (client.try_consume_message(&msg))
        {
            if (msg->get_topic().find("v1/devices/me/rpc/request/") != std::string::npos)
            {
                rpc_message_arrived(msg);
            }
            else if (msg->get_topic().find("v1/devices/me/attributes") != std::string::npos)
            {
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

void camera_thread()
{
    lccv::PiCamera cam;
    cam.options->width = 640;
    cam.options->height = 480;
    cam.options->framerate = 5;
    cam.start();

    while (camera_thread_running)
    {
        cv::Mat frame;
        cam.grab();
        cam.retrieve(frame);

        if (!frame.empty())
        {
            double percentage = detectLiquidLevelPercentage(frame, 100.0); // 假设总容量为100
            if (percentage >= 0)
            {
                liquid_level_percentage.store(percentage);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    cam.stop();
}

void control_thread()
{
    while (motor_thread_running)
    {
        if (pump_params_updated.exchange(false))
        {
            motor->setDirection(pumpParams.direction);
            motor->setSpeed(pumpParams.target_flow_rate);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void mqtt_thread()
{
    while (mqtt_thread_running)
    {
        mqtt::const_message_ptr msg;
        if (client.try_consume_message(&msg))
        {
            if (msg->get_topic().find("v1/devices/me/rpc/request/") != std::string::npos)
            {
                rpc_message_arrived(msg);
            }
            else if (msg->get_topic().find("v1/devices/me/attributes") != std::string::npos)
            {
                attribute_message_arrived(msg);
                pump_params_updated.store(true);
            }
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // 推送液位百分比到远程
        double percentage = liquid_level_percentage.load();
        if (percentage >= 0)
        {
            json payload = {{"liquid_level_percentage", percentage}};
            client.publish("v1/devices/me/telemetry", payload.dump());
        }
    }
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
        std::thread songThread(play_song_thread, beep_fd, buzzer_win10_plugin, sizeof(buzzer_win10_plugin) / sizeof(note_t), std::ref(beep_stop));
        songThread.detach();

        // 请求同步泵的属性
        // 请求共享属性 pump_flow_rate 和 pump_direction
        const std::string attrRequestTopic = "v1/devices/me/attributes/request/1";
        const std::string attrRequestPayload = R"({"sharedKeys":"pump_flow_rate,pump_direction"})";
        client.publish(attrRequestTopic, attrRequestPayload.c_str(), attrRequestPayload.length(), 0, false);
        std::cout << "Requested shared attributes: pump_flow_rate, pump_direction" << std::endl;

        std::thread motorThread(control_thread);
        std::thread mqttThread(mqtt_thread);
        std::thread cameraThread(camera_thread);

        // 主线程作为看门狗
        while (running)
        {
            if (!motor_thread_running || !mqtt_thread_running || !camera_thread_running)
            {
                std::cerr << "Error: One of the threads has stopped unexpectedly!" << std::endl;
                break;
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        // 停止所有线程
        motor_thread_running = false;
        mqtt_thread_running = false;
        camera_thread_running = false;

        motorThread.join();
        mqttThread.join();
        cameraThread.join();
    }
    catch (const mqtt::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    return 0;
}
