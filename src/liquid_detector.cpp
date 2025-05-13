// liquid_detector.cpp
#include "liquid_detector.hpp"
#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>
#include <algorithm>

using namespace cv;
using namespace std;

// === 全局变量：控制裁剪区域 ===
double startHeight = 0.0;
double endHeight = 1.0;
double startWidth = 0.0;
double endWidth = 1.0;

// 液位检测辅助函数（霍夫线检测）
Vec4i detectLiquidLevelLine(const Mat& edgeImage) {
    vector<Vec4i> lines;
    HoughLinesP(edgeImage, lines, 1, CV_PI / 180, 70, 40, 10);

    int imgHeight = edgeImage.rows;
    int minY = static_cast<int>(0.30 * imgHeight);
    int maxY = static_cast<int>(0.60 * imgHeight);

    Vec4i bestLine;
    int maxLength = 0;

    for (const auto& line : lines) {
        int x1 = line[0], y1 = line[1], x2 = line[2], y2 = line[3];
        if (abs(y2 - y1) < 15) { // 接近水平
            int midY = (y1 + y2) / 2;
            if (midY >= minY && midY <= maxY) {
                int length = abs(x2 - x1);
                if (length > maxLength) {
                    maxLength = length;
                    bestLine = line;
                }
            }
        }
    }

    return bestLine;
}

// 映射函数（液位线位置到真实体积）
double simulation_function(double x) {
    double a = -0.1053;
    double b = 3.4573;
    double c = -1.1123;
    double d = 29.3600;
    return a * x * x * x + b * x * x + c * x + d;
}

// 液位检测主函数
double detectLiquidLevelPercentage(const Mat& inputImage, double totalVolume) {
    if (inputImage.empty()) {
        cerr << "输入图像为空，检测失败。" << endl;
        return -1.0;
    }

    // 将图像调整为固定大小
    Mat resizedImage;
    resize(inputImage, resizedImage, Size(640, 480));

    int width = resizedImage.cols;
    int height = resizedImage.rows;

    startHeight = std::clamp(startHeight, 0.0, 1.0);
    endHeight = std::clamp(endHeight, 0.0, 1.0);
    startWidth = std::clamp(startWidth, 0.0, 1.0);
    endWidth = std::clamp(endWidth, 0.0, 1.0);

    if (startHeight >= endHeight || startWidth >= endWidth) {
        cerr << "裁剪参数设置错误（start应小于end）" << endl;
        return -1.0;
    }

    int cropX = static_cast<int>(width * startWidth);
    int cropY = static_cast<int>(height * startHeight);
    int cropWidth = static_cast<int>(width * (endWidth - startWidth));
    int cropHeight = static_cast<int>(height * (endHeight - startHeight));
    Rect roi(cropX, cropY, cropWidth, cropHeight);

    Mat croppedImage = resizedImage(roi);

    Mat gray, edges;
    cvtColor(croppedImage, gray, COLOR_BGR2GRAY);
    Canny(gray, edges, 50, 150);

    Vec4i levelLine = detectLiquidLevelLine(edges);

    if (levelLine == Vec4i(0, 0, 0, 0)) {
        cerr << "未检测到液位线。" << endl;
        return -1.0;
    }

    int midY = (levelLine[1] + levelLine[3]) / 2;
    int distanceToBottom = cropHeight - midY;
    double mappedValue = (distanceToBottom / static_cast<double>(cropHeight)) * 11.98;

    double simulatedValue = simulation_function(mappedValue);
    double percentage = (simulatedValue / totalVolume) * 100.0;

    return std::clamp(percentage, 0.0, 100.0);
}
