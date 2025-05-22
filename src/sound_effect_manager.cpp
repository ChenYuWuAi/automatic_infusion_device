// filepath: src/sound_effect_manager.cpp
#include "sound_effect_manager.hpp"
#include "logger.hpp"

std::shared_ptr<SoundEffectManager> g_soundEffectManager = nullptr;

SoundEffectManager::SoundEffectManager() {}

SoundEffectManager::~SoundEffectManager()
{
    stopAll();
    if (songThread_.joinable())
    {
        songThread_.join();
    }
}

bool SoundEffectManager::initialize(const char *device)
{
    std::lock_guard<std::mutex> lock(mtx_);
    fd_ = getFD(device);
    if (fd_ < 0)
    {
        InfusionLogger::error("无法打开蜂鸣器设备: {}", device);
        return false;
    }
    return true;
}

void SoundEffectManager::stopAll()
{
    std::lock_guard<std::mutex> lock(mtx_);
    if (fd_ >= 0)
    {
        stop_ = true;
        stop_beep(fd_);
    }
}

void SoundEffectManager::playSound(const note_t *song, size_t length)
{
    std::lock_guard<std::mutex> lock(mtx_);
    if (fd_ < 0)
        return;
    InfusionLogger::debug("正在播放音效...");
    // 停止当前音效
    stop_ = true;
    stop_beep(fd_);
    if (songThread_.joinable())
    {
        songThread_.join();
    }
    // 播放新音效
    stop_ = false;
    songThread_ = std::thread(play_song_thread, fd_, static_cast<const note_t *>(song), static_cast<int>(length), std::ref(stop_));
}
