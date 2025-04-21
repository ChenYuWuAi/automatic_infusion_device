#include <iostream>
#include <mqtt/client.h>
#include <string>
#include <random>

#include <signal.h>

const std::string SERVER_ADDRESS("tcp://tb.chenyuwuai.xyz:1883");
const std::string CLIENT_ID("cpp_publisher");
const std::string TOPIC("v1/devices/me/telemetry");
const std::string USERNAME("exggelffk6ghaw2hqus8");
const std::string PAYLOAD(R"({"temperature": 25})");

bool running = true;

void on_sigINT(int signum) {
    std::cout << "Interrupt signal (" << signum << ") received.\n";
    running = false;
}

// int main() {
//     mqtt::client client(SERVER_ADDRESS, CLIENT_ID);
//     mqtt::connect_options connOpts;

//     connOpts.set_clean_session(true);
//     connOpts.set_user_name(USERNAME);

//     signal(SIGINT, on_sigINT);
//     signal(SIGTERM, on_sigINT);

//     try {
//         client.connect(connOpts);
//         while (running)
//         {
//             std::this_thread::sleep_for(std::chrono::seconds(1));
//             std::string payload = R"({"temperature": )" + std::to_string(rand() % 100) + R"(})";
//             std::cout << "Publishing message: " << payload << std::endl;
//             client.publish(TOPIC, payload.c_str(), payload.length(), 1, false);
//         }
//         client.disconnect();
//     } catch (const mqtt::exception& e) {
//         std::cerr << "Error: " << e.what() << std::endl;
//     }

//     return 0;
// }
