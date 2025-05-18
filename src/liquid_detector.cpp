// liquid_detector.cpp
#include "liquid_detector.hpp"
#include "logger.hpp"
#include <opencv2/opencv.hpp>
#include <vector>
#include <algorithm>
#include <numeric>

using namespace cv;
using namespace std;

// === 全局变量：控制裁剪区域 ===
double startHeight = 0.1;
double endHeight = 0.7;
double startWidth = 0.0;
double endWidth = 1.0;

double canny_thr_0 = 60;
double canny_thr_1 = 100;

// 液位检测辅助函数（霍夫线检测）
Vec4i detectLiquidLevelLine(const Mat &edgeImage)
{
    vector<Vec4i> lines;
    HoughLinesP(edgeImage, lines, 1, CV_PI / 180, 5, 10, 10);

    int imgHeight = edgeImage.rows;
    int imgWidth = edgeImage.cols;
    int minY = static_cast<int>(0 * imgHeight);
    int maxY = static_cast<int>(1.0 * imgHeight);

    // 只保留接近水平的线段
    vector<int> midYs;
    vector<Vec4i> horizontalLines;
    for (const auto &line : lines)
    {
        int x1 = line[0], y1 = line[1], x2 = line[2], y2 = line[3];
        int dy = y2 - y1;
        if (abs(dy) < 15)
        { // 接近水平
            int midY = (y1 + y2) / 2;
            if (midY >= minY && midY <= maxY)
            {
                midYs.push_back(midY);
                horizontalLines.push_back(line);
            }
        }
    }

    if (midYs.empty())
        return Vec4i(0, 0, 0, 0);

    // 取中位数对应的线段
    vector<size_t> indices(midYs.size());
    iota(indices.begin(), indices.end(), 0);
    sort(indices.begin(), indices.end(), [&](size_t i, size_t j)
         { return midYs[i] < midYs[j]; });
    size_t medianIdx = indices[midYs.size() / 2];
    Vec4i bestLine = horizontalLines[medianIdx];

    return bestLine;
}

// 映射函数（液位线位置到真实体积）
double simulation_function(double x)
{
    double a = -0.1053;
    double b = 3.4573;
    double c = -1.1123;
    double d = 29.3600;
    return a * x * x * x + b * x * x + c * x + d;
}

void setRoiParameters(double startH, double endH, double startW, double endW)
{
    startHeight = startH;
    endHeight = endH;
    startWidth = startW;
    endWidth = endW;
}

// 液位检测主函数
double detectLiquidLevelPercentage(const Mat &inputImage, double totalVolume)
{
    if (inputImage.empty())
    {
        InfusionLogger::error("输入图像为空，检测失败。");
        return -1.0;
    }

    // 将图像调整为固定大小
    Mat resizedImage;
    resize(inputImage, resizedImage, Size(640, 480));

    // 旋转180度
    Mat rotatedImage;
    rotate(resizedImage, rotatedImage, ROTATE_180);

    int width = rotatedImage.cols;
    int height = rotatedImage.rows;

    if (startHeight >= endHeight || startWidth >= endWidth)
    {
        InfusionLogger::error("裁剪参数设置错误（start应小于end）");
        return -1.0;
    }

    int cropX = static_cast<int>(width * startWidth);
    int cropY = static_cast<int>(height * startHeight);
    int cropWidth = static_cast<int>(width * (endWidth - startWidth));
    int cropHeight = static_cast<int>(height * (endHeight - startHeight));
    Rect roi(cropX, cropY, cropWidth, cropHeight);

    Mat croppedImage = rotatedImage(roi);

    Mat gray, edges;
    cvtColor(croppedImage, gray, COLOR_BGR2GRAY);
    Canny(gray, edges, canny_thr_0, canny_thr_1);

    // 膨胀
    Mat dilatedEdges;
    Mat kernel = getStructuringElement(MORPH_RECT, Size(10, 8));
    dilate(edges, dilatedEdges, kernel);

    imwrite("edges.jpg", dilatedEdges);

    Vec4i levelLine = detectLiquidLevelLine(dilatedEdges);

    if (levelLine == Vec4i(0, 0, 0, 0))
    {
        InfusionLogger::debug("未检测到液位线。");
        return -1.0;
    }

    int midY = (levelLine[1] + levelLine[3]) / 2;
    int distanceToBottom = cropHeight - midY;
    // --- 极低频低通滤波和升高保持器 ---
    static double filtered_percentage = 0.0;
    static double last_percentage = 0.0;
    static int hold_count = 0;
    const double alpha = 0.02; // 低通滤波系数，越小越平滑
    const int hold_limit = 10; // 升高保持次数

    double raw_percentage = (1 - distanceToBottom / static_cast<double>(cropHeight)) * 100.0;

    // 升高时直接更新，降低时需要保持
    if (raw_percentage > last_percentage)
    {
        filtered_percentage = raw_percentage;
        hold_count = 0;
    }
    else
    {
        if (hold_count < hold_limit)
        {
            hold_count++;
            // 保持不变
        }
        else
        {
            // 低通滤波缓慢下降
            filtered_percentage = alpha * raw_percentage + (1 - alpha) * filtered_percentage;
        }
    }
    last_percentage = filtered_percentage;
    double percentage = filtered_percentage;

    //    double simulatedValue = simulation_function(mappedValue);
    //    double percentage = (simulatedValue / totalVolume) * 100.0;

    InfusionLogger::debug("液位线位置：" + to_string(midY) + ", 距离底部: " + to_string(distanceToBottom) +
                          ", 占比: " + to_string(percentage) + "%");

    // 保存检测图片
    Mat outputImage = croppedImage.clone();
    line(outputImage, Point(levelLine[0], levelLine[1]), Point(levelLine[2], levelLine[3]), Scalar(0, 255, 0), 2);
    putText(outputImage, "Liquid Level", Point(levelLine[0], levelLine[1] - 10), FONT_HERSHEY_SIMPLEX, 0.5,
            Scalar(0, 255, 0), 2);
    putText(outputImage, "Percentage: " + to_string(percentage) + "%", Point(10, 30), FONT_HERSHEY_SIMPLEX, 0.5,
            Scalar(255, 255, 255), 2);
    imwrite("output.jpg", outputImage);

    return std::clamp(percentage, 0.0, 100.0);
}
