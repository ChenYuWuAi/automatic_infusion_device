#include <iostream>
#include <mqtt/client.h>
#include <string>
#include <chrono>
#include <thread>
#include <motor_controller.hpp>
#include <linux_beep.h>
#include <signal.h>

const std::string SERVER_ADDRESS("mqtt://tb.chenyuwuai.xyz:1883");
const std::string CLIENT_ID("cpp_subscriber");
const std::string TOPIC("v1/devices/me/rpc/request/+");
const std::string RESPONSE_TOPIC("v1/devices/me/rpc/response/");
const std::string USERNAME("exggelffk6ghaw2hqus8");

mqtt::client client(SERVER_ADDRESS, CLIENT_ID);
mqtt::client client_(SERVER_ADDRESS, CLIENT_ID + "_response");

bool running = true;

class MyCallback : public virtual mqtt::callback
{
public:
    void connection_lost(const std::string &cause) override
    {
        std::cout << "Connection lost: " << cause << std::endl;
    }

    void message_arrived(mqtt::const_message_ptr msg) override
    {
        std::string payload(msg->to_string());
        // 获取请求的ID
        std::string requestId = msg->get_topic().substr(msg->get_topic().find_last_of('/') + 1);
        std::cout << "Received RPC request " << requestId << ": " << payload << std::endl;

        std::string response = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());

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

    void delivery_complete(mqtt::delivery_token_ptr token) override {}
};

void on_sigINT(int signum)
{
    std::cout << "Interrupt signal (" << signum << ") received.\n";
    running = false;
}

int main()
{
    const int microPins[3] = {16, 17, 20};
    const char beep_device[] = "/dev/input/by-path/platform-1000120000.pcie:rp1:pwm_beeper_13-event";

    int beep_fd = getFD(beep_device);
    bool beep_stop = false;

    std::thread songThread(play_song_thread, beep_fd, buzzer_winxp, sizeof(buzzer_winxp) / sizeof(note_t), std::ref(beep_stop));
    songThread.detach();

    try
    {
        MotorController motor("gpiochip4", 27, microPins);
        motor.setControl(0, 4); // 示例：方向为0，细分控制值为4
        std::cout << "设置成功！" << std::endl;
    }
    catch (const std::exception &ex)
    {
        std::cerr << "错误：" << ex.what() << std::endl;
        return 1;
    }

    mqtt::connect_options connOpts;
    connOpts.set_user_name(USERNAME);

    MyCallback cb;
    // client.set_callback(cb);

    signal(SIGINT, on_sigINT);
    signal(SIGTERM, on_sigINT);

    try
    {
        client.connect(connOpts);
        client.subscribe(TOPIC, 1);
        std::cout << "Subscribed to RPC request topic!" << std::endl;

        // 保持连接，等待消息
        while (running)
        {
            // 使用 try_consume_message() 方法来获取消息
            mqtt::const_message_ptr msg;
            if (client.try_consume_message(&msg))
            {
                std::cout << "Received message: " << msg->to_string() << std::endl;
                cb.message_arrived(msg);
                // 发送响应
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
