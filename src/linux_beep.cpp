#include "linux_beep.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <thread>
#include <chrono>

// 定义全局停止标志指针
volatile bool *gStop = nullptr;

void play_beep(int fd, int frequency, int duration)
{
    struct input_event beep_event;

    // 设置频率 (频率单位可能需要适应不同的硬件)
    memset(&beep_event, 0, sizeof(beep_event));
    beep_event.type = EV_SND;
    beep_event.code = SND_TONE;
    beep_event.value = frequency;
    write(fd, &beep_event, sizeof(beep_event));

    // 等待指定时长
    std::this_thread::sleep_for(std::chrono::milliseconds(duration));

    // 停止蜂鸣器
    beep_event.value = 0;
    write(fd, &beep_event, sizeof(beep_event));
}

void stop_beep(int fd)
{
    struct input_event beep_event = {0};
    write(fd, &beep_event, sizeof(beep_event));
}

int getFD(const char *device)
{
    int fd = open(device, O_RDWR);
    if (fd == -1)
    {
        perror("无法打开设备文件");
        return -1;
    }
    return fd;
}

void signal_handler(int signum)
{
    if (gStop)
        *gStop = true;
}

void play_song_thread(int fd, note_t notes_to_play[], int notes_count, bool &stop)
{
    // 不在线程中设置信号处理器，而是由主线程设置
    if (gStop != nullptr)
        *gStop = true;
    gStop = &stop;
    // 移除这里的信号处理器设置，避免与主线程信号处理冲突
    // signal(SIGINT, signal_handler);
    // signal(SIGTERM, signal_handler);
    for (int i = 0; i < notes_count; i++)
    {
        if (stop)
            break;

        // 如果frequency为0则表示休止符，只等待duration的时间
        if (notes_to_play[i].pitch > 0)
        {
            play_beep(fd, notes_to_play[i].pitch, notes_to_play[i].duration);
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(notes_to_play[i].duration));
        }

        // 两个音符间加入短暂停顿（例如5毫秒），可根据需要调整
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}