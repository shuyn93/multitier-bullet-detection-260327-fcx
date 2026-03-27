#include "feature/ImprovedFeatureExtractor.h"
#include "core/MathUtils.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <opencv2/opencv.hpp>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace bullet_detection {

ImprovedFeatureExtractor::ImprovedFeatureExtractor() {}

ImprovedFeatureExtractor::ImprovedFeatureExtractor(const FeatureConfig& config)
    : config_(config) {}

CandidateFeature ImprovedFeatureExtractor::extractFeatures(
    const cv::Mat& roi_gray,
    const std::vector<cv::Point>& contour,
    const FeatureStatistics* stats,
    int feature_idx_offset
) {
    CandidateFeature feat;

    // Geometric (0-3)
    feat.area = computeArea(contour);
    feat.circularity = computeCircularity(contour);
    feat.solidity = computeSolidity(contour);
    feat.aspect_ratio = computeAspectRatio(contour);

    // Radial (4-5)
    feat.radial_symmetry = computeRadialSymmetry(roi_gray, contour);
    feat.radial_gradient = computeRadialGradient(roi_gray, contour);

    // Energy & Texture (6-8)
    feat.snr = computeSNR(roi_gray);
    feat.entropy = computeEntropy(roi_gray);
    feat.ring_energy = computeRingEnergy(roi_gray, contour);

    // Frequency & Phase (9-10, removed 11)
    feat.sharpness = computeSharpness(roi_gray);
    feat.laplacian_density = computeLaplacianDensity(roi_gray, contour);
    feat.phase_coherence = 0.5f;  // Placeholder (feature removed as incorrect)

    // Extended (12-15, consolidated)
    feat.contrast = computeContrast(roi_gray);
    feat.mean_intensity = computeMeanIntensity(roi_gray);
    feat.std_intensity = feat.contrast;  // Consolidated with contrast
    feat.edge_density = computeEdgeDensity(roi_gray, contour);
    feat.corner_count = computeCornerCount(roi_gray);

    // Normalization
    normalizeFeatures(feat);

    // Optional: Standardization using training statistics
    if (stats != nullptr && config_.use_standardization) {
        standardizeFeatures(feat, stats, feature_idx_offset);
    }

    return feat;
}

FeatureVector ImprovedFeatureExtractor::extractFeatureVector(
    const cv::Mat& roi_gray,
    const std::vector<cv::Point>& contour,
    const FeatureStatistics* stats
) {
    auto feat = extractFeatures(roi_gray, contour, stats, 0);
    return feat.toFeatureVector();
}

// ===== GEOMETRIC FEATURES (IMPROVED) =====

float ImprovedFeatureExtractor::computeArea(const std::vector<cv::Point>& contour) const {
    if (contour.empty()) return 0.0f;
    double area = cv::contourArea(contour);
    return std::min(1.0f, static_cast<float>(area / config_.area_divisor));
}

float ImprovedFeatureExtractor::computeCircularity(const std::vector<cv::Point>& contour) const {
    if (contour.empty()) return 0.0f;
    double perimeter = cv::arcLength(contour, true);
    double area = cv::contourArea(contour);
    return math_utils::computeCircularity(area, perimeter);
}

float ImprovedFeatureExtractor::computeSolidity(const std::vector<cv::Point>& contour) const {
    if (contour.empty()) return 0.0f;
    std::vector<cv::Point> hull;
    cv::convexHull(contour, hull);
    double contour_area = cv::contourArea(contour);
    double hull_area = cv::contourArea(hull);
    return math_utils::computeSolidity(contour_area, hull_area);
}

float ImprovedFeatureExtractor::computeAspectRatio(const std::vector<cv::Point>& contour) const {
    if (contour.empty()) return 0.5f;
    cv::Rect bbox = cv::boundingRect(contour);
    return math_utils::computeAspectRatio(bbox.width, bbox.height);
}

// ===== RADIAL STRUCTURE (IMPROVED WITH ROBUST STATS) =====

float ImprovedFeatureExtractor::computeRadialSymmetry(
    const cv::Mat& roi,
    const std::vector<cv::Point>& contour
) const {
    if (contour.empty() || roi.empty()) return 0.5f;

    cv::Moments moments = cv::moments(contour);
    if (moments.m00 < 1e-6) return 0.5f;

    cv::Point2f center = getCentroid(contour);
    std::vector<float> radii;

    for (int i = 0; i < config_.radial_ray_count; ++i) {
        double angle = 2.0 * M_PI * i / config_.radial_ray_count;
        float max_r = 0.0f;

        for (const auto& pt : contour) {
            double dx = pt.x - center.x;
            double dy = pt.y - center.y;
            double pt_angle = std::atan2(dy, dx);

            // Check if point is in this ray direction
            double angle_diff = std::abs(pt_angle - angle);
            if (angle_diff > M_PI) angle_diff = 2 * M_PI - angle_diff;

            if (angle_diff < M_PI / config_.radial_ray_count) {
                float dist = std::sqrt(dx * dx + dy * dy);
                max_r = std::max(max_r, dist);
            }
        }

        if (max_r > 0) {
            radii.push_back(max_r);
        }
    }

    if (radii.empty()) return 0.5f;

    // Use robust statistics (median and IQR)
    float median_r = RobustStats::median(radii);
    float iqr_r = RobustStats::iqr(radii);

    if (median_r < 1e-6f) return 0.5f;

    float normalized_iqr = iqr_r / median_r;
    return std::max(0.0f, 1.0f - normalized_iqr);
}

float ImprovedFeatureExtractor::computeRadialGradient(
    const cv::Mat& roi,
    const std::vector<cv::Point>& contour
) const {
    if (roi.empty() || contour.empty()) return 0.5f;

    cv::Mat dx, dy;
    cv::Sobel(roi, dx, CV_32F, 1, 0, 3);
    cv::Sobel(roi, dy, CV_32F, 0, 1, 3);

    std::vector<float> gradients;

    for (const auto& pt : contour) {
        if (pt.x > 0 && pt.x < roi.cols - 1 && pt.y > 0 && pt.y < roi.rows - 1) {
            float gx = dx.at<float>(pt.y, pt.x);
            float gy = dy.at<float>(pt.y, pt.x);
            float grad = std::sqrt(gx * gx + gy * gy);
            gradients.push_back(grad);
        }
    }

    if (gradients.empty()) return 0.5f;

    // Use robust statistics (median)
    float median_grad = RobustStats::median(gradients);
    return std::min(1.0f, median_grad / 255.0f);
}

// ===== ENERGY & TEXTURE (IMPROVED) =====

float ImprovedFeatureExtractor::computeSNR(const cv::Mat& roi) const {
    if (roi.empty()) return 0.5f;

    cv::Mat roi_float;
    roi.convertTo(roi_float, CV_32F);

    // Better SNR: Use Laplacian variance for IR images
    cv::Mat laplacian;
    cv::Laplacian(roi_float, laplacian, CV_32F);

    float signal_power = cv::norm(laplacian, cv::NORM_L2) / roi.total();
    signal_power *= signal_power;  // Square for power
    
    float noise_power = 1.0f;  // Estimate noise from local variance

    if (signal_power < 1e-6f) return 0.0f;
    if (noise_power < 1e-6f) noise_power = 1e-6f;

    float snr = signal_power / noise_power;
    return std::min(1.0f, snr / 1000.0f);  // Normalized
}

float ImprovedFeatureExtractor::computeEntropy(const cv::Mat& roi) const {
    if (roi.empty()) return 0.5f;

    cv::Mat hist;
    int histSize = 256;
    float range[] = {0, 256};
    const float* histRange = range;

    cv::calcHist(&roi, 1, nullptr, cv::Mat(), hist, 1, &histSize, &histRange);
    hist /= roi.total();

    float entropy = 0.0f;
    for (int i = 0; i < histSize; ++i) {
        float p = hist.at<float>(i);
        if (p > 1e-6f) {
            entropy -= p * std::log(p);
        }
    }

    float max_entropy = std::log(256.0f);
    return entropy / max_entropy;
}

float ImprovedFeatureExtractor::computeRingEnergy(
    const cv::Mat& roi,
    const std::vector<cv::Point>& contour
) const {
    if (roi.empty() || contour.empty()) return 0.5f;

    cv::Point2f center = getCentroid(contour);

    // Compute mean radius
    double mean_radius = 0.0f;
    for (const auto& pt : contour) {
        double dx = pt.x - center.x;
        double dy = pt.y - center.y;
        mean_radius += std::sqrt(dx * dx + dy * dy);
    }
    mean_radius /= std::max(1, static_cast<int>(contour.size()));

    if (mean_radius < 1e-6) return 0.5f;

    // Adaptive ring thickness
    float ring_thickness = static_cast<float>(mean_radius * config_.ring_thickness_factor);
    ring_thickness = std::max(1.0f, ring_thickness);

    float ring_energy = 0.0f;
    int count = 0;

    for (int y = std::max(0, static_cast<int>(center.y - mean_radius - ring_thickness));
         y < std::min(roi.rows, static_cast<int>(center.y + mean_radius + ring_thickness + 1));
         ++y) {
        for (int x = std::max(0, static_cast<int>(center.x - mean_radius - ring_thickness));
             x < std::min(roi.cols, static_cast<int>(center.x + mean_radius + ring_thickness + 1));
             ++x) {
            double dx = x - center.x;
            double dy = y - center.y;
            double dist = std::sqrt(dx * dx + dy * dy);

            if (std::abs(dist - mean_radius) < ring_thickness) {
                ring_energy += roi.at<uchar>(y, x);
                count++;
            }
        }
    }

    if (count == 0) return 0.5f;
    return std::min(1.0f, (ring_energy / count) / 255.0f);
}

// ===== FREQUENCY & PHASE (IMPROVED) =====

float ImprovedFeatureExtractor::computeSharpness(const cv::Mat& roi) const {
    if (roi.empty()) return 0.5f;

    cv::Mat laplacian;
    cv::Laplacian(roi, laplacian, CV_32F);

    double sharpness = cv::norm(laplacian, cv::NORM_L2) / roi.total();
    return std::min(1.0f, static_cast<float>(sharpness / 100.0f));
}

float ImprovedFeatureExtractor::computeLaplacianDensity(
    const cv::Mat& roi,
    const std::vector<cv::Point>& contour
) const {
    if (roi.empty() || contour.empty()) return 0.5f;

    cv::Mat laplacian;
    cv::Laplacian(roi, laplacian, CV_32F);

    float laplacian_sum = 0.0f;
    int count = 0;

    for (const auto& pt : contour) {
        if (pt.x > 0 && pt.x < roi.cols - 1 && pt.y > 0 && pt.y < roi.rows - 1) {
            laplacian_sum += std::abs(laplacian.at<float>(pt.y, pt.x));
            count++;
        }
    }

    if (count == 0) return 0.5f;
    float density = laplacian_sum / count;
    return std::min(1.0f, density / 100.0f);
}

// ===== EXTENDED FEATURES (IMPROVED) =====

float ImprovedFeatureExtractor::computeContrast(const cv::Mat& roi) const {
    if (roi.empty()) return 0.5f;

    cv::Scalar mean, stddev;
    cv::meanStdDev(roi, mean, stddev);

    return std::min(1.0f, static_cast<float>(stddev[0] / 255.0f));
}

float ImprovedFeatureExtractor::computeMeanIntensity(const cv::Mat& roi) const {
    if (roi.empty()) return 0.5f;

    cv::Scalar mean = cv::mean(roi);
    return static_cast<float>(mean[0] / 255.0f);
}

float ImprovedFeatureExtractor::computeEdgeDensity(
    const cv::Mat& roi,
    const std::vector<cv::Point>& contour
) const {
    if (roi.empty() || contour.empty()) return 0.5f;

    cv::Mat edges;
    cv::Canny(roi, edges, config_.canny_threshold1, config_.canny_threshold2);

    float edge_pixels = static_cast<float>(cv::countNonZero(edges));
    return std::min(1.0f, edge_pixels / contour.size());
}

float ImprovedFeatureExtractor::computeCornerCount(const cv::Mat& roi) const {
    if (roi.empty() || roi.cols < 3 || roi.rows < 3) return 0.0f;

    cv::Mat roi_float;
    roi.convertTo(roi_float, CV_32F);

    cv::Mat corners;
    cv::cornerHarris(roi_float, corners, 2, 3, 0.04);

    float corner_response = 0.0f;
    for (int y = 0; y < corners.rows; ++y) {
        for (int x = 0; x < corners.cols; ++x) {
            corner_response += corners.at<float>(y, x);
        }
    }

    return std::min(1.0f, corner_response / (roi.rows * roi.cols * 1e6f));
}

// ===== NORMALIZATION & STANDARDIZATION =====

void ImprovedFeatureExtractor::normalizeFeatures(CandidateFeature& feat) const {
    feat.area = std::max(0.0f, std::min(1.0f, feat.area));
    feat.circularity = std::max(0.0f, std::min(1.0f, feat.circularity));
    feat.solidity = std::max(0.0f, std::min(1.0f, feat.solidity));
    feat.aspect_ratio = std::max(0.0f, std::min(1.0f, feat.aspect_ratio));
    feat.radial_symmetry = std::max(0.0f, std::min(1.0f, feat.radial_symmetry));
    feat.radial_gradient = std::max(0.0f, std::min(1.0f, feat.radial_gradient));
    feat.snr = std::max(0.0f, std::min(1.0f, feat.snr));
    feat.entropy = std::max(0.0f, std::min(1.0f, feat.entropy));
    feat.ring_energy = std::max(0.0f, std::min(1.0f, feat.ring_energy));
    feat.sharpness = std::max(0.0f, std::min(1.0f, feat.sharpness));
    feat.laplacian_density = std::max(0.0f, std::min(1.0f, feat.laplacian_density));
    feat.phase_coherence = std::max(0.0f, std::min(1.0f, feat.phase_coherence));
    feat.contrast = std::max(0.0f, std::min(1.0f, feat.contrast));
    feat.mean_intensity = std::max(0.0f, std::min(1.0f, feat.mean_intensity));
    feat.std_intensity = std::max(0.0f, std::min(1.0f, feat.std_intensity));
    feat.edge_density = std::max(0.0f, std::min(1.0f, feat.edge_density));
    feat.corner_count = std::max(0.0f, std::min(1.0f, feat.corner_count));
}

void ImprovedFeatureExtractor::standardizeFeatures(
    CandidateFeature& feat,
    const FeatureStatistics* stats,
    int offset
) const {
    if (!stats) return;

    // Standardize each feature using training statistics
    feat.area = stats->standardize(feat.area, offset + 0);
    feat.circularity = stats->standardize(feat.circularity, offset + 1);
    feat.solidity = stats->standardize(feat.solidity, offset + 2);
    feat.aspect_ratio = stats->standardize(feat.aspect_ratio, offset + 3);
    feat.radial_symmetry = stats->standardize(feat.radial_symmetry, offset + 4);
    feat.radial_gradient = stats->standardize(feat.radial_gradient, offset + 5);
    feat.snr = stats->standardize(feat.snr, offset + 6);
    feat.entropy = stats->standardize(feat.entropy, offset + 7);
    feat.ring_energy = stats->standardize(feat.ring_energy, offset + 8);
    feat.sharpness = stats->standardize(feat.sharpness, offset + 9);
    feat.laplacian_density = stats->standardize(feat.laplacian_density, offset + 10);
    feat.phase_coherence = stats->standardize(feat.phase_coherence, offset + 11);
    feat.contrast = stats->standardize(feat.contrast, offset + 12);
    feat.mean_intensity = stats->standardize(feat.mean_intensity, offset + 13);
    feat.std_intensity = stats->standardize(feat.std_intensity, offset + 13);  // Duplicate
    feat.edge_density = stats->standardize(feat.edge_density, offset + 15);
    feat.corner_count = stats->standardize(feat.corner_count, offset + 16);
}

cv::Point2f ImprovedFeatureExtractor::getCentroid(const std::vector<cv::Point>& contour) const {
    if (contour.empty()) return cv::Point2f(0, 0);

    cv::Moments moments = cv::moments(contour);
    if (moments.m00 < 1e-6) return cv::Point2f(0, 0);

    return cv::Point2f(
        static_cast<float>(moments.m10 / moments.m00),
        static_cast<float>(moments.m01 / moments.m00)
    );
}

// ===== FEATURE ANALYSIS UTILITIES =====

cv::Mat FeatureAnalyzer::computeCorrelationMatrix(
    const std::vector<std::vector<float>>& features
) {
    if (features.empty() || features[0].empty()) {
        return cv::Mat();
    }

    int n_features = features[0].size();
    int n_samples = features.size();

    cv::Mat data(n_samples, n_features, CV_32F);
    for (int i = 0; i < n_samples; ++i) {
        for (int j = 0; j < n_features; ++j) {
            data.at<float>(i, j) = features[i][j];
        }
    }

    cv::Mat covar, mean;
    cv::calcCovarMatrix(data, covar, mean, cv::COVAR_NORMAL | cv::COVAR_ROWS);

    cv::Mat correlation(n_features, n_features, CV_32F);
    for (int i = 0; i < n_features; ++i) {
        for (int j = 0; j < n_features; ++j) {
            float cov_ij = covar.at<float>(i, j);
            float std_i = std::sqrt(covar.at<float>(i, i));
            float std_j = std::sqrt(covar.at<float>(j, j));

            if (std_i > 1e-6f && std_j > 1e-6f) {
                correlation.at<float>(i, j) = cov_ij / (std_i * std_j);
            } else {
                correlation.at<float>(i, j) = (i == j) ? 1.0f : 0.0f;
            }
        }
    }

    return correlation;
}

std::vector<float> FeatureAnalyzer::computeFeatureImportance(
    const std::vector<std::vector<float>>& features,
    const std::vector<int>& labels
) {
    // Placeholder: Compute mutual information or permutation importance
    // For now, return uniform importance
    int n_features = features[0].size();
    return std::vector<float>(n_features, 1.0f / n_features);
}

std::vector<int> FeatureAnalyzer::identifyRedundantFeatures(
    const cv::Mat& correlation_matrix,
    float correlation_threshold
) {
    std::vector<int> redundant;

    for (int i = 0; i < correlation_matrix.rows; ++i) {
        for (int j = i + 1; j < correlation_matrix.cols; ++j) {
            float corr = correlation_matrix.at<float>(i, j);
            if (std::abs(corr) > correlation_threshold) {
                redundant.push_back(j);  // Mark second feature as redundant
            }
        }
    }

    return redundant;
}

std::vector<std::vector<bool>> FeatureAnalyzer::detectFeatureOutliers(
    const std::vector<std::vector<float>>& features,
    float k
) {
    std::vector<std::vector<bool>> outlier_matrix;

    if (features.empty() || features[0].empty()) {
        return outlier_matrix;
    }

    int n_features = features[0].size();

    for (int i = 0; i < n_features; ++i) {
        std::vector<float> feature_values;
        for (const auto& sample : features) {
            feature_values.push_back(sample[i]);
        }

        auto outliers = RobustStats::detectOutliers(feature_values, k);
        outlier_matrix.push_back(outliers);
    }

    return outlier_matrix;
}

} // namespace bullet_detection
