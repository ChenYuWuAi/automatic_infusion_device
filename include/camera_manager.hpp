#ifndef CAMERA_MANAGER_HPP
#define CAMERA_MANAGER_HPP

#include <memory>
#include <atomic>
#include <opencv2/opencv.hpp>
#include <camera_hal/camera_driver.hpp>
#include <chrono>

/**
 * @brief 相机管理器类，负责相机操作和液位检测
 */
class CameraManager {
public:
    /**
     * @brief 构造函数
     */
    CameraManager();
    
    /**
     * @brief 析构函数
     */
    ~CameraManager();
    
    /**
     * @brief 初始化相机
     * @param width 宽度
     * @param height 高度
     * @param framerate 帧率
     * @return 是否初始化成功
     */
    bool initialize(int width = 640, int height = 480, int framerate = 30);
    
    /**
     * @brief 启动相机处理线程
     */
    void startProcessing();
    
    /**
     * @brief 停止相机处理线程
     */
    void stopProcessing();
    
    /**
     * @brief 获取最新的液位百分比
     * @return 液位百分比
     */
    double getLiquidLevelPercentage() const;
    
    /**
     * @brief 相机线程是否正在运行
     * @return 是否正在运行
     */
    bool isRunning() const;
    
private:
    std::shared_ptr<CameraHAL::CameraDriver> camera_driver_;
    std::atomic<bool> camera_thread_running_{false};
    std::atomic<double> liquid_level_percentage_{-1.0};
    // ROI坐标（相对比例）
    double startHeight_ = 0.0, startWidth_ = 0.0, endHeight_ = 1.0, endWidth_ = 1.0;
    // 标定间隔（毫秒），默认5分钟
    const std::chrono::milliseconds calibrationInterval_{300000};
    std::chrono::steady_clock::time_point lastCalibration_;
  
    /**
     * @brief 执行ROI自动标定
     * @param frame 当前帧
     */
    void calibrateROI(const cv::Mat& frame);

    /**
     * @brief 相机处理线程
     */
    void cameraThread();
};

#endif // CAMERA_MANAGER_HPP
