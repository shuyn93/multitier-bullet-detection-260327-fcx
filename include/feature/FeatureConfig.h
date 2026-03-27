#pragma once

#include <vector>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <opencv2/opencv.hpp>

namespace bullet_detection {

// ===== FEATURE STATISTICS =====
// Maintains training set statistics for standardization
class FeatureStatistics {
public:
    FeatureStatistics() = default;
    
    // Initialize with training data
    void computeStatistics(const std::vector<std::vector<float>>& training_features) {
        if (training_features.empty()) return;
        
        int n_features = training_features[0].size();
        means_.resize(n_features, 0.0f);
        stddevs_.resize(n_features, 0.0f);
        mins_.resize(n_features, std::numeric_limits<float>::max());
        maxs_.resize(n_features, std::numeric_limits<float>::lowest());
        
        // Compute mean
        for (const auto& sample : training_features) {
            for (int i = 0; i < n_features; ++i) {
                means_[i] += sample[i];
                mins_[i] = std::min(mins_[i], sample[i]);
                maxs_[i] = std::max(maxs_[i], sample[i]);
            }
        }
        
        for (int i = 0; i < n_features; ++i) {
            means_[i] /= training_features.size();
        }
        
        // Compute stddev
        for (const auto& sample : training_features) {
            for (int i = 0; i < n_features; ++i) {
                float diff = sample[i] - means_[i];
                stddevs_[i] += diff * diff;
            }
        }
        
        for (int i = 0; i < n_features; ++i) {
            stddevs_[i] = std::sqrt(stddevs_[i] / training_features.size());
            // Ensure non-zero stddev (add small epsilon)
            if (stddevs_[i] < 1e-6f) {
                stddevs_[i] = 1e-6f;
            }
        }
    }
    
    // Standardize a feature value
    float standardize(float value, int feature_idx) const {
        if (feature_idx < 0 || feature_idx >= static_cast<int>(means_.size())) {
            return value;
        }
        
        float standardized = (value - means_[feature_idx]) / stddevs_[feature_idx];
        // Clip extreme values
        return std::max(-3.0f, std::min(3.0f, standardized));
    }
    
    const std::vector<float>& getMeans() const { return means_; }
    const std::vector<float>& getStddevs() const { return stddevs_; }
    const std::vector<float>& getMins() const { return mins_; }
    const std::vector<float>& getMaxs() const { return maxs_; }
    
private:
    std::vector<float> means_;
    std::vector<float> stddevs_;
    std::vector<float> mins_;
    std::vector<float> maxs_;
};

// ===== ROBUST STATISTICS UTILITIES =====
class RobustStats {
public:
    // Compute median of vector
    static float median(std::vector<float> values) {
        if (values.empty()) return 0.0f;
        std::sort(values.begin(), values.end());
        
        if (values.size() % 2 == 0) {
            return (values[values.size() / 2 - 1] + values[values.size() / 2]) / 2.0f;
        } else {
            return values[values.size() / 2];
        }
    }
    
    // Compute interquartile range (Q3 - Q1)
    static float iqr(std::vector<float> values) {
        if (values.size() < 4) return 0.0f;
        std::sort(values.begin(), values.end());
        
        int n = values.size();
        float q1 = values[n / 4];
        float q3 = values[3 * n / 4];
        
        return q3 - q1;
    }
    
    // Compute percentile
    static float percentile(std::vector<float> values, float p) {
        if (values.empty()) return 0.0f;
        if (p < 0.0f || p > 100.0f) return 0.0f;
        
        std::sort(values.begin(), values.end());
        
        int idx = static_cast<int>((p / 100.0f) * (values.size() - 1));
        idx = std::max(0, std::min(idx, static_cast<int>(values.size() - 1)));
        
        return values[idx];
    }
    
    // Detect outliers using IQR method
    static std::vector<bool> detectOutliers(const std::vector<float>& values, float k = 1.5f) {
        std::vector<bool> is_outlier(values.size(), false);
        
        if (values.size() < 4) return is_outlier;
        
        std::vector<float> sorted_values = values;
        std::sort(sorted_values.begin(), sorted_values.end());
        
        int n = sorted_values.size();
        float q1 = sorted_values[n / 4];
        float q3 = sorted_values[3 * n / 4];
        float iqr_val = q3 - q1;
        
        float lower_bound = q1 - k * iqr_val;
        float upper_bound = q3 + k * iqr_val;
        
        for (size_t i = 0; i < values.size(); ++i) {
            if (values[i] < lower_bound || values[i] > upper_bound) {
                is_outlier[i] = true;
            }
        }
        
        return is_outlier;
    }
};

// ===== IMPROVED FEATURE EXTRACTION WITH CONFIGURATION =====
struct FeatureConfig {
    // Area normalization
    float area_divisor = 2000.0f;
    
    // Radial structure
    int radial_ray_count = 32;  // Increased from 16 for better sampling
    
    // Ring energy
    float ring_thickness_factor = 0.3f;  // Relative to mean radius
    
    // Edge detection
    float canny_threshold1 = 50.0f;
    float canny_threshold2 = 150.0f;
    
    // Normalization
    bool use_standardization = true;
    
    // Robust statistics
    bool use_robust_stats = true;
};

} // namespace bullet_detection
