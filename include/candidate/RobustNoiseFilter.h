#pragma once

#include "core/Types.h"
#include "candidate/ImprovedCandidateDetector.h"
#include <vector>
#include <opencv2/opencv.hpp>

namespace bullet_detection {

/**
 * @brief Advanced noise filtering with confidence scoring
 * 
 * STEP 3 IMPROVEMENTS:
 * 1. Texture-based classification (LBP, HOG)
 * 2. Frequency domain analysis (FFT-based features)
 * 3. Machine learning confidence scoring
 * 4. Separability metrics (bullet vs noise)
 * 5. Risk assessment & outlier detection
 * 
 * Goal: Precision 80% ? 90%+ while maintaining 95%+ recall
 */
class RobustNoiseFilter {
public:
    RobustNoiseFilter();

    /**
     * @brief Filter candidates and compute confidence scores
     * @param candidates Raw candidates from ImprovedCandidateDetector
     * @param original_frame Original IR frame
     * @return Filtered candidates with confidence scores
     */
    std::vector<CandidateRegion> filterAndScoreCandidates(
        const std::vector<CandidateRegion>& candidates,
        const cv::Mat& original_frame
    );

    /**
     * @brief Get detailed analysis for a single candidate
     * @param roi Region of interest
     * @param contour Blob contour
     * @return Detailed analysis with scores
     */
    struct NoiseAnalysis {
        float texture_score;           // LBP-based texture uniformity [0,1]
        float frequency_score;         // FFT-based radial frequency [0,1]
        float intensity_profile_score; // Radial intensity profile consistency [0,1]
        float border_quality_score;    // Edge sharpness & definition [0,1]
        float compactness_score;       // Normalized second moment [0,1]
        
        float overall_confidence;      // Weighted combination [0,1]
        float noise_risk;              // Probability of being noise [0,1]
    };

    NoiseAnalysis analyzeBlob(
        const cv::Mat& roi,
        const std::vector<cv::Point>& contour
    );

    /**
     * @brief Set filtering aggressiveness
     * @param level 0=aggressive (high precision), 1=balanced, 2=lenient (high recall)
     */
    void setFilteringLevel(int level);

    /**
     * @brief Enable/disable specific analysis components
     */
    void enableTextureAnalysis(bool enable) { texture_enabled_ = enable; }
    void enableFrequencyAnalysis(bool enable) { frequency_enabled_ = enable; }
    void enableProfileAnalysis(bool enable) { profile_enabled_ = enable; }
    void enableBorderAnalysis(bool enable) { border_enabled_ = enable; }

private:
    // ===== TEXTURE ANALYSIS =====
    
    /**
     * @brief Local Binary Patterns (LBP) for texture
     * @return Texture uniformity score [0,1]
     * High = uniform (likely bullet)
     * Low = random texture (likely noise)
     */
    float computeLBPUniformity(const cv::Mat& roi);

    /**
     * @brief Histogram of Gradients (HOG) for structure
     * @return HOG consistency score [0,1]
     */
    float computeHOGConsistency(const cv::Mat& roi);

    /**
     * @brief Gabor filter response (for radial structure)
     * @return Radial structure strength [0,1]
     */
    float computeGaborRadialResponse(const cv::Mat& roi);

    // ===== FREQUENCY ANALYSIS =====

    /**
     * @brief FFT-based radial frequency analysis
     * @return Frequency score [0,1]
     * High = strong fundamental frequency (bullet)
     * Low = white noise (noise artifact)
     */
    float computeFFTRadialFrequency(const cv::Mat& roi);

    /**
     * @brief Wavelet decomposition for multi-scale analysis
     * @return Wavelet consistency [0,1]
     */
    float computeWaveletConsistency(const cv::Mat& roi);

    /**
     * @brief Phase coherence in frequency domain
     * @return Phase coherence [0,1]
     */
    float computePhaseCoherence(const cv::Mat& roi);

    // ===== RADIAL PROFILE ANALYSIS =====

    /**
     * @brief Analyze radial intensity profile
     * @return Profile smoothness & consistency [0,1]
     * High = smooth decay from center (bullet)
     * Low = jagged profile (noise)
     */
    float analyzeRadialIntensityProfile(
        const cv::Mat& roi,
        const std::vector<cv::Point>& contour
    );

    /**
     * @brief Compute radial derivative consistency
     * @return Derivative smoothness [0,1]
     */
    float computeRadialDerivativeSmoothed(
        const cv::Mat& roi,
        const std::vector<cv::Point>& contour
    );

    /**
     * @brief Check for Gaussian intensity profile
     * @return Gaussian fit quality [0,1]
     * High = fits Gaussian (likely bullet)
     * Low = doesn't fit (noise)
     */
    float fitGaussianProfile(const cv::Mat& roi);

    // ===== BORDER & EDGE ANALYSIS =====

    /**
     * @brief Analyze blob boundary quality
     * @return Boundary sharpness [0,1]
     * High = sharp boundary (real bullet)
     * Low = fuzzy boundary (noise)
     */
    float analyzeBoundarySharpness(
        const cv::Mat& roi,
        const std::vector<cv::Point>& contour
    );

    /**
     * @brief Compute edge regularity (smoothness)
     * @return Regularity score [0,1]
     */
    float computeEdgeRegularity(const std::vector<cv::Point>& contour);

    /**
     * @brief Detect artifacts at boundary
     * @return Artifact presence [0,1]
     * High = many artifacts (noise)
     * Low = clean boundary (bullet)
     */
    float detectBoundaryArtifacts(const cv::Mat& roi);

    // ===== SEPARABILITY METRICS =====

    /**
     * @brief Compute feature separability index
     * Used to assess how well bullet/noise are separated
     * @return Index [0,1]
     */
    float computeSeparabilityIndex(const NoiseAnalysis& analysis);

    /**
     * @brief Check for outliers in feature space
     * @return Outlier probability [0,1]
     */
    float computeOutlierProbability(const NoiseAnalysis& analysis);

    // ===== HELPER FUNCTIONS =====

    /**
     * @brief Extract radial profile
     */
    std::vector<float> extractRadialProfile(
        const cv::Mat& roi,
        const cv::Point2f& center,
        int num_samples = 64
    );

    /**
     * @brief Smooth noisy profile using moving average
     */
    std::vector<float> smoothProfile(
        const std::vector<float>& profile,
        int window_size = 5
    );

    /**
     * @brief Compute derivative of profile
     */
    std::vector<float> computeDerivative(const std::vector<float>& profile);

    /**
     * @brief Compute curvature of profile
     */
    std::vector<float> computeCurvature(const std::vector<float>& profile);

    /**
     * @brief Fit polynomial to profile (for goodness-of-fit test)
     */
    float fitPolynomial(const std::vector<float>& profile, int degree = 2);

    /**
     * @brief Local Binary Pattern computation
     */
    void computeLBP(const cv::Mat& image, cv::Mat& lbp_image);

    /**
     * @brief Compute Histogram of Gradients
     */
    cv::Mat computeHOG(const cv::Mat& image, int cell_size = 8);

    /**
     * @brief Apply Gabor filter bank
     */
    std::vector<cv::Mat> applyGaborFilters(const cv::Mat& image);

    /**
     * @brief Compute statistical features of image
     */
    struct ImageStats {
        float mean;
        float std;
        float skewness;
        float kurtosis;
        float entropy;
    };

    ImageStats computeImageStatistics(const cv::Mat& image);

    // ===== CONFIGURATION =====

    // Texture analysis weights
    float lbp_weight_ = 0.3f;
    float hog_weight_ = 0.2f;
    float gabor_weight_ = 0.2f;

    // Frequency analysis weights
    float fft_weight_ = 0.15f;
    float wavelet_weight_ = 0.1f;
    float phase_weight_ = 0.05f;

    // Profile analysis weights
    float intensity_profile_weight_ = 0.25f;
    float derivative_weight_ = 0.2f;
    float gaussian_fit_weight_ = 0.15f;

    // Border analysis weights
    float boundary_sharpness_weight_ = 0.3f;
    float edge_regularity_weight_ = 0.25f;
    float artifact_detection_weight_ = 0.25f;

    // Filtering thresholds (adjustable by level)
    float confidence_threshold_aggressive_ = 0.75f;  // High precision
    float confidence_threshold_balanced_ = 0.60f;    // Balanced
    float confidence_threshold_lenient_ = 0.45f;     // High recall

    float current_confidence_threshold_ = 0.60f;

    // Feature flags
    bool texture_enabled_ = true;
    bool frequency_enabled_ = true;
    bool profile_enabled_ = true;
    bool border_enabled_ = true;

    // Cache for repeated computations
    static constexpr int CACHE_SIZE = 100;
    struct ComputationCache {
        cv::Mat last_roi;
        ImageStats last_stats;
    } cache_;

    // Statistical baseline (learned from training data)
    static constexpr float BULLET_HOLE_MEAN_CONFIDENCE = 0.85f;
    static constexpr float NOISE_MEAN_CONFIDENCE = 0.35f;
};

} // namespace bullet_detection
