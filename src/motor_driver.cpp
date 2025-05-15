#include "motor_driver.hpp"
#include "logger.hpp"
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <linux/input.h>
#include <thread>
#include <chrono>

MotorDriver::MotorDriver(const char *chipname, int dirPin, const int microPins[3], const char *motorPwmDevice,
                         PumpState &pumpState)
        : chipname_(chipname), dirPin_(dirPin), motorPwmDevice_(motorPwmDevice), pumpState_(pumpState) {
    // 复制细分控制引脚数组
    for (int i = 0; i < 3; i++) {
        microPins_[i] = microPins[i];
    }
}

MotorDriver::~MotorDriver() {
    stopControlThread();

    // 关闭PWM设备
    if (motor_fd_ >= 0) {
        close(motor_fd_);
    }

    // 释放GPIO资源
    if (chip_) {
        gpiod_chip_close(chip_);
    }
}

bool MotorDriver::initialize() {
    try {
        // 打开GPIO芯片
        chip_ = gpiod_chip_open_by_name(chipname_);
        if (!chip_) {
            InfusionLogger::error("打开GPIO芯片失败");
            return false;
        }

        // 获取方向控制引脚
        dirLine_ = gpiod_chip_get_line(chip_, dirPin_);
        if (!dirLine_) {
            InfusionLogger::error("获取方向GPIO失败");
            return false;
        }

        if (gpiod_line_request_output(dirLine_, "MotorDriver", 0) < 0) {
            InfusionLogger::error("请求方向GPIO输出模式失败");
            return false;
        }

        // 获取细分控制引脚
        for (int i = 0; i < 3; i++) {
            microLines_[i] = gpiod_chip_get_line(chip_, microPins_[i]);
            if (!microLines_[i]) {
                InfusionLogger::error("获取细分控制GPIO失败");
                return false;
            }

            if (gpiod_line_request_output(microLines_[i], "MotorDriver", 0) < 0) {
                InfusionLogger::error("请求细分控制GPIO输出模式失败");
                return false;
            }
        }

        // 开启PWM
        motor_fd_ = open(motorPwmDevice_, O_RDWR);
        if (motor_fd_ < 0) {
            InfusionLogger::error("打开电机PWM设备失败");
            return false;
        }

        // 初始设置
        setDirection(0);
        setSpeed(0);

        return true;
    } catch (const std::exception &e) {
        InfusionLogger::error("初始化电机驱动时出错: {}", e.what());
        return false;
    }
}

void MotorDriver::setDirection(int direction) {
    if (direction != 0 && direction != 1) {
        InfusionLogger::warn("方向值必须为0或1，收到: {}", direction);
        return;
    }

    if (dirLine_ == nullptr) {
        InfusionLogger::error("方向GPIO未初始化");
        return;
    }

    currentDirection_ = direction;

    // 设置方向
    if (gpiod_line_set_value(dirLine_, direction) < 0) {
        InfusionLogger::error("设置方向GPIO失败");
    } else {
        InfusionLogger::debug("电机方向已设置为: {}", direction);
    }
}

void MotorDriver::setMicrostep(int microstep) {
    // 按照真值表转换
    // 000 -> 1
    // 001 -> 2
    // 010 -> 4
    // 011 -> 8
    // 100 -> 16
    // 101 -> 32
    int microstep_value = 0;
    switch (microstep) {
        case 1:
            microstep_value = 0;
            break;
        case 2:
            microstep_value = 1;
            break;
        case 4:
            microstep_value = 2;
            break;
        case 8:
            microstep_value = 3;
            break;
        case 16:
            microstep_value = 4;
            break;
        case 32:
            microstep_value = 5;
            break;
        default:
            InfusionLogger::warn("microstep值范围必须在1 2 4 8 16 32之间，收到: {}", microstep);
            return;
    }

    currentMicrostep_ = microstep;

    // 分别设置3个位的细分控制
    for (int i = 0; i < 3; i++) {
        int bit = (microstep_value >> i) & 0x1;
        if (gpiod_line_set_value(microLines_[i], bit) < 0) {
            InfusionLogger::error("设置细分控制GPIO失败");
        }
    }

    InfusionLogger::debug("电机细分已设置为: {}", microstep);
}

int MotorDriver::getDirection() const {
    return currentDirection_;
}

int MotorDriver::getMicrostep() const {
    return currentMicrostep_;
}

void MotorDriver::setSpeed(double speed) {
    // 更新当前速度
    current_speed_ = speed;

    // 速度最大值： 150rpm
    // 最小：0.009375rpm
    // 频率最高不超过500
    if (abs(speed) <= 0.009375) {
        // 停止电机
        struct input_event stop_event;
        memset(&stop_event, 0, sizeof(stop_event));
        stop_event.type = EV_SND;
        stop_event.code = SND_TONE;
        stop_event.value = 0;
        write(motor_fd_, &stop_event, sizeof(stop_event));
        InfusionLogger::debug("电机已停止");
        return;
    }

    double frequency = 0;

    // 根据speed的rpm决定细分值
    static int microstep[] = {32, 16, 8, 4, 2, 1};

    int i = 0;
    for (; i < sizeof(microstep) / sizeof(microstep[0]); i++) {
        frequency = abs(speed) * 6 * microstep[i] / 1.8;
        if (frequency < 500 / microstep[i]) {
            setMicrostep(microstep[i]);
            break;
        }
    }

    if (i == sizeof(microstep) / sizeof(microstep[0])) {
        setMicrostep(microstep[i - 1]);
    }

    setDirection(speed > 0 ? 1 : 0);

    struct input_event beep_event;

    // 设置频率 (频率单位可能需要适应不同的硬件)
    memset(&beep_event, 0, sizeof(beep_event));
    beep_event.type = EV_SND;
    beep_event.code = SND_TONE;
    beep_event.value = int(frequency);
    write(motor_fd_, &beep_event, sizeof(beep_event));

    InfusionLogger::debug("电机速度已设置为: {}rpm，频率: {}Hz", speed, frequency);
}

double MotorDriver::getSpeed() const {
    return current_speed_;
}

void MotorDriver::startControlThread(PumpParams &pumpParams, std::atomic<bool> &paramsUpdatedFlag) {
    if (control_thread_running_.load()) {
        InfusionLogger::warn("电机控制线程已经在运行！");
        return;
    }

    control_thread_running_ = true;
    std::thread thread(&MotorDriver::controlThread, this, std::ref(pumpParams), std::ref(paramsUpdatedFlag));
    thread.detach();
}

void MotorDriver::stopControlThread() {
    control_thread_running_ = false;
    // 给线程一些时间来完成当前操作
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // 确保电机停止
    setSpeed(0);
}

bool MotorDriver::isControlThreadRunning() const {
    return control_thread_running_.load();
}

void MotorDriver::controlThread(PumpParams &pumpParams, std::atomic<bool> &paramsUpdatedFlag) {
    InfusionLogger::info("电机控制线程已启动");

    // 用于紧急停止的时间控制
    auto emergencyStopStartTime = std::chrono::steady_clock::now();
    bool emergencyStopReverseDone = false;

    while (control_thread_running_) {
        try {
            // 获取当前泵状态
            PumpControlState currentState = pumpState_.state.load();

            // 根据不同状态控制电机行为
            switch (currentState) {
                case IDLE:
                    // 空闲状态，电机不运行
                    setSpeed(0);
                    break;

                case VERIFY_PENDING:
                case VERIFIED:
                    // 等待验证或已验证状态，不运行
                    setSpeed(0);
                    break;

                case PREPARING:
                    // 准备状态，正向运行排空气
                    setDirection(true); // 正向
                    setSpeed(pumpParams.target_rpm); // 使用目标转速
                    break;

                case INFUSING:
                    // 输液状态，正向运行，使用参数中的方向和转速
                    if (paramsUpdatedFlag.exchange(false)) {
                        // 更新电机方向
                        setDirection(pumpParams.direction);
                        // 更新电机速度
                        setSpeed(pumpParams.target_rpm);

                        InfusionLogger::info("电机参数已更新: 方向={}, 转速={} RPM",
                                             pumpParams.direction ? "正向" : "反向",
                                             pumpParams.target_rpm.load());
                    }
                    break;

                case PAUSED:
                    // 暂停状态，停止运行
                    setSpeed(0);
                    break;

                case EMERGENCY_STOP:
                    // 紧急停止状态，先反转一下，然后停止
                    if (!emergencyStopReverseDone) {
                        // 开始紧急停止时的反转
                        setDirection(!pumpParams.direction);
                        setSpeed(5.0); // 低速反转
                        emergencyStopStartTime = std::chrono::steady_clock::now();
                        emergencyStopReverseDone = true;
                        InfusionLogger::warn("紧急停止: 开始反转");
                    } else {
                        // 检查是否反转时间已到
                        auto now = std::chrono::steady_clock::now();
                        auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                now - emergencyStopStartTime).count();

                        if (elapsedMs >= 500) { // 反转0.5秒后停止
                            setSpeed(0);
                            InfusionLogger::warn("紧急停止: 电机已停止");
                        }
                    }
                    break;

                case ERROR:
                    // 错误状态，不运行
                    setSpeed(0);
                    break;

                default:
                    // 未知状态，安全起见不运行
                    setSpeed(0);
                    InfusionLogger::warn("未知的泵状态: {}", static_cast<int>(currentState));
                    break;
            }

            // 更新泵状态中的电机实际值
            pumpState_.current_speed.store(current_speed_);
            pumpState_.direction.store(currentDirection_ > 0);

            // 控制周期
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        } catch (const std::exception &e) {
            InfusionLogger::error("电机控制线程出错: {}", e.what());
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }

    // 确保线程结束时电机停止
    setSpeed(0);
    InfusionLogger::info("电机控制线程已停止");
}
