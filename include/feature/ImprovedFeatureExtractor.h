#pragma once

#include <vector>
#include "core/Types.h"
#include "feature/FeatureConfig.h"
#include <opencv2/opencv.hpp>

namespace bullet_detection {

class ImprovedFeatureExtractor {
public:
    ImprovedFeatureExtractor();
    explicit ImprovedFeatureExtractor(const FeatureConfig& config);

    // Extract with optional statistics for standardization
    CandidateFeature extractFeatures(
        const cv::Mat& roi_gray,
        const std::vector<cv::Point>& contour,
        const FeatureStatistics* stats = nullptr,
        int feature_idx_offset = 0
    );

    FeatureVector extractFeatureVector(
        const cv::Mat& roi_gray,
        const std::vector<cv::Point>& contour,
        const FeatureStatistics* stats = nullptr
    );

    // Update configuration
    void setConfig(const FeatureConfig& config) { config_ = config; }
    const FeatureConfig& getConfig() const { return config_; }

private:
    FeatureConfig config_;

    // Geometric features (0-3)
    float computeArea(const std::vector<cv::Point>& contour) const;
    float computeCircularity(const std::vector<cv::Point>& contour) const;
    float computeSolidity(const std::vector<cv::Point>& contour) const;
    float computeAspectRatio(const std::vector<cv::Point>& contour) const;

    // Radial structure (4-5)
    float computeRadialSymmetry(
        const cv::Mat& roi,
        const std::vector<cv::Point>& contour
    ) const;
    float computeRadialGradient(
        const cv::Mat& roi,
        const std::vector<cv::Point>& contour
    ) const;

    // Energy & Texture (6-8)
    float computeSNR(const cv::Mat& roi) const;
    float computeEntropy(const cv::Mat& roi) const;
    float computeRingEnergy(
        const cv::Mat& roi,
        const std::vector<cv::Point>& contour
    ) const;

    // Frequency & Phase (9-10, removed 11 = incorrect phase coherence)
    float computeSharpness(const cv::Mat& roi) const;
    float computeLaplacianDensity(
        const cv::Mat& roi,
        const std::vector<cv::Point>& contour
    ) const;
    // REMOVED: computePhaseCoherence() - incorrect implementation

    // Extended (12-16, consolidated to 15 = removed redundant std_intensity)
    float computeContrast(const cv::Mat& roi) const;
    float computeMeanIntensity(const cv::Mat& roi) const;
    // REMOVED: computeStdIntensity() - duplicate of contrast
    float computeEdgeDensity(
        const cv::Mat& roi,
        const std::vector<cv::Point>& contour
    ) const;
    float computeCornerCount(const cv::Mat& roi) const;

    // Normalization and standardization
    void normalizeFeatures(CandidateFeature& feat) const;
    void standardizeFeatures(
        CandidateFeature& feat,
        const FeatureStatistics* stats,
        int offset
    ) const;

    // Helper: Get centroid
    cv::Point2f getCentroid(const std::vector<cv::Point>& contour) const;
};

// ===== NEW FEATURE ANALYSIS UTILITIES =====

class FeatureAnalyzer {
public:
    // Compute correlation matrix between features
    static cv::Mat computeCorrelationMatrix(
        const std::vector<std::vector<float>>& features
    );

    // Compute feature importance using ensemble methods
    static std::vector<float> computeFeatureImportance(
        const std::vector<std::vector<float>>& features,
        const std::vector<int>& labels
    );

    // Detect redundant features
    static std::vector<int> identifyRedundantFeatures(
        const cv::Mat& correlation_matrix,
        float correlation_threshold = 0.9f
    );

    // Detect outliers per feature
    static std::vector<std::vector<bool>> detectFeatureOutliers(
        const std::vector<std::vector<float>>& features,
        float k = 1.5f
    );
};

} // namespace bullet_detection
