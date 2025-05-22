#ifndef LINUX_BEEP_H
#define LINUX_BEEP_H

#include <linux/input.h>
#include <unistd.h>
#include <signal.h>
#include <thread>
#include "buzzer_songs.h"

/**
 * @brief 播放蜂鸣器音符
 * @param fd 设备文件描述符
 * @param frequency 音符频率（Hz）
 * @param duration 音符持续时间（毫秒）
 */
void play_beep(int fd, int frequency, int duration);

/**
 * @brief 停止蜂鸣器
 * @param fd 设备文件描述符
 */
void stop_beep(int fd);

/**
 * @brief 打开设备文件并返回文件描述符
 * @param device 设备文件路径
 * @return 文件描述符，失败时返回-1
 */
int getFD(const char *device);

/**
 * @brief 播放音符数组的线程函数
 * @param fd 设备文件描述符
 * @param notes_to_play 音符数组
 * @param stop 停止标志
 */
void play_song_thread(int fd, const note_t notes_to_play[], int notes_count, bool &stop);

#endif // LINUX_BEEP_H