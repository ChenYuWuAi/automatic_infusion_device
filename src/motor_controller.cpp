#include "motor_controller.hpp"
#include <iostream>

MotorController::MotorController(const char *chipname, int dirPin, const int microPins[3])
{
    chip = gpiod_chip_open_by_name(chipname);
    if (!chip)
    {
        throw std::runtime_error("打开GPIO芯片失败");
    }
    // 获取方向控制引脚
    dirLine = gpiod_chip_get_line(chip, dirPin);
    if (!dirLine)
    {
        gpiod_chip_close(chip);
        throw std::runtime_error("获取方向GPIO失败");
    }
    if (gpiod_line_request_output(dirLine, "MotorController", 0) < 0)
    {
        gpiod_chip_close(chip);
        throw std::runtime_error("请求方向GPIO输出模式失败");
    }
    // 获取细分控制引脚
    for (int i = 0; i < 3; i++)
    {
        microLines[i] = gpiod_chip_get_line(chip, microPins[i]);
        if (!microLines[i])
        {
            gpiod_chip_close(chip);
            throw std::runtime_error("获取细分控制GPIO失败");
        }
        if (gpiod_line_request_output(microLines[i], "MotorController", 0) < 0)
        {
            gpiod_chip_close(chip);
            throw std::runtime_error("请求细分控制GPIO输出模式失败");
        }
    }
}

MotorController::~MotorController()
{
    if (chip)
    {
        gpiod_chip_close(chip);
    }
}

void MotorController::setControl(int direction, int microstep)
{
    if (direction != 0 && direction != 1)
    {
        throw std::invalid_argument("direction必须为0或1");
    }
    if (microstep < 0 || microstep > 7)
    {
        throw std::invalid_argument("microstep值范围必须在0~7之间");
    }
    // 设置方向
    if (gpiod_line_set_value(dirLine, direction) < 0)
    {
        throw std::runtime_error("设置方向GPIO失败");
    }
    // 分别设置3个位的细分控制
    for (int i = 0; i < 3; i++)
    {
        int bit = (microstep >> i) & 0x1;
        if (gpiod_line_set_value(microLines[i], bit) < 0)
        {
            throw std::runtime_error("设置细分控制GPIO失败");
        }
    }
}