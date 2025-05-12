#pragma once
#include <opencv2/opencv.hpp>

double detectLiquidLevelPercentage(const cv::Mat& inputImage, double totalVolume = 250.0);