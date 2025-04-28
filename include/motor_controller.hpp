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
    MotorController(const char *chipname, int dirPin, const int microPins[3], const char *motor_pwm_dev);

    /**
     * @brief 析构函数，释放GPIO资源
     */
    ~MotorController();

    /**
     * @brief 单独设置方向
     */
    void setDirection(int direction);

    /**
     * @brief 单独设置细分控制
     */
    void setMicrostep(int microstep);

    /**
     * @brief 获取当前方向
     * @return 方向值，0或1
     */
    int getDirection() const;

    /**
     * @brief 获取当前细分控制值
     * @return 细分控制值，0~7
     */
    int getMicrostep() const;

    /**
     * @brief 设置目标转速
     * @param speed 转速值rpm
     */
    void setSpeed(double speed);

private:
    gpiod_chip* chip = nullptr;
    gpiod_line* dirLine = nullptr;
    gpiod_line* microLines[3] = {nullptr, nullptr, nullptr};
    int currentDirection = 0; // 当前方向
    int currentMicrostep = 0; // 当前细分控制值

    int motor_fd;
};

#endif // MOTOR_CONTROLLER_H