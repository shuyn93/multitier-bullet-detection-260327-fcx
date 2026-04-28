#pragma once

#include "core/Types.h"
#include <vector>
#include <string>
#include <opencv2/opencv.hpp>

namespace bullet_detection {

/**
 * @brief Improved Candidate Detector with High Recall Strategy
 * 
 * IMPROVEMENTS over original:
 * 1. Multi-stage preprocessing (denoise + CLAHE)
 * 2. Multi-threshold detection (Otsu + adaptive)
 * 3. Early radial property computation
 * 4. Robust noise filtering (geometric + radial)
 * 5. Small object handling (r < 10px)
 * 
 * Expected: Recall 70% ? 95%+, Precision 80% ? 85%+
 */
class ImprovedCandidateDetector {
public:
    ImprovedCandidateDetector();

    /**
     * @brief Detect candidates with high recall
     * @param frame_ir Input IR image
     * @param frame_id Frame identifier
     * @return Vector of detected candidates
     */
    std::vector<CandidateRegion> detectCandidates(
        const cv::Mat& frame_ir,
        uint64_t frame_id
    );

    /**
     * @brief Set preprocessing parameters
     */
    void setPreprocessingParams(
        int bilateral_diameter = 9,
        float bilateral_sigma = 75.0f,
        float clahe_clip_limit = 2.0f
    );

    /**
     * @brief Set detection thresholds
     */
    void setDetectionThresholds(
        int min_area = 10,              // Lowered from 25 for r < 10
        int max_area = 10000,
        float min_circularity = 0.5f,   // Tightened from 0.3
        float min_solidity = 0.6f,      // NEW
        float min_radial_symmetry = 0.5f // NEW
    );

    /**
     * @brief Enable/disable specific processing stages
     */
    void enableDenoising(bool enable) { denoise_enabled_ = enable; }
    void enableCLAHE(bool enable) { clahe_enabled_ = enable; }
    void enableAdaptiveThreshold(bool enable) { adaptive_threshold_enabled_ = enable; }
    void enableRadialFiltering(bool enable) { radial_filtering_enabled_ = enable; }

private:
    // ===== PREPROCESSING =====
    
    /**
     * @brief Multi-stage preprocessing
     * @param frame Input image
     * @return Preprocessed image
     */
    cv::Mat preprocessImage(const cv::Mat& frame);

    /**
     * @brief Denoise using bilateral filtering
     */
    cv::Mat denoiseImage(const cv::Mat& image);

    /**
     * @brief Enhance contrast using CLAHE
     */
    cv::Mat enhanceContrast(const cv::Mat& image);

    // ===== BLOB DETECTION =====

    /**
     * @brief Multi-threshold detection (Otsu + adaptive)
     * @param frame Preprocessed image
     * @return Vector of candidate contours
     */
    std::vector<std::vector<cv::Point>> detectBlobsMultiThreshold(
        const cv::Mat& frame
    );

    /**
     * @brief Otsu's automatic thresholding
     */
    std::vector<std::vector<cv::Point>> detectBlobsOtsu(const cv::Mat& frame);

    /**
     * @brief Adaptive local thresholding
     */
    std::vector<std::vector<cv::Point>> detectBlobsAdaptive(const cv::Mat& frame);

    /**
     * @brief Fusion: combine results from multiple thresholding methods
     */
    std::vector<std::vector<cv::Point>> fuseDetectionResults(
        const std::vector<std::vector<cv::Point>>& blobs_otsu,
        const std::vector<std::vector<cv::Point>>& blobs_adaptive
    );

    /**
     * @brief Process blobs for small object handling
     */
    void processSmallBlobs(
        std::vector<std::vector<cv::Point>>& contours,
        const cv::Mat& original_image
    );

    // ===== RADIAL PROPERTIES (EARLY COMPUTATION) =====

    /**
     * @brief Quick computation of radial symmetry (for early filtering)
     * @param roi Region of interest
     * @param contour Blob contour
     * @return Radial symmetry score [0, 1]
     */
    float computeQuickRadialSymmetry(
        const cv::Mat& roi,
        const std::vector<cv::Point>& contour
    );

    /**
     * @brief Quick computation of radial gradient consistency
     * @param roi Region of interest
     * @param contour Blob contour
     * @return Radial gradient consistency [0, 1]
     */
    float computeQuickRadialGradient(
        const cv::Mat& roi,
        const std::vector<cv::Point>& contour
    );

    // ===== FILTERING =====

    /**
     * @brief Geometric filtering (basic properties)
     * @return True if blob passes all checks
     */
    bool passesGeometricFilter(
        const cv::Rect& bbox,
        double area,
        double perimeter,
        const std::vector<cv::Point>& hull
    );

    /**
     * @brief Radial-based filtering (detects noise)
     * @return True if blob likely is a bullet hole
     */
    bool passesRadialFilter(
        const cv::Mat& roi,
        const std::vector<cv::Point>& contour
    );

    /**
     * @brief Intensity contrast filtering
     * @return True if contrast adequate
     */
    bool passesIntensityFilter(const cv::Mat& roi);

    /**
     * @brief Morphological validation
     */
    void validateAndCleanupContours(std::vector<std::vector<cv::Point>>& contours);

    // ===== HELPER FUNCTIONS =====

    /**
     * @brief Extract ROI from frame
     */
    cv::Mat extractROI(const cv::Mat& frame, const cv::Rect& bbox);

    /**
     * @brief Compute blob moments and centroid
     */
    cv::Point2f computeBlobCentroid(const std::vector<cv::Point>& contour);

    /**
     * @brief Sample points along radial direction
     */
    std::vector<float> sampleRadialProfile(
        const cv::Mat& roi,
        const cv::Point2f& center,
        float max_radius,
        int num_angles = 16
    );

    // ===== CONFIGURATION PARAMETERS =====

    // Preprocessing
    int bilateral_diameter_ = 9;
    float bilateral_sigma_ = 75.0f;
    float clahe_clip_limit_ = 2.0f;
    int clahe_tile_size_ = 8;

    // Thresholding
    int otsu_offset_ = 10;              // Adjust Otsu for local adaptation
    int adaptive_block_size_ = 11;      // Must be odd
    float adaptive_constant_ = 2.0f;

    // Filtering thresholds
    int min_area_ = 10;
    int max_area_ = 10000;
    float min_circularity_ = 0.5f;
    float min_solidity_ = 0.6f;
    float min_radial_symmetry_ = 0.5f;
    float min_radial_gradient_ = 0.4f;
    float min_contrast_ = 5.0f;        // Lowered from 10.0f to catch more small holes

    // Feature flags
    bool denoise_enabled_ = true;
    bool clahe_enabled_ = true;
    bool adaptive_threshold_enabled_ = true;
    bool radial_filtering_enabled_ = true;

    // Small blob handling
    static constexpr int SMALL_BLOB_THRESHOLD = 100;  // px²
    static constexpr int SMALL_BLOB_MAX_RADIUS = 10;  // px

    // Morphology kernel sizes
    static constexpr int MORPH_KERNEL_LARGE = 5;
    static constexpr int MORPH_KERNEL_SMALL = 3;
};

} // namespace bullet_detection
