/**
 * STEP 2: IMPROVED BLOB DETECTOR FOR PRODUCTION
 * 
 * C++ implementation of high-recall, high-precision blob detection
 * - Two-stage pipeline (generation + filtering)
 * - Multi-threshold strategy
 * - Small object optimization
 * - High-resolution support
 */

#pragma once

#include <opencv2/opencv.hpp>
#include <vector>
#include <memory>
#include <cmath>

namespace bullet_detection {

// Structure to hold candidate blob information
struct BlobCandidate {
    cv::Rect bbox;
    std::vector<cv::Point> contour;
    float circularity;
    float solidity;
    float intensity_contrast;
    float blob_intensity;
    float background_intensity;
    cv::Mat roi;
};

class ImprovedBlobDetectorCpp {
public:
    ImprovedBlobDetectorCpp(int image_size = 256);
    
    /**
     * Complete two-stage detection pipeline
     * Stage 1: Generate candidates (maximize recall)
     * Stage 2: Filter & score (maximize precision)
     */
    std::vector<BlobCandidate> detectBlobs(const cv::Mat& image);
    
    /**
     * Specialized detection for small objects (r < 10 px)
     */
    std::vector<BlobCandidate> detectSmallObjects(const cv::Mat& image);
    
    /**
     * High-resolution optimization (downsample + detect + refine)
     */
    std::vector<BlobCandidate> detectBlobsHighRes(
        const cv::Mat& image,
        float scale_factor = 0.5f
    );
    
    // Setters for parameters
    void setThresholdLevels(const std::vector<int>& levels) { threshold_levels_ = levels; }
    void setAdaptiveBlockSize(int size) { adaptive_block_size_ = size; }
    void setAdaptiveConstant(int constant) { adaptive_constant_ = constant; }
    void setMinCircularity(float value) { min_circularity_loose_ = value; }
    
private:
    // Detection parameters
    int image_size_;
    std::vector<int> threshold_levels_;  // Multi-threshold levels
    int adaptive_block_size_;             // For adaptive threshold
    int adaptive_constant_;
    
    // Size constraints (loose, for STAGE 1)
    int min_size_small_;    // Minimum pixel area
    int max_size_small_;    // ~10 pixel radius
    int min_size_large_;    // >10 pixel radius
    int max_size_large_;    // Large upper limit
    
    // Filtering parameters (STAGE 2)
    float min_circularity_loose_;
    float min_solidity_;
    
    // ==================== STAGE 1: CANDIDATE GENERATION ====================
    
    /**
     * Generate all candidates (high recall)
     */
    std::vector<std::vector<cv::Point>> detectCandidatesStage1(const cv::Mat& image);
    
    /**
     * STAGE 1A: Multi-threshold detection
     * Try multiple threshold levels, combine results
     */
    std::vector<std::vector<cv::Point>> detectMultiThreshold(const cv::Mat& image);
    
    /**
     * STAGE 1B: Adaptive threshold detection
     * Handle varying background brightness
     */
    std::vector<std::vector<cv::Point>> detectAdaptiveThreshold(const cv::Mat& image);
    
    /**
     * STAGE 1C: Morphological reconstruction
     * Good for low-contrast, partially visible holes
     */
    std::vector<std::vector<cv::Point>> detectMorphological(const cv::Mat& image);
    
    /**
     * Deduplicate candidates (same region detected by multiple methods)
     */
    std::vector<std::vector<cv::Point>> deduplicateCandidates(
        const std::vector<std::vector<cv::Point>>& candidates
    );
    
    /**
     * Calculate bounding box intersection over union
     */
    float bboxOverlap(const cv::Rect& bbox1, const cv::Rect& bbox2);
    
    // ==================== STAGE 2: FILTERING & SCORING ====================
    
    /**
     * Apply filtering and scoring to candidates
     */
    std::vector<BlobCandidate> filterAndScoreCandidates(
        const cv::Mat& image,
        const std::vector<std::vector<cv::Point>>& contours
    );
    
    /**
     * Compute quality scores for a blob
     */
    BlobCandidate computeQualityScores(
        const cv::Mat& image,
        const std::vector<cv::Point>& contour
    );
};

} // namespace bullet_detection
