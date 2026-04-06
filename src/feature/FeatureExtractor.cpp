#include "feature/FeatureExtractor.h"
#include <cmath>
#include <numeric>
#include <algorithm>
#include "core/MathUtils.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace bullet_detection {

FeatureExtractor::FeatureExtractor() {}

CandidateFeature FeatureExtractor::extractFeatures(
    const cv::Mat& roi_gray,
    const std::vector<cv::Point>& contour
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

    // Frequency & Phase (9-11)
    feat.sharpness = computeSharpness(roi_gray);
    feat.laplacian_density = computeLaplacianDensity(roi_gray, contour);
    feat.phase_coherence = computePhaseCoherence(roi_gray);

    // Extended (12-16)
    feat.contrast = computeContrast(roi_gray);
    feat.mean_intensity = computeMeanIntensity(roi_gray);
    feat.std_intensity = computeStdIntensity(roi_gray);
    feat.edge_density = computeEdgeDensity(roi_gray, contour);
    feat.corner_count = computeCornerCount(roi_gray);

    normalizeFeatures(feat);
    return feat;
}

FeatureVector FeatureExtractor::extractFeatureVector(
    const cv::Mat& roi_gray,
    const std::vector<cv::Point>& contour
) {
    return extractFeatures(roi_gray, contour).toFeatureVector();
}

// ===== GEOMETRIC FEATURES =====

float FeatureExtractor::computeArea(const std::vector<cv::Point>& contour) {
    if (contour.empty()) return 0.0f;
    double area = cv::contourArea(contour);
    return std::min(1.0f, static_cast<float>(area / 2000.0f));
}

float FeatureExtractor::computeCircularity(const std::vector<cv::Point>& contour) {
    if (contour.empty()) return 0.0f;
    double perimeter = cv::arcLength(contour, true);
    double area = cv::contourArea(contour);
    // Use shared utility
    return math_utils::computeCircularity(area, perimeter);
}

float FeatureExtractor::computeSolidity(const std::vector<cv::Point>& contour) {
    if (contour.empty()) return 0.0f;
    std::vector<cv::Point> hull;
    cv::convexHull(contour, hull);
    double contour_area = cv::contourArea(contour);
    double hull_area = cv::contourArea(hull);
    // Use shared utility
    return math_utils::computeSolidity(contour_area, hull_area);
}

float FeatureExtractor::computeAspectRatio(const std::vector<cv::Point>& contour) {
    if (contour.empty()) return 0.5f;
    cv::Rect bbox = cv::boundingRect(contour);
    // Use shared utility
    return math_utils::computeAspectRatio(bbox.width, bbox.height);
}

// ===== RADIAL STRUCTURE FEATURES =====

float FeatureExtractor::computeRadialSymmetry(
    const cv::Mat& roi,
    const std::vector<cv::Point>& contour
) {
    if (contour.empty() || roi.empty()) return 0.5f;

    cv::Moments moments = cv::moments(contour);
    if (moments.m00 < 1e-6) return 0.5f;

    double cx = moments.m10 / moments.m00;
    double cy = moments.m01 / moments.m00;

    int n_rays = 16;
    std::vector<float> radii(n_rays, 0.0f);

    for (const auto& pt : contour) {
        double angle = std::atan2(pt.y - cy, pt.x - cx);
        int ray_idx = static_cast<int>((angle + M_PI) / (2.0 * M_PI) * n_rays) % n_rays;
        float dist = math_utils::distance2D(static_cast<float>(pt.x), static_cast<float>(pt.y),
                                           static_cast<float>(cx), static_cast<float>(cy));
        radii[ray_idx] = std::max(radii[ray_idx], dist);
    }

    float mean_r = std::accumulate(radii.begin(), radii.end(), 0.0f) / n_rays;
    float variance = 0.0f;
    for (float r : radii) {
        variance += (r - mean_r) * (r - mean_r);
    }
    variance /= n_rays;

    if (mean_r < 1e-6) return 0.5f;
    float coefficient_of_variation = std::sqrt(variance) / mean_r;
    return std::max(0.0f, 1.0f - coefficient_of_variation);
}

float FeatureExtractor::computeRadialGradient(
    const cv::Mat& roi,
    const std::vector<cv::Point>& contour
) {
    if (roi.empty() || contour.empty()) return 0.5f;

    cv::Mat dx, dy;
    cv::Sobel(roi, dx, CV_32F, 1, 0, 3);
    cv::Sobel(roi, dy, CV_32F, 0, 1, 3);

    float gradient_sum = 0.0f;
    int count = 0;

    for (const auto& pt : contour) {
        if (pt.x > 0 && pt.x < roi.cols - 1 && pt.y > 0 && pt.y < roi.rows - 1) {
            float gx = dx.at<float>(pt.y, pt.x);
            float gy = dy.at<float>(pt.y, pt.x);
            gradient_sum += std::sqrt(gx * gx + gy * gy);
            count++;
        }
    }

    if (count == 0) return 0.5f;
    float avg_gradient = gradient_sum / count;
    return std::min(1.0f, avg_gradient / 255.0f);
}

// ===== ENERGY & TEXTURE FEATURES =====

float FeatureExtractor::computeSNR(const cv::Mat& roi) {
    if (roi.empty()) return 0.5f;

    cv::Mat roi_float;
    roi.convertTo(roi_float, CV_32F);

    double mean = cv::mean(roi_float)[0];
    cv::Scalar mean_val, stddev_val;
    cv::meanStdDev(roi_float, mean_val, stddev_val);

    if (stddev_val[0] < 1e-6) return 0.0f;
    float snr = static_cast<float>(mean_val[0] / stddev_val[0]);
    return std::min(1.0f, snr / 10.0f);
}

float FeatureExtractor::computeEntropy(const cv::Mat& roi) {
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
        if (p > 0.0f) {
            entropy -= p * std::log(p);
        }
    }

    float max_entropy = std::log(256.0f);
    return entropy / max_entropy;
}

float FeatureExtractor::computeRingEnergy(
    const cv::Mat& roi,
    const std::vector<cv::Point>& contour
) {
    if (roi.empty() || contour.empty()) return 0.5f;

    cv::Moments moments = cv::moments(contour);
    if (moments.m00 < 1e-6) return 0.5f;

    double cx = moments.m10 / moments.m00;
    double cy = moments.m01 / moments.m00;

    double mean_radius = 0.0f;
    for (const auto& pt : contour) {
        mean_radius += std::sqrt((pt.x - cx) * (pt.x - cx) + (pt.y - cy) * (pt.y - cy));
    }
    mean_radius /= std::max(1, static_cast<int>(contour.size()));

    float ring_energy = 0.0f;
    int count = 0;

    for (int y = std::max(0, static_cast<int>(cy - mean_radius - 1));
         y < std::min(roi.rows, static_cast<int>(cy + mean_radius + 2)); ++y) {
        for (int x = std::max(0, static_cast<int>(cx - mean_radius - 1));
             x < std::min(roi.cols, static_cast<int>(cx + mean_radius + 2)); ++x) {
            double dist = std::sqrt((x - cx) * (x - cx) + (y - cy) * (y - cy));
            if (std::abs(dist - mean_radius) < 2.0) {
                ring_energy += roi.at<uchar>(y, x);
                count++;
            }
        }
    }

    if (count == 0) return 0.5f;
    return std::min(1.0f, (ring_energy / count) / 255.0f);
}

// ===== FREQUENCY & PHASE FEATURES =====

float FeatureExtractor::computeSharpness(const cv::Mat& roi) {
    if (roi.empty()) return 0.5f;

    cv::Mat laplacian;
    cv::Laplacian(roi, laplacian, CV_32F);

    double sharpness = cv::norm(laplacian, cv::NORM_L2) / roi.total();
    return std::min(1.0f, static_cast<float>(sharpness / 100.0f));
}

float FeatureExtractor::computeLaplacianDensity(
    const cv::Mat& roi,
    const std::vector<cv::Point>& contour
) {
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

float FeatureExtractor::computePhaseCoherence(const cv::Mat& roi) {
    if (roi.empty()) return 0.5f;
    // Simplified: use Fourier transform phase coherence
    // For production, use more sophisticated phase congruency algorithms

    cv::Mat roi_float;
    roi.convertTo(roi_float, CV_32F);

    cv::Mat padded;
    int m = cv::getOptimalDFTSize(roi_float.rows);
    int n = cv::getOptimalDFTSize(roi_float.cols);
    cv::copyMakeBorder(roi_float, padded, 0, m - roi_float.rows, 0, n - roi_float.cols,
                       cv::BORDER_CONSTANT, cv::Scalar::all(0));

    cv::Mat planes[] = {padded, cv::Mat::zeros(padded.size(), CV_32F)};
    cv::Mat complexI;
    cv::merge(planes, 2, complexI);

    cv::dft(complexI, complexI);
    cv::split(complexI, planes);

    cv::magnitude(planes[0], planes[1], planes[0]);
    float magnitude = static_cast<float>(cv::norm(planes[0]));

    return std::min(1.0f, magnitude / (roi.rows * roi.cols * 255.0f));
}

// ===== EXTENDED FEATURES =====

float FeatureExtractor::computeContrast(const cv::Mat& roi) {
    if (roi.empty()) return 0.5f;

    cv::Scalar mean, stddev;
    cv::meanStdDev(roi, mean, stddev);

    return std::min(1.0f, static_cast<float>(stddev[0] / 255.0f));
}

float FeatureExtractor::computeMeanIntensity(const cv::Mat& roi) {
    if (roi.empty()) return 0.5f;

    cv::Scalar mean = cv::mean(roi);
    return static_cast<float>(mean[0] / 255.0f);
}

float FeatureExtractor::computeStdIntensity(const cv::Mat& roi) {
    if (roi.empty()) return 0.5f;

    cv::Scalar mean, stddev;
    cv::meanStdDev(roi, mean, stddev);

    return static_cast<float>(stddev[0] / 255.0f);
}

float FeatureExtractor::computeEdgeDensity(
    const cv::Mat& roi,
    const std::vector<cv::Point>& contour
) {
    if (roi.empty() || contour.empty()) return 0.5f;

    cv::Mat edges;
    cv::Canny(roi, edges, 50, 150);

    float edge_pixels = static_cast<float>(cv::countNonZero(edges));
    return std::min(1.0f, edge_pixels / contour.size());
}

float FeatureExtractor::computeCornerCount(const cv::Mat& roi) {
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

void FeatureExtractor::normalizeFeatures(CandidateFeature& feat) {
    // Ensure all features are in [0, 1]
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

} // namespace bullet_detection

