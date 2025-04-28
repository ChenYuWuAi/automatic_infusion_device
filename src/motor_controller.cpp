#include "motor_controller.hpp"
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <linux/input.h>

MotorController::MotorController(const char *chipname, int dirPin, const int microPins[3], const char *motor_pwm_dev)
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
    // 开启PWM
    motor_fd = open(motor_pwm_dev, O_RDWR);
}

MotorController::~MotorController()
{
    if (chip)
    {
        gpiod_chip_close(chip);
    }
}

void MotorController::setDirection(int direction)
{
    if (direction != 0 && direction != 1)
    {
        throw std::invalid_argument("direction必须为0或1");
    }
    this->currentDirection = direction;
    // 设置方向
    if (gpiod_line_set_value(dirLine, direction) < 0)
    {
        throw std::runtime_error("设置方向GPIO失败");
    }
}

void MotorController::setMicrostep(int microstep)
{

    // 按照真值表转换
    // 000 -> 1
    // 001 -> 2
    // 010 -> 4
    // 011 -> 8
    // 100 -> 16
    // 101 -> 32
    int microstep_value = 0;
    switch (microstep)
    {
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
        throw std::invalid_argument("microstep值范围必须在1 2 4 8 16 32之间");
    }
    this->currentMicrostep = microstep;

    // 分别设置3个位的细分控制
    for (int i = 0; i < 3; i++)
    {
        int bit = (microstep_value >> i) & 0x1;
        if (gpiod_line_set_value(microLines[i], bit) < 0)
        {
            throw std::runtime_error("设置细分控制GPIO失败");
        }
    }
}

void MotorController::setSpeed(double speed)
{
    // 速度最大值： 150rpm
    // 最小：0.009375rpm
    // 频率最高不超过500
    // 计算频率
    if (abs(speed) <= 0.009375)
    {
        // 停止电机
        struct input_event stop_event;
        memset(&stop_event, 0, sizeof(stop_event));
        stop_event.type = EV_SND;
        stop_event.code = SND_TONE;
        stop_event.value = 0;
        write(motor_fd, &stop_event, sizeof(stop_event));
        return;
    }

    double frequency = 0;

    // 根据speed的rpm决定细分值
    static int microstep[] = {32, 16, 8, 4, 2, 1};

    int i = 0;
    for (; i < sizeof(microstep) / sizeof(microstep[0]); i++)
    {
        frequency = abs(speed) * 6 * microstep[i] / 1.8;
        if (frequency < 500 / microstep[i])
        {
            setMicrostep(microstep[i]);
            break;
        }
    }

    if (i == sizeof(microstep) / sizeof(microstep[0]))
    {
        setMicrostep(microstep[i - 1]);
    }

    setDirection(speed > 0 ? 1 : 0);

    struct input_event beep_event;

    // 设置频率 (频率单位可能需要适应不同的硬件)
    memset(&beep_event, 0, sizeof(beep_event));
    beep_event.type = EV_SND;
    beep_event.code = SND_TONE;
    beep_event.value = int(frequency);
    write(motor_fd, &beep_event, sizeof(beep_event));
}
