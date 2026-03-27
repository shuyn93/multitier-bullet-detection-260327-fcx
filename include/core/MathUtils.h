#pragma once

#include <cmath>
#include <algorithm>
#include <vector>

namespace bullet_detection {
namespace math_utils {

// ===== Mathematical Constants =====
constexpr double PI = 3.14159265358979323846;
constexpr float PI_F = 3.14159265358979f;
constexpr float EPSILON = 1e-6f;

// ===== Geometric Utilities =====

// Compute circularity metric: 4*pi*area / perimeter^2
// Range: [0, 1], where 1 = perfect circle
inline float computeCircularity(double area, double perimeter) {
    if (perimeter < EPSILON) return 0.0f;
    return static_cast<float>((4.0 * PI * area) / (perimeter * perimeter));
}

// Compute solidity: contour_area / convex_hull_area
// Range: [0, 1], where 1 = convex shape
inline float computeSolidity(double contour_area, double hull_area) {
    if (hull_area < EPSILON) return 0.0f;
    return static_cast<float>(contour_area / hull_area);
}

// Aspect ratio: min(width, height) / max(width, height)
// Range: [0, 1], where 1 = square
inline float computeAspectRatio(int width, int height) {
    if (width == 0 || height == 0) return 0.0f;
    float ratio = static_cast<float>(std::min(width, height)) / 
                  static_cast<float>(std::max(width, height));
    return std::max(0.0f, std::min(1.0f, ratio));
}

// ===== Distance Metrics =====

// 2D Euclidean distance
inline float distance2D(float x1, float y1, float x2, float y2) {
    float dx = x1 - x2;
    float dy = y1 - y2;
    return std::sqrt(dx * dx + dy * dy);
}

// 3D Euclidean distance
inline double distance3D(double x1, double y1, double z1,
                        double x2, double y2, double z2) {
    double dx = x1 - x2;
    double dy = y1 - y2;
    double dz = z1 - z2;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

// Squared distance (faster when absolute distance not needed)
inline float distance2DSquared(float x1, float y1, float x2, float y2) {
    float dx = x1 - x2;
    float dy = y1 - y2;
    return dx * dx + dy * dy;
}

inline double distance3DSquared(double x1, double y1, double z1,
                                double x2, double y2, double z2) {
    double dx = x1 - x2;
    double dy = y1 - y2;
    double dz = z1 - z2;
    return dx * dx + dy * dy + dz * dz;
}

// ===== Numerical Safety =====

// Safe division with epsilon checking
inline float safeDivide(float numerator, float denominator, float fallback = 0.0f) {
    if (std::abs(denominator) < EPSILON) {
        return fallback;
    }
    return numerator / denominator;
}

inline double safeDivide(double numerator, double denominator, double fallback = 0.0) {
    if (std::abs(denominator) < 1e-9) {
        return fallback;
    }
    return numerator / denominator;
}

// Safe normalize: prevent division by zero
inline float safeNormalize(float value, float max_value = 1.0f) {
    if (max_value < EPSILON) return 0.0f;
    return std::max(0.0f, std::min(max_value, value)) / max_value;
}

// Clamp value to range [min, max]
template<typename T>
inline T clamp(T value, T min_val, T max_val) {
    return std::max(min_val, std::min(max_val, value));
}

// ===== Linear Algebra Helpers =====

// Vector dot product (any dimension)
template<typename T>
inline T dotProduct(const std::vector<T>& a, const std::vector<T>& b) {
    T result = 0;
    size_t len = std::min(a.size(), b.size());
    for (size_t i = 0; i < len; ++i) {
        result += a[i] * b[i];
    }
    return result;
}

// Vector magnitude/norm
template<typename T>
inline T magnitude(const std::vector<T>& vec) {
    T sum_sq = 0;
    for (const auto& v : vec) {
        sum_sq += v * v;
    }
    return std::sqrt(sum_sq);
}

// Normalize vector to unit length
template<typename T>
inline std::vector<T> normalize(const std::vector<T>& vec) {
    T mag = magnitude(vec);
    if (mag < EPSILON) return vec;
    
    std::vector<T> normalized(vec.size());
    for (size_t i = 0; i < vec.size(); ++i) {
        normalized[i] = vec[i] / mag;
    }
    return normalized;
}

} // namespace math_utils
} // namespace bullet_detection
