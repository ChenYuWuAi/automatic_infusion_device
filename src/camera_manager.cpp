#include "camera_manager.hpp"
#include "logger.hpp"
#include "liquid_detector.hpp"
#include <camera_hal/camera_lccv.hpp>
#include <thread>
#include <chrono>
#include <cstdio>
#include <sstream>
#include <nlohmann/json.hpp>
#include <array>

using json = nlohmann::json;

CameraManager::CameraManager()
{
}

CameraManager::~CameraManager()
{
    stopProcessing();
}

bool CameraManager::initialize(int width, int height, int framerate)
{
    try
    {
        camera_driver_ = std::make_shared<CameraHAL::CameraDriver_LCCV>();

        std::unordered_map<std::string, std::string> camera_params;
        camera_params["Width"] = std::to_string(width);
        camera_params["Height"] = std::to_string(height);
        camera_params["Framerate"] = std::to_string(framerate);

        if (!camera_driver_->open(camera_params))
        {
            InfusionLogger::error("无法打开相机！");
            return false;
        }

        return true;
    }
    catch (const std::exception &e)
    {
        InfusionLogger::error("初始化相机时出错: {}", e.what());
        return false;
    }
}

void CameraManager::startProcessing()
{
    if (camera_thread_running_.load())
    {
        InfusionLogger::warn("相机处理线程已经在运行！");
        return;
    }

    camera_thread_running_ = true;
    std::thread thread(&CameraManager::cameraThread, this);
    thread.detach(); // 分离线程
}

void CameraManager::stopProcessing()
{
    camera_thread_running_ = false;
    // 给线程一些时间来完成当前操作
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
}

double CameraManager::getLiquidLevelPercentage() const
{
    return liquid_level_percentage_.load();
}

bool CameraManager::isRunning() const
{
    return camera_thread_running_.load();
}

void CameraManager::cameraThread()
{
    InfusionLogger::info("相机处理线程已启动");
    lastCalibration_ = std::chrono::steady_clock::now() - calibrationInterval_; // 保证启动时立即标定

    while (camera_thread_running_)
    {
        try
        {
            cv::Mat frame;
            if (!camera_driver_->read(frame))
            {
                InfusionLogger::error("无法从相机读取帧！");
                std::this_thread::sleep_for(std::chrono::milliseconds(500)); // 出错时稍微等待长一点
                continue;
            }
            // 自动ROI标定
            auto now = std::chrono::steady_clock::now();
            if (now - lastCalibration_ >= calibrationInterval_)
            {
                calibrateROI(frame);
                lastCalibration_ = now;
                setRoiParameters(startHeight_, endHeight_, startWidth_, endWidth_);
                InfusionLogger::info("ROI 已标定: [{}, {}, {}, {}]", startHeight_, startWidth_, endHeight_, endWidth_);
            }

            if (!frame.empty())
            {
                // 假设总容量为100（可以在后续版本中参数化）
                double percentage = detectLiquidLevelPercentage(frame, 100.0);
                if (percentage >= 0)
                {
                    liquid_level_percentage_.store(percentage);
                }
            }

            // 控制帧率，避免过度占用CPU
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        catch (const std::exception &e)
        {
            InfusionLogger::error("相机处理线程出错: {}", e.what());
            std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // 出错时等待一秒
        }
    }

    InfusionLogger::info("相机处理线程已停止");
}

void CameraManager::calibrateROI(const cv::Mat &frame)
{
    try
    {
        // Rotate

        // 旋转180度
        cv::Mat rotatedImage;
        cv::rotate(frame, rotatedImage, cv::ROTATE_180);
        // 将帧保存到临时文件
        const std::string tmpFile = "/tmp/roi_calibration.jpg";
        cv::imwrite(tmpFile, rotatedImage);
        // 构造curl命令
        std::ostringstream cmd;
        cmd << "curl -s -F \"image=@" << tmpFile << "\" ";
        cmd << "-H \"X-API-KEY: 11222118\" ";
        cmd << "https://im.chenyuwuai.xyz/upload";

        // 执行并获取输出
        std::array<char, 4096> buffer;
        std::string result;
        FILE *pipe = popen(cmd.str().c_str(), "r");
        if (!pipe)
        {
            InfusionLogger::error("ROI 标定: 无法执行curl命令");
            return;
        }
        while (fgets(buffer.data(), buffer.size(), pipe))
        {
            result += buffer.data();
        }
        pclose(pipe);

        // 解析JSON
        auto j = json::parse(result);
        if (j.contains("bbox") && j["bbox"].is_array() && j["bbox"].size() == 4)
        {
            startWidth_ = j["bbox"][0].get<double>();
            startHeight_ = j["bbox"][1].get<double>();
            endWidth_ = j["bbox"][2].get<double>();
            endHeight_ = j["bbox"][3].get<double>();

            endHeight_ = endHeight_ - (endHeight_ - startHeight_) * 0.1;
            startHeight_ = startHeight_ + (endHeight_ - startHeight_) * 0.2;
            startHeight_ = std::clamp(startHeight_, 0.0, 1.0);

            if (endHeight_ - startHeight_ < 0.1)
            {
                endHeight_ = startHeight_ + 0.1;
            }

            InfusionLogger::info("ROI 已标定: [{}, {}, {}, {}]", startHeight_, startWidth_, endHeight_, endWidth_);
        }
        else
        {
            InfusionLogger::warn("ROI 标定: 返回数据不包含有效 bbox");
        }
    }
    catch (const std::exception &e)
    {
        InfusionLogger::error("ROI 标定出错: {}", e.what());
    }
}
