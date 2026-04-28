/**
 * STEP 3: OPTIMIZED FEATURE EXTRACTION
 * 
 * This is an optimized version of ImprovedFeatureExtractor with:
 * - Vectorized operations (avoid explicit loops)
 * - Cached computations (reuse expensive results)
 * - Numerical stability (epsilon checks)
 * - Memory efficiency (pre-allocation)
 * - SIMD-friendly code structure
 * 
 * Expected speedup: 3-5x vs original
 */

#pragma once

#include <vector>
#include "core/Types.h"
#include "feature/FeatureConfig.h"
#include <opencv2/opencv.hpp>

namespace bullet_detection {

// ===== CACHED COMPUTATIONS STRUCTURE =====

struct CachedComputations {
    // Geometry cache
    double area = 0.0;
    double perimeter = 0.0;
    cv::Rect bbox;
    cv::Point2f centroid;
    std::vector<cv::Point> hull;
    double hull_area = 0.0;
    
    // Image cache
    cv::Mat roi_float;
    cv::Mat roi_gradient_x;
    cv::Mat roi_gradient_y;
    cv::Mat roi_laplacian;
    cv::Mat roi_edges;        // NEW: Cached Canny edges
    double mean_intensity = 0.0;
    double std_intensity = 0.0;
    
    // Statistics cache
    float signal_power = 0.0f;
    float noise_power = 1.0f;
    
    // Flags to track what's been computed
    bool has_area = false;
    bool has_gradient = false;
    bool has_laplacian = false;
    bool has_edges = false;   // NEW: Cached edges flag
    bool has_intensity_stats = false;
};

// ===== OPTIMIZED FEATURE EXTRACTOR =====

class ImprovedFeatureExtractorOptimized {
public:
    ImprovedFeatureExtractorOptimized();
    explicit ImprovedFeatureExtractorOptimized(const FeatureConfig& config);

    // Main extraction methods
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

    // Configuration
    void setConfig(const FeatureConfig& config) { config_ = config; }
    const FeatureConfig& getConfig() const { return config_; }

    // Performance metrics
    double getLastExtractionTime() const { return last_extraction_time_ms_; }
    int getTotalExtractionsCount() const { return total_extractions_; }
    double getAverageExtractionTime() const;

private:
    FeatureConfig config_;
    
    // Performance tracking
    mutable double last_extraction_time_ms_ = 0.0;
    mutable int total_extractions_ = 0;
    mutable double total_time_ms_ = 0.0;

    // ===== CACHED/VECTORIZED IMPLEMENTATIONS =====
    
    // Geometric features (vectorized)
    void computeGeometricFeaturesFast(
        const std::vector<cv::Point>& contour,
        CachedComputations& cache,
        CandidateFeature& feat
    ) const;
    
    // Radial features (optimized)
    void computeRadialFeaturesFast(
        const cv::Mat& roi,
        const std::vector<cv::Point>& contour,
        const CachedComputations& cache,
        CandidateFeature& feat
    ) const;
    
    // Energy & texture (vectorized)
    void computeEnergyTextureFeaturesFast(
        const cv::Mat& roi,
        const std::vector<cv::Point>& contour,
        CachedComputations& cache,
        CandidateFeature& feat
    ) const;
    
    // Frequency & phase (fast)
    void computeFrequencyFeaturesFast(
        const cv::Mat& roi,
        const std::vector<cv::Point>& contour,
        const CachedComputations& cache,
        CandidateFeature& feat
    ) const;
    
    // Extended features (vectorized)
    void computeExtendedFeaturesFast(
        const cv::Mat& roi,
        const std::vector<cv::Point>& contour,
        const CachedComputations& cache,
        CandidateFeature& feat
    ) const;

    // ===== CACHE INITIALIZATION =====
    
    // Pre-compute everything we can from contour
    void initializeGeometryCache(
        const std::vector<cv::Point>& contour,
        CachedComputations& cache
    ) const;
    
    // Pre-compute everything we can from image
    void initializeImageCache(
        const cv::Mat& roi_gray,
        const std::vector<cv::Point>& contour,
        CachedComputations& cache
    ) const;

    // ===== OPTIMIZATION HELPERS =====
    
    // Vectorized centroid computation
    cv::Point2f getCentroidFast(const std::vector<cv::Point>& contour) const;
    
    // Vectorized radius computation
    std::vector<float> computeRadiiFast(
        const std::vector<cv::Point>& contour,
        cv::Point2f center,
        int ray_count
    ) const;
    
    // Efficient intensity statistics using OpenCV functions
    void computeIntensityStatsFast(
        const cv::Mat& roi,
        const std::vector<cv::Point>& contour,
        double& mean,
        double& std_dev
    ) const;
    
    // Fast histogram-based entropy
    float computeEntropyFast(const cv::Mat& roi) const;
    
    // Vectorized edge detection and counting
    float computeEdgeDensityFast(
        const cv::Mat& roi,
        const std::vector<cv::Point>& contour
    ) const;

    // ===== NUMERICAL STABILITY =====
    
    // Safe division with epsilon
    static constexpr float EPSILON = 1e-6f;
    
    float safeDivide(float numerator, float denominator) const {
        return (std::abs(denominator) > EPSILON) ? numerator / denominator : 0.0f;
    }
    
    // Safe log
    float safeLog(float value) const {
        return (value > EPSILON) ? std::log(value) : std::log(EPSILON);
    }
    
    // Clamp to [0, 1]
    float clamp01(float value) const {
        return std::max(0.0f, std::min(1.0f, value));
    }

    // ===== NORMALIZATION & STANDARDIZATION =====
    
    void normalizeFeatures(CandidateFeature& feat) const;
    void standardizeFeatures(
        CandidateFeature& feat,
        const FeatureStatistics* stats,
        int offset
    ) const;

    // ===== TIMING HELPER =====
    
    class ScopedTimer {
    public:
        explicit ScopedTimer(double& elapsed_ms);
        ~ScopedTimer();
    private:
        double& elapsed_ms_;
        std::chrono::high_resolution_clock::time_point start_;
    };
};

} // namespace bullet_detection
