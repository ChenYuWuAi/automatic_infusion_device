#include "camera_manager.hpp"
#include "logger.hpp"
#include "liquid_detector.hpp"
#include <camera_hal/camera_lccv.hpp>
#include <thread>

CameraManager::CameraManager() {
}

CameraManager::~CameraManager() {
    stopProcessing();
}

bool CameraManager::initialize(int width, int height, int framerate) {
    try {
        camera_driver_ = std::make_shared<CameraHAL::CameraDriver_LCCV>();
        
        std::unordered_map<std::string, std::string> camera_params;
        camera_params["Width"] = std::to_string(width);
        camera_params["Height"] = std::to_string(height);
        camera_params["Framerate"] = std::to_string(framerate);
        
        if (!camera_driver_->open(camera_params)) {
            InfusionLogger::error("无法打开相机！");
            return false;
        }
        
        return true;
    } catch (const std::exception& e) {
        InfusionLogger::error("初始化相机时出错: {}", e.what());
        return false;
    }
}

void CameraManager::startProcessing() {
    if (camera_thread_running_.load()) {
        InfusionLogger::warn("相机处理线程已经在运行！");
        return;
    }
    
    camera_thread_running_ = true;
    std::thread thread(&CameraManager::cameraThread, this);
    thread.detach(); // 分离线程
}

void CameraManager::stopProcessing() {
    camera_thread_running_ = false;
    // 给线程一些时间来完成当前操作
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
}

double CameraManager::getLiquidLevelPercentage() const {
    return liquid_level_percentage_.load();
}

bool CameraManager::isRunning() const {
    return camera_thread_running_.load();
}

void CameraManager::cameraThread() {
    InfusionLogger::info("相机处理线程已启动");
    
    while (camera_thread_running_) {
        try {
            cv::Mat frame;
            if (!camera_driver_->read(frame)) {
                InfusionLogger::error("无法从相机读取帧！");
                std::this_thread::sleep_for(std::chrono::milliseconds(500)); // 出错时稍微等待长一点
                continue;
            }
            
            if (!frame.empty()) {
                // 假设总容量为100（可以在后续版本中参数化）
                double percentage = detectLiquidLevelPercentage(frame, 100.0);
                if (percentage >= 0) {
                    liquid_level_percentage_.store(percentage);
                }
            }
            
            // 控制帧率，避免过度占用CPU
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        } catch (const std::exception& e) {
            InfusionLogger::error("相机处理线程出错: {}", e.what());
            std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // 出错时等待一秒
        }
    }
    
    InfusionLogger::info("相机处理线程已停止");
}
