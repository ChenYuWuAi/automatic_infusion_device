#include "battery_monitor.hpp"
#include <fstream>
#include <sstream>

// 静态成员初始化
bool BatteryMonitor::shutdown_warning_shown_ = false;
bool BatteryMonitor::shutdown_initiated_ = false;
std::chrono::steady_clock::time_point BatteryMonitor::last_warning_time_ = std::chrono::steady_clock::now();

BatteryMonitor::BatteryMonitor()
{
    readBatteryStatus();
}

bool BatteryMonitor::update()
{
    if (!readBatteryStatus())
    {
        return false;
    }

    // 处理低电量和临界电量逻辑
    if (batteryInfo_.status != "Charging" && batteryInfo_.present)
    {
        // 低电量警告
        if (batteryInfo_.capacity <= 10)
        {
            handleLowBattery();
        }

        // 临界电量关机逻辑
        if (batteryInfo_.capacity <= 7)
        {
            handleCriticalBattery();
        }
    }

    return true;
}

void BatteryMonitor::setLowBatteryCallback(std::function<void()> callback)
{
    lowBatteryCallback_ = callback;
}

int BatteryMonitor::getBatteryLevel() const
{
    return batteryInfo_.capacity;
}

std::string BatteryMonitor::getBatteryStatus() const
{
    return batteryInfo_.status;
}

bool BatteryMonitor::isBatteryPresent() const
{
    return batteryInfo_.present;
}

bool BatteryMonitor::readBatteryStatus()
{
    try
    {
        // 检查必须文件是否存在
        std::ifstream capStream(capacityFile_);
        if (!capStream.is_open())
        {
            InfusionLogger::error("电池容量文件不存在: {}", capacityFile_);
            return false;
        }

        // 读取电池容量
        std::string capStr;
        std::getline(capStream, capStr);
        batteryInfo_.capacity = std::stoi(capStr);

        // 读取电池状态
        batteryInfo_.status = "Unknown";
        std::ifstream statusStream(statusFile_);
        if (statusStream.is_open())
        {
            std::getline(statusStream, batteryInfo_.status);
        }
        else
        {
            InfusionLogger::warn("电池状态文件不存在，无法检查充电状态");
        }

        // 读取电池是否存在
        batteryInfo_.present = false;
        std::ifstream presentStream(presentFile_);
        if (presentStream.is_open())
        {
            std::string presentStr;
            std::getline(presentStream, presentStr);
            batteryInfo_.present = (presentStr != "0");
        }

        // 读取功率
        batteryInfo_.power = 0.0;
        std::ifstream powerStream(powerFile_);
        if (powerStream.is_open())
        {
            std::string powerStr;
            std::getline(powerStream, powerStr);
            batteryInfo_.power = std::stod(powerStr) / 1000000.0; // 转换为 W
        }

        // 读取时间信息
        batteryInfo_.timeToFull = 0;
        std::ifstream timeToFullStream(timeToFullFile_);
        if (timeToFullStream.is_open())
        {
            std::string timeStr;
            std::getline(timeToFullStream, timeStr);
            try
            {
                batteryInfo_.timeToFull = std::stoll(timeStr);
            }
            catch (const std::exception &)
            {
                batteryInfo_.timeToFull = 0;
            }
        }

        batteryInfo_.timeToEmpty = 0;
        std::ifstream timeToEmptyStream(timeToEmptyFile_);
        if (timeToEmptyStream.is_open())
        {
            std::string timeStr;
            std::getline(timeToEmptyStream, timeStr);
            try
            {
                batteryInfo_.timeToEmpty = std::stoll(timeStr);
            }
            catch (const std::exception &)
            {
                batteryInfo_.timeToEmpty = 0;
            }
        }

        return true;
    }
    catch (const std::exception &e)
    {
        InfusionLogger::error("读取电池状态时出错: {}", e.what());
        return false;
    }
}

void BatteryMonitor::handleLowBattery()
{
    InfusionLogger::warn("UPS警告: 电池电量低 ({}%). 请连接充电器。", batteryInfo_.capacity);

    // 如果设置了回调，执行回调
    if (lowBatteryCallback_)
    {
        lowBatteryCallback_();
    }
}

void BatteryMonitor::handleCriticalBattery()
{
    auto current_time = std::chrono::steady_clock::now();

    if (!shutdown_warning_shown_)
    {
        InfusionLogger::critical("UPS临界警告: 电池电量极低 ({}%). 如果不在30秒内连接充电器，系统将关机。", batteryInfo_.capacity);
        shutdown_warning_shown_ = true;
        last_warning_time_ = current_time;
    }

    // 检查是否已经过了30秒
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(current_time - last_warning_time_).count();
    if (elapsed >= 30 && !shutdown_initiated_)
    {
        shutdown_initiated_ = true;

        // 重新读取电池状态
        int newCapacity = 0;
        std::string newStatus = "Unknown";
        bool newPresent = false;

        std::ifstream newCapStream(capacityFile_);
        if (newCapStream.is_open())
        {
            std::string newCapStr;
            std::getline(newCapStream, newCapStr);
            newCapacity = std::stoi(newCapStr);
        }

        std::ifstream newStatusStream(statusFile_);
        if (newStatusStream.is_open())
        {
            std::getline(newStatusStream, newStatus);
        }

        std::ifstream newPresentStream(presentFile_);
        if (newPresentStream.is_open())
        {
            std::string newPresentStr;
            std::getline(newPresentStream, newPresentStr);
            newPresent = (newPresentStr != "0");
        }

        // 如果电量仍然低且未充电，执行关机
        if (newCapacity <= 7 && newStatus != "Charging" && newPresent)
        {
            InfusionLogger::critical("UPS: 电池电量仍然极低 ({}%). 正在启动关机程序。", newCapacity);

            // 向 UPS 设备发送关机命令
            std::ofstream shutdownStream(shutdownFile_);
            if (shutdownStream.is_open())
            {
                shutdownStream << "shutdown";
            }

            // 调用系统关机命令
            system("systemctl poweroff");
        }
    }
}

long long BatteryMonitor::getCurrentStateRemainTime() const
{
    if (batteryInfo_.status == "Charging")
    {
        return batteryInfo_.timeToFull;
    }
    else
    {
        return batteryInfo_.timeToEmpty;
    }
}

double BatteryMonitor::getPower() const
{
    return batteryInfo_.power;
}

const BatteryMonitor::BatteryInfo &BatteryMonitor::getBatteryInfo() const
{
    return batteryInfo_;
}
