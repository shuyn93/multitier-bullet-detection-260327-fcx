#pragma once
#include <vector>
#include "core/Types.h"
#include "core/MathUtils.h"
#include <opencv2/opencv.hpp>

namespace bullet_detection {

class FeatureExtractor {
public:
    FeatureExtractor();

    // Extract all 17 features with normalization
    CandidateFeature extractFeatures(
        const cv::Mat& roi_gray,
        const std::vector<cv::Point>& contour
    );

    FeatureVector extractFeatureVector(
        const cv::Mat& roi_gray,
        const std::vector<cv::Point>& contour
    );

private:
    // Geometric features (0-3)
    float computeArea(const std::vector<cv::Point>& contour);
    float computeCircularity(const std::vector<cv::Point>& contour);
    float computeSolidity(const std::vector<cv::Point>& contour);
    float computeAspectRatio(const std::vector<cv::Point>& contour);

    // Radial structure (4-5)
    float computeRadialSymmetry(const cv::Mat& roi, const std::vector<cv::Point>& contour);
    float computeRadialGradient(const cv::Mat& roi, const std::vector<cv::Point>& contour);

    // Energy & Texture (6-8)
    float computeSNR(const cv::Mat& roi);
    float computeEntropy(const cv::Mat& roi);
    float computeRingEnergy(const cv::Mat& roi, const std::vector<cv::Point>& contour);

    // Frequency & Phase (9-11)
    float computeSharpness(const cv::Mat& roi);
    float computeLaplacianDensity(const cv::Mat& roi, const std::vector<cv::Point>& contour);
    float computePhaseCoherence(const cv::Mat& roi);

    // Extended (12-16)
    float computeContrast(const cv::Mat& roi);
    float computeMeanIntensity(const cv::Mat& roi);
    float computeStdIntensity(const cv::Mat& roi);
    float computeEdgeDensity(const cv::Mat& roi, const std::vector<cv::Point>& contour);
    float computeCornerCount(const cv::Mat& roi);

    // Normalization
    void normalizeFeatures(CandidateFeature& feat);
};

} // namespace bullet_detection
