#ifndef BATTERY_MONITOR_HPP
#define BATTERY_MONITOR_HPP

#include <string>
#include <functional>
#include <chrono>
#include "logger.hpp"

/**
 * @brief 电池监控类，负责监控和管理UPS电池状态
 */
class BatteryMonitor {
public:
    /**
     * @brief 电池状态信息结构体
     */
    struct BatteryInfo {
        int capacity = 0;                // 电池电量百分比
        std::string status = "Unknown";  // 电池状态（如充电、放电）
        bool present = false;            // 电池是否存在
        double power = 0.0;              // 电池功率 (W)
        long long timeToFull = 0;        // 充满所需时间 (秒)
        long long timeToEmpty = 0;       // 放电剩余时间 (秒)
    };
    
public:
    /**
     * @brief 构造函数
     */
    BatteryMonitor();
    
    /**
     * @brief 更新电池状态
     * @return 是否成功更新
     */
    bool update();
    
    /**
     * @brief 设置低电量警告回调
     * @param callback 当电池电量低时调用的回调函数
     */
    void setLowBatteryCallback(std::function<void()> callback);
    
    /**
     * @brief 获取电池信息
     * @return 电池信息结构体
     */
    const BatteryInfo& getBatteryInfo() const;
    
    /**
     * @brief 获取电池电量
     * @return 电池电量百分比
     */
    int getBatteryLevel() const;
    
    /**
     * @brief 获取电池状态
     * @return 电池状态字符串 (Charging, Discharging, etc.)
     */
    std::string getBatteryStatus() const;
    
    /**
     * @brief 电池是否存在
     * @return 是否存在电池
     */
    bool isBatteryPresent() const;
    
    /**
     * @brief 获取当前剩余时间 (秒)
     * @return 剩余时间，充电时表示充满所需时间，放电时表示剩余电量可用时间
     */
    long long getCurrentStateRemainTime() const;
    
    /**
     * @brief 获取当前电池功率 (W)
     * @return 电池功率
     */
    double getPower() const;

private:
    // 电池文件路径
    const std::string capacityFile_ = "/sys/class/power_supply/rpi-ups-battery/capacity";
    const std::string statusFile_ = "/sys/class/power_supply/rpi-ups-battery/status";
    const std::string presentFile_ = "/sys/class/power_supply/rpi-ups-battery/present";
    const std::string powerFile_ = "/sys/class/power_supply/rpi-ups-battery/power_now";
    const std::string shutdownFile_ = "/sys/class/power_supply/rpi-ups-battery/device/shutdown";
    const std::string timeToFullFile_ = "/sys/class/power_supply/rpi-ups-battery/time_to_full_now";
    const std::string timeToEmptyFile_ = "/sys/class/power_supply/rpi-ups-battery/time_to_empty_now";
    
    // 电池状态
    BatteryInfo batteryInfo_;
    
    // 记录警告状态的变量
    static bool shutdown_warning_shown_;
    static bool shutdown_initiated_;
    static std::chrono::steady_clock::time_point last_warning_time_;
    
    // 低电量回调
    std::function<void()> lowBatteryCallback_ = nullptr;
    
    /**
     * @brief 读取电池状态
     * @return 是否成功读取
     */
    bool readBatteryStatus();
    
    /**
     * @brief 处理低电量警告
     */
    void handleLowBattery();
    
    /**
     * @brief 处理临界电量关机逻辑
     */
    void handleCriticalBattery();
};

#endif // BATTERY_MONITOR_HPP
