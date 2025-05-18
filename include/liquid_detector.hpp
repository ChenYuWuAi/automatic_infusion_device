#pragma once
#include <opencv2/opencv.hpp>

double detectLiquidLevelPercentage(const cv::Mat& inputImage, double totalVolume = 250.0);
void setRoiParameters(double startH, double endH, double startW, double endW);