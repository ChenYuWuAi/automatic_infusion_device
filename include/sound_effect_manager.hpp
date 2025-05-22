// filepath: include/sound_effect_manager.hpp
#ifndef SOUND_EFFECT_MANAGER_HPP
#define SOUND_EFFECT_MANAGER_HPP

#include <thread>
#include <atomic>
#include <mutex>
#include <memory>
#include "linux_beep.h"

class SoundEffectManager
{
public:
    SoundEffectManager();
    ~SoundEffectManager();

    // 初始化蜂鸣器设备
    bool initialize(const char *device);

    // 播放音效
    void playSound(const note_t *song, size_t length);

    // 停止当前音效
    void stopAll();

private:
    int fd_{-1};
    std::thread songThread_;
    bool stop_ = false;
    std::mutex mtx_;
};

// 声明全局SoundEffectManager实例
extern std::shared_ptr<SoundEffectManager> g_soundEffectManager;

#endif // SOUND_EFFECT_MANAGER_HPP
