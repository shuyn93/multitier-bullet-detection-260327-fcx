#include "candidate/RobustNoiseFilter.h"
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cmath>
#include <numeric>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace bullet_detection {

RobustNoiseFilter::RobustNoiseFilter()
    : current_confidence_threshold_(confidence_threshold_balanced_) {}

std::vector<CandidateRegion> RobustNoiseFilter::filterAndScoreCandidates(
    const std::vector<CandidateRegion>& candidates,
    const cv::Mat& original_frame
) {
    std::vector<CandidateRegion> filtered_candidates;

    for (const auto& candidate : candidates) {
        // Analyze blob
        auto analysis = analyzeBlob(candidate.roi_image, candidate.contour);

        // Check against threshold
        if (analysis.overall_confidence >= current_confidence_threshold_) {
            // Create copy with updated confidence
            CandidateRegion updated = candidate;
            updated.detection_score = analysis.overall_confidence;

            filtered_candidates.push_back(updated);
        }
    }

    return filtered_candidates;
}

RobustNoiseFilter::NoiseAnalysis RobustNoiseFilter::analyzeBlob(
    const cv::Mat& roi,
    const std::vector<cv::Point>& contour
) {
    NoiseAnalysis analysis = {};

    if (roi.empty() || contour.empty()) {
        analysis.overall_confidence = 0.5f;
        analysis.noise_risk = 0.5f;
        return analysis;
    }

    // ===== TEXTURE ANALYSIS =====
    if (texture_enabled_) {
        float lbp_score = computeLBPUniformity(roi);
        float hog_score = computeHOGConsistency(roi);
        float gabor_score = computeGaborRadialResponse(roi);

        float texture_combined = (lbp_score * lbp_weight_ +
                                  hog_score * hog_weight_ +
                                  gabor_score * gabor_weight_) /
                                 (lbp_weight_ + hog_weight_ + gabor_weight_);

        analysis.texture_score = std::max(0.0f, std::min(1.0f, texture_combined));
    } else {
        analysis.texture_score = 0.5f;
    }

    // ===== FREQUENCY ANALYSIS =====
    if (frequency_enabled_) {
        float fft_score = computeFFTRadialFrequency(roi);
        float wavelet_score = computeWaveletConsistency(roi);
        float phase_score = computePhaseCoherence(roi);

        float frequency_combined = (fft_score * fft_weight_ +
                                    wavelet_score * wavelet_weight_ +
                                    phase_score * phase_weight_) /
                                   (fft_weight_ + wavelet_weight_ + phase_weight_);

        analysis.frequency_score = std::max(0.0f, std::min(1.0f, frequency_combined));
    } else {
        analysis.frequency_score = 0.5f;
    }

    // ===== RADIAL PROFILE ANALYSIS =====
    if (profile_enabled_) {
        float profile_score = analyzeRadialIntensityProfile(roi, contour);
        float derivative_score = computeRadialDerivativeSmoothed(roi, contour);
        float gaussian_score = fitGaussianProfile(roi);

        float profile_combined = (profile_score * intensity_profile_weight_ +
                                  derivative_score * derivative_weight_ +
                                  gaussian_score * gaussian_fit_weight_) /
                                 (intensity_profile_weight_ + derivative_weight_ +
                                  gaussian_fit_weight_);

        analysis.intensity_profile_score = std::max(0.0f, std::min(1.0f, profile_combined));
    } else {
        analysis.intensity_profile_score = 0.5f;
    }

    // ===== BORDER ANALYSIS =====
    if (border_enabled_) {
        float boundary_score = analyzeBoundarySharpness(roi, contour);
        float regularity_score = computeEdgeRegularity(contour);
        float artifact_score = 1.0f - detectBoundaryArtifacts(roi);  // Invert

        float border_combined = (boundary_score * boundary_sharpness_weight_ +
                                 regularity_score * edge_regularity_weight_ +
                                 artifact_score * artifact_detection_weight_) /
                                (boundary_sharpness_weight_ + edge_regularity_weight_ +
                                 artifact_detection_weight_);

        analysis.border_quality_score = std::max(0.0f, std::min(1.0f, border_combined));
    } else {
        analysis.border_quality_score = 0.5f;
    }

    // ===== COMPACTNESS SCORE =====
    // Normalized second moment (compactness)
    double area = cv::contourArea(contour);
    double perimeter = cv::arcLength(contour, true);

    if (perimeter > 0 && area > 0) {
        // Compactness = 4?*A / P²
        float compactness = static_cast<float>(4 * M_PI * area / (perimeter * perimeter));
        analysis.compactness_score = std::max(0.0f, std::min(1.0f, compactness));
    } else {
        analysis.compactness_score = 0.5f;
    }

    // ===== OVERALL CONFIDENCE (Weighted Combination) =====
    float total_weight = 0.3f + 0.25f + 0.25f + 0.1f + 0.1f;  // Arbitrary weights

    analysis.overall_confidence = (analysis.texture_score * 0.3f +
                                   analysis.frequency_score * 0.25f +
                                   analysis.intensity_profile_score * 0.25f +
                                   analysis.border_quality_score * 0.1f +
                                   analysis.compactness_score * 0.1f) /
                                  total_weight;

    analysis.overall_confidence = std::max(0.0f, std::min(1.0f, analysis.overall_confidence));

    // ===== NOISE RISK =====
    // Inverse of confidence with adjustments for outliers
    analysis.noise_risk = 1.0f - analysis.overall_confidence;
    analysis.noise_risk += computeOutlierProbability(analysis) * 0.1f;  // Boost if outlier
    analysis.noise_risk = std::max(0.0f, std::min(1.0f, analysis.noise_risk));

    return analysis;
}

// ===== TEXTURE ANALYSIS =====

float RobustNoiseFilter::computeLBPUniformity(const cv::Mat& roi) {
    if (roi.empty()) return 0.5f;

    cv::Mat lbp_image;
    computeLBP(roi, lbp_image);

    // Compute histogram of LBP values
    cv::Mat hist;
    int histSize = 256;
    float range[] = {0, 256};
    const float* histRange = range;

    cv::calcHist(&lbp_image, 1, 0, cv::Mat(), hist, 1, &histSize, &histRange);
    hist = hist / lbp_image.total();  // Normalize

    // Entropy of LBP histogram
    // High entropy = many different LBP patterns = textured (noise)
    // Low entropy = few patterns = uniform (bullet hole)
    float entropy = 0.0f;
    for (int i = 0; i < histSize; ++i) {
        float p = hist.at<float>(i, 0);
        if (p > 0) {
            entropy -= p * std::log(p);
        }
    }

    // Normalize entropy [0,1] (max entropy ? 5.5 for 256 bins)
    entropy = entropy / std::log(static_cast<float>(histSize));

    // Invert: low entropy ? high uniformity
    float uniformity = 1.0f - entropy;
    return std::max(0.0f, std::min(1.0f, uniformity));
}

float RobustNoiseFilter::computeHOGConsistency(const cv::Mat& roi) {
    if (roi.empty()) return 0.5f;

    cv::Mat hog_response = computeHOG(roi);

    if (hog_response.empty()) return 0.5f;

    // HOG consistency: measure how concentrated gradients are
    // Bullets have radial gradients (concentrated)
    // Noise has random gradients (scattered)

    cv::Scalar mean_val = cv::mean(hog_response);
    cv::Scalar std_val;

    int n = hog_response.total();
    float sum_sq_diff = 0.0f;

    for (int i = 0; i < n; ++i) {
        float val = hog_response.at<float>(i);
        sum_sq_diff += (val - mean_val[0]) * (val - mean_val[0]);
    }

    float std_dev = std::sqrt(sum_sq_diff / n);

    // CV (coefficient of variation)
    float cv = mean_val[0] > 0.1f ? std_dev / mean_val[0] : 1.0f;

    // Low CV = consistent = bullet
    // High CV = inconsistent = noise
    float consistency = 1.0f / (1.0f + cv);
    return std::max(0.0f, std::min(1.0f, consistency));
}

float RobustNoiseFilter::computeGaborRadialResponse(const cv::Mat& roi) {
    if (roi.empty()) return 0.5f;

    auto gabor_responses = applyGaborFilters(roi);

    if (gabor_responses.empty()) return 0.5f;

    // Gabor filters tuned for radial patterns
    // Compute response strength
    float total_response = 0.0f;

    for (const auto& response : gabor_responses) {
        total_response += cv::sum(cv::abs(response))[0];
    }

    float avg_response = total_response / (gabor_responses.size() * roi.total());

    // Normalize (typical range 0-100)
    float radial_score = avg_response / 100.0f;
    return std::max(0.0f, std::min(1.0f, radial_score));
}

// ===== FREQUENCY ANALYSIS =====

float RobustNoiseFilter::computeFFTRadialFrequency(const cv::Mat& roi) {
    if (roi.empty()) return 0.5f;

    cv::Mat float_roi;
    roi.convertTo(float_roi, CV_32F);

    // FFT
    cv::Mat fft_result;
    cv::dft(float_roi, fft_result, cv::DFT_COMPLEX_OUTPUT);

    // Shift zero frequency to center
    cv::Mat fft_shift;
    fft_shift = fft_result.clone();

    // Compute magnitude spectrum
    std::vector<cv::Mat> planes;
    cv::split(fft_shift, planes);

    cv::Mat magnitude;
    cv::magnitude(planes[0], planes[1], magnitude);

    // Analyze power in radial vs random frequencies
    // Bullets have dominant frequency (circular blob)
    // Noise has uniform frequency distribution

    // Simple metric: peak vs average ratio
    double min_val, max_val;
    cv::minMaxLoc(magnitude, &min_val, &max_val);

    float avg_magnitude = static_cast<float>(cv::mean(magnitude)[0]);

    float peak_ratio = avg_magnitude > 1.0f ? static_cast<float>(max_val) / avg_magnitude : 1.0f;

    // Normalize (typical range 1-100)
    float frequency_score = peak_ratio / 100.0f;
    return std::max(0.0f, std::min(1.0f, frequency_score));
}

float RobustNoiseFilter::computeWaveletConsistency(const cv::Mat& roi) {
    if (roi.empty()) return 0.5f;

    // Simplified: use Laplacian as approximation of wavelet
    cv::Mat laplacian;
    cv::Laplacian(roi, laplacian, CV_32F);

    // Compute variance of Laplacian at different scales
    cv::Scalar mean_val = cv::mean(cv::abs(laplacian));
    cv::Scalar std_val;

    float mean_abs_laplacian = static_cast<float>(mean_val[0]);

    // High Laplacian = many edges = structured (bullet)
    // Low Laplacian = few edges = smooth (noise or blank)
    float consistency = mean_abs_laplacian / 100.0f;  // Normalize
    return std::max(0.0f, std::min(1.0f, consistency));
}

float RobustNoiseFilter::computePhaseCoherence(const cv::Mat& roi) {
if (roi.empty()) return 0.5f;

cv::Mat float_roi;
roi.convertTo(float_roi, CV_32F);
float_roi = float_roi / 255.0f;

// FFT
cv::Mat fft_result;
cv::dft(float_roi, fft_result, cv::DFT_COMPLEX_OUTPUT);

    // Extract phase
    std::vector<cv::Mat> planes;
    cv::split(fft_result, planes);

    cv::Mat phase;
    cv::phase(planes[0], planes[1], phase);

    // Phase coherence = how aligned phases are
    // Mean of exp(i*phase)
    cv::Mat exp_phase;
    cv::exp(phase * cv::Scalar(0, 1), exp_phase);  // e^(i*phase)

    cv::Scalar mean_exp = cv::mean(exp_phase);

    // Magnitude of mean complex exponential
    float phase_coherence = std::sqrt(mean_exp[0] * mean_exp[0] + mean_exp[1] * mean_exp[1]);
    return std::max(0.0f, std::min(1.0f, phase_coherence));
}

// ===== RADIAL PROFILE ANALYSIS =====

float RobustNoiseFilter::analyzeRadialIntensityProfile(
    const cv::Mat& roi,
    const std::vector<cv::Point>& contour
) {
    if (roi.empty() || contour.empty()) return 0.5f;

    // Get centroid
    cv::Moments moments = cv::moments(contour);
    if (moments.m00 == 0) return 0.5f;

    cv::Point2f center(
        static_cast<float>(moments.m10 / moments.m00),
        static_cast<float>(moments.m01 / moments.m00)
    );

    // Extract radial profile
    auto profile = extractRadialProfile(roi, center, 64);

    if (profile.empty() || profile.size() < 10) return 0.5f;

    // Smooth profile
    auto smoothed = smoothProfile(profile, 5);

    // Check if profile is monotonically decreasing (bullet hole characteristic)
    int decreasing_count = 0;
    for (size_t i = 1; i < smoothed.size(); ++i) {
        if (smoothed[i] < smoothed[i - 1]) {
            decreasing_count++;
        }
    }

    float monotonicity = static_cast<float>(decreasing_count) / smoothed.size();

    // Bullets have smooth, monotonically decreasing profiles
    // Noise has jagged, random profiles
    return std::max(0.0f, std::min(1.0f, monotonicity));
}

float RobustNoiseFilter::computeRadialDerivativeSmoothed(
    const cv::Mat& roi,
    const std::vector<cv::Point>& contour
) {
    if (roi.empty() || contour.empty()) return 0.5f;

    cv::Moments moments = cv::moments(contour);
    if (moments.m00 == 0) return 0.5f;

    cv::Point2f center(
        static_cast<float>(moments.m10 / moments.m00),
        static_cast<float>(moments.m01 / moments.m00)
    );

    auto profile = extractRadialProfile(roi, center, 64);
    if (profile.empty() || profile.size() < 10) return 0.5f;

    auto smoothed = smoothProfile(profile, 5);
    auto derivative = computeDerivative(smoothed);

    if (derivative.empty()) return 0.5f;

    // Compute smoothness of derivative (second derivative)
    // Smooth derivative = consistent falloff (bullet)
    // Jagged derivative = random pattern (noise)
    auto second_deriv = computeDerivative(derivative);

    float sum_sq = 0.0f;
    for (float val : second_deriv) {
        sum_sq += val * val;
    }

    float smoothness = 1.0f / (1.0f + std::sqrt(sum_sq / second_deriv.size()));
    return std::max(0.0f, std::min(1.0f, smoothness));
}

float RobustNoiseFilter::fitGaussianProfile(const cv::Mat& roi) {
    if (roi.empty()) return 0.5f;

    // Get centroid
    cv::Mat binary;
    cv::threshold(roi, binary, cv::mean(roi)[0], 255, cv::THRESH_BINARY);

    cv::Moments moments = cv::moments(binary);
    if (moments.m00 == 0) return 0.5f;

    cv::Point2f center(
        static_cast<float>(moments.m10 / moments.m00),
        static_cast<float>(moments.m01 / moments.m00)
    );

    auto profile = extractRadialProfile(roi, center, 64);
    if (profile.empty()) return 0.5f;

    // Fit to Gaussian: exp(-r²/?²)
    float gaussian_fit = fitPolynomial(profile, 2);  // 0-1 goodness
    return std::max(0.0f, std::min(1.0f, gaussian_fit));
}

// ===== BORDER ANALYSIS =====

float RobustNoiseFilter::analyzeBoundarySharpness(
    const cv::Mat& roi,
    const std::vector<cv::Point>& contour
) {
    if (roi.empty() || contour.empty()) return 0.5f;

    // Compute gradient magnitude near contour
    cv::Mat gradx, grady;
    cv::Sobel(roi, gradx, CV_32F, 1, 0, 3);
    cv::Sobel(roi, grady, CV_32F, 0, 1, 3);

    cv::Mat gradient_magnitude;
    cv::magnitude(gradx, grady, gradient_magnitude);

    // Sample gradient at boundary points
    std::vector<float> boundary_gradients;

    for (const auto& pt : contour) {
        if (pt.x >= 0 && pt.x < gradient_magnitude.cols &&
            pt.y >= 0 && pt.y < gradient_magnitude.rows) {
            boundary_gradients.push_back(gradient_magnitude.at<float>(pt.y, pt.x));
        }
    }

    if (boundary_gradients.empty()) return 0.5f;

    // Sharpness = mean gradient at boundary
    float avg_gradient = std::accumulate(boundary_gradients.begin(),
                                         boundary_gradients.end(), 0.0f) /
                         boundary_gradients.size();

    // Normalize (typical range 0-100)
    float sharpness = avg_gradient / 100.0f;
    return std::max(0.0f, std::min(1.0f, sharpness));
}

float RobustNoiseFilter::computeEdgeRegularity(const std::vector<cv::Point>& contour) {
    if (contour.size() < 10) return 0.5f;

    // Compute distances between consecutive points
    std::vector<float> distances;

    for (size_t i = 0; i < contour.size(); ++i) {
        cv::Point p1 = contour[i];
        cv::Point p2 = contour[(i + 1) % contour.size()];

        float dist = std::sqrt((p1.x - p2.x) * (p1.x - p2.x) +
                               (p1.y - p2.y) * (p1.y - p2.y));
        distances.push_back(dist);
    }

    // Regularity: low variance in distances = regular edge (bullet)
    float mean_dist = std::accumulate(distances.begin(), distances.end(), 0.0f) /
                      distances.size();

    float variance = 0.0f;
    for (float d : distances) {
        variance += (d - mean_dist) * (d - mean_dist);
    }
    variance /= distances.size();

    float cv = mean_dist > 0.1f ? std::sqrt(variance) / mean_dist : 1.0f;

    // Low CV = regular = bullet
    float regularity = 1.0f / (1.0f + cv);
    return std::max(0.0f, std::min(1.0f, regularity));
}

float RobustNoiseFilter::detectBoundaryArtifacts(const cv::Mat& roi) {
    if (roi.empty()) return 0.0f;

    // Look for salt-and-pepper noise at boundary
    // This is a simplified check
    cv::Mat binary;
    cv::threshold(roi, binary, cv::mean(roi)[0] + 20, 255, cv::THRESH_BINARY);

    // Morphological opening removes small artifacts
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(3, 3));
    cv::Mat opened;
    cv::morphologyEx(binary, opened, cv::MORPH_OPEN, kernel);

    // Compare before/after
    cv::Mat diff;
    cv::absdiff(binary, opened, diff);

    float artifact_ratio = static_cast<float>(cv::countNonZero(diff)) / binary.total();
    return std::max(0.0f, std::min(1.0f, artifact_ratio));
}

// ===== SEPARABILITY METRICS =====

float RobustNoiseFilter::computeSeparabilityIndex(const NoiseAnalysis& analysis) {
    // Fisher-like separability between bullet hole and noise
    // Higher = better separation
    float bullet_confidence = analysis.overall_confidence;
    float distance_from_bullet = std::abs(bullet_confidence - BULLET_HOLE_MEAN_CONFIDENCE);
    float distance_from_noise = std::abs(bullet_confidence - NOISE_MEAN_CONFIDENCE);

    float separability = distance_from_noise / (distance_from_bullet + distance_from_noise + 0.01f);
    return std::max(0.0f, std::min(1.0f, separability));
}

float RobustNoiseFilter::computeOutlierProbability(const NoiseAnalysis& analysis) {
    // Check if blob is an outlier in feature space
    // (Too different from typical bullets or noise)

    float total_dev = 0.0f;
    float n_features = 0.0f;

    // Deviation from bullet mean
    if (analysis.overall_confidence > 0.5f) {
        total_dev += std::abs(analysis.overall_confidence - BULLET_HOLE_MEAN_CONFIDENCE);
        n_features++;
    } else {
        total_dev += std::abs(analysis.overall_confidence - NOISE_MEAN_CONFIDENCE);
        n_features++;
    }

    // Feature consistency check
    std::vector<float> features = {analysis.texture_score,
                                    analysis.frequency_score,
                                    analysis.intensity_profile_score,
                                    analysis.border_quality_score};

    float mean_feature = std::accumulate(features.begin(), features.end(), 0.0f) /
                         features.size();
    for (float f : features) {
        total_dev += std::abs(f - mean_feature);
        n_features++;
    }

    float avg_dev = total_dev / n_features;

    // High deviation = outlier
    float outlier_prob = avg_dev;
    return std::max(0.0f, std::min(1.0f, outlier_prob));
}

// ===== HELPER FUNCTIONS =====

std::vector<float> RobustNoiseFilter::extractRadialProfile(
    const cv::Mat& roi,
    const cv::Point2f& center,
    int num_samples
) {
    std::vector<float> profile;

    if (roi.empty()) return profile;

    float max_radius = std::min(roi.cols, roi.rows) / 2.0f;

    for (int i = 0; i < num_samples; ++i) {
        float r = (i + 1) * max_radius / num_samples;

        // Sample at this radius (average over 8 directions)
        float sample_sum = 0.0f;
        int count = 0;

        for (int angle = 0; angle < 8; ++angle) {
            float theta = 2.0f * M_PI * angle / 8;
            int x = static_cast<int>(center.x + r * std::cos(theta));
            int y = static_cast<int>(center.y + r * std::sin(theta));

            if (x >= 0 && x < roi.cols && y >= 0 && y < roi.rows) {
                sample_sum += roi.at<uchar>(y, x);
                count++;
            }
        }

        if (count > 0) {
            profile.push_back(sample_sum / count);
        }
    }

    return profile;
}

std::vector<float> RobustNoiseFilter::smoothProfile(
    const std::vector<float>& profile,
    int window_size
) {
    if (profile.size() < static_cast<size_t>(window_size)) {
        return profile;
    }

    std::vector<float> smoothed;
    int half_window = window_size / 2;

    for (size_t i = 0; i < profile.size(); ++i) {
        int start = std::max(0, static_cast<int>(i) - half_window);
        int end = std::min(static_cast<int>(profile.size()), static_cast<int>(i) + half_window + 1);

        float sum = 0.0f;
        int count = end - start;

        for (int j = start; j < end; ++j) {
            sum += profile[j];
        }

        smoothed.push_back(sum / count);
    }

    return smoothed;
}

std::vector<float> RobustNoiseFilter::computeDerivative(const std::vector<float>& profile) {
    std::vector<float> derivative;

    for (size_t i = 1; i < profile.size(); ++i) {
        derivative.push_back(profile[i] - profile[i - 1]);
    }

    return derivative;
}

std::vector<float> RobustNoiseFilter::computeCurvature(const std::vector<float>& profile) {
    auto deriv1 = computeDerivative(profile);
    auto deriv2 = computeDerivative(deriv1);
    return deriv2;
}

float RobustNoiseFilter::fitPolynomial(const std::vector<float>& profile, int degree) {
    // Simplified: just check R² value
    if (profile.size() < 10) return 0.5f;

    // For now, return dummy value (full implementation would use polynomial fitting)
    float mean = std::accumulate(profile.begin(), profile.end(), 0.0f) / profile.size();

    float ss_tot = 0.0f;
    for (float val : profile) {
        ss_tot += (val - mean) * (val - mean);
    }

    // Simplified R² (would be actual polynomial fit)
    float r_squared = std::min(1.0f, ss_tot / 10000.0f);
    return r_squared;
}

void RobustNoiseFilter::computeLBP(const cv::Mat& image, cv::Mat& lbp_image) {
    lbp_image = cv::Mat::zeros(image.size(), CV_8U);

    for (int y = 1; y < image.rows - 1; ++y) {
        for (int x = 1; x < image.cols - 1; ++x) {
            uchar center = image.at<uchar>(y, x);
            uchar lbp_value = 0;

            // 8 neighbors
            std::vector<uchar> neighbors = {
                image.at<uchar>(y - 1, x - 1), image.at<uchar>(y - 1, x),
                image.at<uchar>(y - 1, x + 1), image.at<uchar>(y, x + 1),
                image.at<uchar>(y + 1, x + 1), image.at<uchar>(y + 1, x),
                image.at<uchar>(y + 1, x - 1), image.at<uchar>(y, x - 1)};

            for (int i = 0; i < 8; ++i) {
                if (neighbors[i] >= center) {
                    lbp_value |= (1 << i);
                }
            }

            lbp_image.at<uchar>(y, x) = lbp_value;
        }
    }
}

cv::Mat RobustNoiseFilter::computeHOG(const cv::Mat& image, int cell_size) {
    cv::Mat hog_response = cv::Mat::zeros(image.size(), CV_32F);

    cv::Mat gradx, grady;
    cv::Sobel(image, gradx, CV_32F, 1, 0, 3);
    cv::Sobel(image, grady, CV_32F, 0, 1, 3);

    cv::Mat magnitude;
    cv::magnitude(gradx, grady, magnitude);

    return magnitude;
}

std::vector<cv::Mat> RobustNoiseFilter::applyGaborFilters(const cv::Mat& image) {
    std::vector<cv::Mat> responses;

    // Create Gabor filters tuned for different orientations
    for (int orientation = 0; orientation < 4; ++orientation) {
        float theta = M_PI * orientation / 4;

        // Gabor parameters
        float lambda = 10.0f;
        float gamma = 0.5f;
        float sigma = 3.0f;

        int kernel_size = 21;
        cv::Mat kernel = cv::Mat::zeros(kernel_size, kernel_size, CV_32F);

        float x_theta, y_theta;
        for (int y = -kernel_size / 2; y <= kernel_size / 2; ++y) {
            for (int x = -kernel_size / 2; x <= kernel_size / 2; ++x) {
                x_theta = x * std::cos(theta) + y * std::sin(theta);
                y_theta = -x * std::sin(theta) + y * std::cos(theta);

                float gabor = std::exp(-(x_theta * x_theta + gamma * gamma * y_theta * y_theta) /
                                       (2 * sigma * sigma)) *
                              std::cos(2 * M_PI * x_theta / lambda);

                kernel.at<float>(y + kernel_size / 2, x + kernel_size / 2) = gabor;
            }
        }

        // Apply filter
        cv::Mat response;
        cv::filter2D(image, response, CV_32F, kernel);
        responses.push_back(response);
    }

    return responses;
}

RobustNoiseFilter::ImageStats RobustNoiseFilter::computeImageStatistics(const cv::Mat& image) {
    ImageStats stats = {};

    cv::Scalar mean_val = cv::mean(image);
    stats.mean = static_cast<float>(mean_val[0]);

    cv::Scalar std_val;
    cv::Mat diff;
    cv::absdiff(image, cv::Scalar(stats.mean), diff);
    cv::Scalar var_val = cv::mean(diff.mul(diff));
    stats.std = std::sqrt(static_cast<float>(var_val[0]));

    // Entropy
    cv::Mat hist;
    int histSize = 256;
    float range[] = {0, 256};
    const float* histRange = range;

    cv::calcHist(&image, 1, 0, cv::Mat(), hist, 1, &histSize, &histRange);
    hist = hist / image.total();

    stats.entropy = 0.0f;
    for (int i = 0; i < histSize; ++i) {
        float p = hist.at<float>(i, 0);
        if (p > 0) {
            stats.entropy -= p * std::log(p);
        }
    }

    return stats;
}

void RobustNoiseFilter::setFilteringLevel(int level) {
    switch (level) {
        case 0:  // Aggressive (high precision)
            current_confidence_threshold_ = confidence_threshold_aggressive_;
            break;
        case 1:  // Balanced
            current_confidence_threshold_ = confidence_threshold_balanced_;
            break;
        case 2:  // Lenient (high recall)
            current_confidence_threshold_ = confidence_threshold_lenient_;
            break;
        default:
            current_confidence_threshold_ = confidence_threshold_balanced_;
    }
}

} // namespace bullet_detection
