#ifndef MOTOR_CONTROLLER_H
#define MOTOR_CONTROLLER_H

#include <gpiod.h>
#include <stdexcept>

/**
 * @brief 封装电机控制的类
 */
class MotorController {
public:
    /**
     * @brief 构造函数，初始化GPIO
     * @param chipname GPIO芯片名称（如"gpiochip0"）
     * @param dirPin 方向控制的GPIO编号（例如27）
     * @param microPins 三个位的GPIO编号数组（例如{16, 17, 20}）
     */
    MotorController(const char* chipname, int dirPin, const int microPins[3]);

    /**
     * @brief 析构函数，释放GPIO资源
     */
    ~MotorController();

    /**
     * @brief 设置方向以及细分控制
     * @param direction 方向控制，0或1（例如0：反转，1：正转）
     * @param microstep 3bit细分控制值（0~7），各bit分别对应细分控制I、II、III（低位为I）
     */
    void setControl(int direction, int microstep);

private:
    gpiod_chip* chip = nullptr;
    gpiod_line* dirLine = nullptr;
    gpiod_line* microLines[3] = {nullptr, nullptr, nullptr};
};

#endif // MOTOR_CONTROLLER_H