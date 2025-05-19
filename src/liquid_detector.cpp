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

double canny_thr_0 = 40;
double canny_thr_1 = 60;

// 液位检测辅助函数（霍夫线检测）
Vec4i detectLiquidLevelLine(const Mat &edgeImage)
{
    vector<Vec4i> lines;
    HoughLinesP(edgeImage, lines, 1, CV_PI / 180, 5, 50, 10);

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

#include <map>
#include <cmath>

// 液位检测主函数（多次检测+分桶+中位平均）
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

    std::vector<double> percentages;

    for (int i = 0; i < 50; ++i)
    {
        Mat gray, edges;
        cvtColor(croppedImage, gray, COLOR_BGR2GRAY);
        Canny(gray, edges, canny_thr_0, canny_thr_1);

        // 膨胀
        Mat dilatedEdges;
        Mat kernel = getStructuringElement(MORPH_RECT, Size(15, 8));
        dilate(edges, dilatedEdges, kernel);

        Vec4i levelLine = detectLiquidLevelLine(dilatedEdges);

        if (levelLine == Vec4i(0, 0, 0, 0))
        {
            percentages.push_back(0.0);
            continue;
        }

        int midY = (levelLine[1] + levelLine[3]) / 2;
        int distanceToBottom = cropHeight - midY;
        double raw_percentage = (1 - distanceToBottom / static_cast<double>(cropHeight)) * 100.0;
        percentages.push_back(std::clamp(raw_percentage, 0.0, 100.0));
    }

    // 分桶，相差不超过5为一桶
    std::vector<std::vector<double>> buckets;
    std::vector<bool> used(percentages.size(), false);

    for (size_t i = 0; i < percentages.size(); ++i)
    {
        if (used[i])
            continue;
        std::vector<double> bucket = {percentages[i]};
        used[i] = true;
        for (size_t j = i + 1; j < percentages.size(); ++j)
        {
            if (!used[j] && std::abs(percentages[j] - percentages[i]) <= 5.0)
            {
                bucket.push_back(percentages[j]);
                used[j] = true;
            }
        }
        buckets.push_back(bucket);
    }

    // 计算每个桶的平均值
    std::vector<double> bucket_averages;
    for (const auto &bucket : buckets)
    {
        if (!bucket.empty())
        {
            double sum = std::accumulate(bucket.begin(), bucket.end(), 0.0);
            bucket_averages.push_back(sum / bucket.size());
        }
    }

    if (bucket_averages.empty())
    {
        InfusionLogger::debug("未检测到有效液位线。");
        return -1.0;
    }

    // 取平均后的中位数作为最终结果
    std::sort(bucket_averages.begin(), bucket_averages.end());
    double final_result;
    size_t n = bucket_averages.size();
    if (n % 2 == 1)
        final_result = bucket_averages[n / 2];
    else
        final_result = (bucket_averages[n / 2 - 1] + bucket_averages[n / 2]) / 2.0;

    double raw_percentage = final_result;
    static double last_percentage = 0.0;
    static double filtered_percentage = 0.0;
    static int hold_count = 0;
    static const int hold_limit = 5; // 保持次数限制
    double alpha = 0.1;              // 低通滤波系数

    // 升高时直接更新，降低时需要保持
    if (raw_percentage > last_percentage)
    {
        // 低通滤波缓慢上升
        filtered_percentage = alpha * raw_percentage + (1 - alpha) * filtered_percentage;
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
    final_result = filtered_percentage;

    // 保存检测图片（最后一次）
    Mat outputImage = croppedImage.clone();
    // 需要对croppedImage进行灰度和边缘处理，才能传给detectLiquidLevelLine
    Mat gray, edges;
    cvtColor(outputImage, gray, COLOR_BGR2GRAY);
    Canny(gray, edges, canny_thr_0, canny_thr_1);
    Mat dilatedEdges;
    Mat kernel = getStructuringElement(MORPH_RECT, Size(10, 8));
    dilate(edges, dilatedEdges, kernel);
    Vec4i lastLine = detectLiquidLevelLine(dilatedEdges);
    if (lastLine != Vec4i(0, 0, 0, 0))
    {
        line(outputImage, Point(lastLine[0], lastLine[1]), Point(lastLine[2], lastLine[3]), Scalar(0, 255, 0), 2);
        putText(outputImage, "Liquid Level", Point(lastLine[0], lastLine[1] - 10), FONT_HERSHEY_SIMPLEX, 0.5,
                Scalar(0, 255, 0), 2);
    }
    putText(outputImage, "Percentage: " + to_string(final_result) + "%", Point(10, 30), FONT_HERSHEY_SIMPLEX, 0.5,
            Scalar(255, 255, 255), 2);
    imwrite("output.jpg", outputImage);

    InfusionLogger::debug("最终液位占比: " + to_string(final_result) + "%");

    return std::clamp(final_result, 0.0, 100.0);
}
