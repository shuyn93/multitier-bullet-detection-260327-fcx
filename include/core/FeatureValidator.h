#pragma once

#include <cmath>
#include <algorithm>
#include <limits>
#include "core/Types.h"
#include "core/ErrorHandler.h"

namespace bullet_detection {

// ===== Feature Validation & Normalization =====
class FeatureValidator {
public:
    // Check if feature is valid (no NaN, Inf, or out of bounds)
    static bool isValid(const CandidateFeature& feat) {
        return isValidFloat(feat.area) &&
               isValidFloat(feat.circularity) &&
               isValidFloat(feat.solidity) &&
               isValidFloat(feat.aspect_ratio) &&
               isValidFloat(feat.radial_symmetry) &&
               isValidFloat(feat.radial_gradient) &&
               isValidFloat(feat.snr) &&
               isValidFloat(feat.entropy) &&
               isValidFloat(feat.ring_energy) &&
               isValidFloat(feat.sharpness) &&
               isValidFloat(feat.laplacian_density) &&
               isValidFloat(feat.phase_coherence) &&
               isValidFloat(feat.contrast) &&
               isValidFloat(feat.mean_intensity) &&
               isValidFloat(feat.std_intensity) &&
               isValidFloat(feat.edge_density) &&
               isValidFloat(feat.corner_count);
    }

    static bool isValid(const FeatureVector& fv) {
        for (int i = 0; i < FeatureVector::DIM; ++i) {
            if (!isValidFloat(fv.data[i])) {
                return false;
            }
        }
        return true;
    }

    // Safe normalization with fallback
    static CandidateFeature sanitize(const CandidateFeature& feat) {
        CandidateFeature sanitized = feat;

        // Clamp each feature to [0, 1] range
        sanitized.area = clampToValidRange(sanitized.area);
        sanitized.circularity = clampToValidRange(sanitized.circularity);
        sanitized.solidity = clampToValidRange(sanitized.solidity);
        sanitized.aspect_ratio = clampToValidRange(sanitized.aspect_ratio);
        sanitized.radial_symmetry = clampToValidRange(sanitized.radial_symmetry);
        sanitized.radial_gradient = clampToValidRange(sanitized.radial_gradient);
        sanitized.snr = clampToValidRange(sanitized.snr);
        sanitized.entropy = clampToValidRange(sanitized.entropy);
        sanitized.ring_energy = clampToValidRange(sanitized.ring_energy);
        sanitized.sharpness = clampToValidRange(sanitized.sharpness);
        sanitized.laplacian_density = clampToValidRange(sanitized.laplacian_density);
        sanitized.phase_coherence = clampToValidRange(sanitized.phase_coherence);
        sanitized.contrast = clampToValidRange(sanitized.contrast);
        sanitized.mean_intensity = clampToValidRange(sanitized.mean_intensity);
        sanitized.std_intensity = clampToValidRange(sanitized.std_intensity);
        sanitized.edge_density = clampToValidRange(sanitized.edge_density);
        sanitized.corner_count = clampToValidRange(sanitized.corner_count);

        return sanitized;
    }

    static FeatureVector sanitize(const FeatureVector& fv) {
        FeatureVector sanitized = fv;
        for (int i = 0; i < FeatureVector::DIM; ++i) {
            sanitized.data[i] = clampToValidRange(sanitized.data[i]);
        }
        return sanitized;
    }

    // Standardization (zero mean, unit variance) for improved classifier stability
    static FeatureVector standardize(
        const FeatureVector& fv,
        const cv::Mat& means,
        const cv::Mat& stddevs
    ) {
        FeatureVector standardized;

        for (int i = 0; i < FeatureVector::DIM; ++i) {
            float mean = means.at<float>(i, 0);
            float stddev = stddevs.at<float>(i, 0);

            if (stddev < 1e-6f) {
                standardized.data[i] = 0.0f;
            } else {
                standardized.data[i] = (fv.data[i] - mean) / stddev;
            }

            // Clip extreme values
            standardized.data[i] = std::max(-3.0f, std::min(3.0f, standardized.data[i]));
        }

        return standardized;
    }

private:
    static bool isValidFloat(float value) {
        return std::isfinite(value);
    }

    static float clampToValidRange(float value) {
        if (!std::isfinite(value)) {
            return 0.5f; // neutral fallback
        }
        return std::max(0.0f, std::min(1.0f, value));
    }
};

} // namespace bullet_detection
