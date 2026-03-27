#pragma once

#include <vector>
#include <opencv2/opencv.hpp>
#include "core/Types.h"

namespace bullet_detection {

// ===== PERFORMANCE-OPTIMIZED FEATURE EXTRACTOR =====

class OptimizedFeatureExtractor {
public:
    OptimizedFeatureExtractor();

    // Main extraction method (optimized)
    CandidateFeature extractFeatures(
        const cv::Mat& roi_gray,
        const std::vector<cv::Point>& contour
    );

    // Feature vector version
    FeatureVector extractFeatureVector(
        const cv::Mat& roi_gray,
        const std::vector<cv::Point>& contour
    );

    // Initialize workspace (call once after creating extractor)
    void initializeWorkspace(int max_roi_width, int max_roi_height);

private:
    // Pre-allocated workspace (avoid repeated allocations)
    cv::Mat workspace_roi_float_;
    cv::Mat workspace_dx_;
    cv::Mat workspace_dy_;
    cv::Mat workspace_blurred_;
    cv::Mat workspace_edges_;
    std::vector<float> radii_buffer_;      // For radial symmetry
    std::vector<int> histogram_buffer_;    // For entropy
    
    // Feature computation methods (optimized)
    float computeRadialSymmetryFast(
        const cv::Mat& roi,
        const std::vector<cv::Point>& contour
    );

    float computeRadialGradientFast(
        const cv::Mat& roi,
        const std::vector<cv::Point>& contour
    );

    float computeEntropyFast(const cv::Mat& roi);

    float computeHistogramFeatures(const cv::Mat& roi);

    // Helper: Get ROI moments once
    void computeCentroid(const std::vector<cv::Point>& contour,
                        float& cx, float& cy) const;

    // Original methods (kept for compatibility)
    float computeArea(const std::vector<cv::Point>& contour);
    float computeCircularity(const std::vector<cv::Point>& contour);
    float computeSolidity(const std::vector<cv::Point>& contour);
    float computeAspectRatio(const std::vector<cv::Point>& contour);
    float computeSNR(const cv::Mat& roi);
    float computeRingEnergy(const cv::Mat& roi, const std::vector<cv::Point>& contour);
    float computeSharpness(const cv::Mat& roi);
    float computeLaplacianDensity(const cv::Mat& roi, const std::vector<cv::Point>& contour);
    float computePhaseCoherence(const cv::Mat& roi);
    float computeContrast(const cv::Mat& roi);
    float computeMeanIntensity(const cv::Mat& roi);
    float computeStdIntensity(const cv::Mat& roi);
    float computeEdgeDensity(const cv::Mat& roi, const std::vector<cv::Point>& contour);
    float computeCornerCount(const cv::Mat& roi);
    void normalizeFeatures(CandidateFeature& feat);
};

// ===== SIMD UTILITIES =====

class SIMDUtils {
public:
    // Fast distance computation (SIMD optimized)
    static float computeDistanceSSE(
        const float* v1,
        const float* v2,
        int dim
    );

    // Fast dot product
    static float computeDotProductSSE(
        const float* v1,
        const float* v2,
        int dim
    );

    // SIMD-friendly vector normalization
    static void normalizeVectorSSE(
        float* v,
        int dim
    );

    // Check if SIMD is available
    static bool isSSEAvailable();
    static bool isAVXAvailable();
};

// ===== OPTIMIZED CANDIDATE DETECTOR =====

class OptimizedCandidateDetector {
public:
    OptimizedCandidateDetector();

    std::vector<CandidateRegion> detectCandidates(
        const cv::Mat& frame_ir,
        uint64_t frame_id
    );

private:
    std::vector<std::vector<cv::Point>> detectContours(
        const cv::Mat& frame,
        int threshold = 100
    );

    float computeDetectionScore(const std::vector<cv::Point>& contour);

    // Configuration
    int min_area_ = 50;
    int max_area_ = 20000;
    float min_circularity_ = 0.5f;
};

// ===== PERFORMANCE MONITORING =====

struct StageLatency {
    float input_ms = 0.0f;
    float detection_ms = 0.0f;
    float feature_extraction_ms = 0.0f;
    float tier1_ms = 0.0f;
    float tier2_ms = 0.0f;
    float tier3_ms = 0.0f;
    float decision_ms = 0.0f;
    float output_ms = 0.0f;
    float total_ms = 0.0f;

    std::string toString() const;
};

class PerformanceMonitor {
public:
    PerformanceMonitor();

    void recordStageStart(int stage_id);
    void recordStageEnd(int stage_id);
    
    StageLatency getLastFrameLatency() const { return last_latency_; }
    StageLatency getAverageLatency() const { return avg_latency_; }

    void reset();
    std::string generateReport() const;

private:
    std::vector<double> stage_start_times_;
    StageLatency last_latency_;
    StageLatency avg_latency_;
    int frame_count_ = 0;
};

// ===== STAGE IDENTIFIERS =====
enum class PerformanceStage {
    INPUT = 0,
    DETECTION = 1,
    FEATURE_EXTRACTION = 2,
    TIER1 = 3,
    TIER2 = 4,
    TIER3 = 5,
    DECISION = 6,
    OUTPUT = 7
};

} // namespace bullet_detection
