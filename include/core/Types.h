#pragma once
#include <vector>
#include <opencv2/opencv.hpp>
#include <cstdint>

namespace bullet_detection {

constexpr int FEATURE_DIM = 17;
constexpr float FEATURE_NORM_SCALE = 1.0f;

// ===== Core Feature Vector =====
struct FeatureVector {
    static constexpr int DIM = FEATURE_DIM;
    float data[FEATURE_DIM];

    FeatureVector() {
        std::fill(data, data + FEATURE_DIM, 0.0f);
    }

    std::vector<float> toVector() const {
        return std::vector<float>(data, data + FEATURE_DIM);
    }

    cv::Mat toMat() const {
        cv::Mat m(FEATURE_DIM, 1, CV_32F);
        for (int i = 0; i < FEATURE_DIM; ++i) {
            m.at<float>(i, 0) = data[i];
        }
        return m;
    }

    float norm() const {
        float sum_sq = 0.0f;
        for (int i = 0; i < FEATURE_DIM; ++i) {
            sum_sq += data[i] * data[i];
        }
        return std::sqrt(sum_sq);
    }

    void normalize() {
        float n = norm();
        if (n > 1e-6f) {
            for (int i = 0; i < FEATURE_DIM; ++i) {
                data[i] /= n;
            }
        }
    }
};

// ===== 17 Feature Breakdown =====
struct CandidateFeature {
    // Geometric (0-3): area, circularity, solidity, aspect_ratio
    float area;
    float circularity;
    float solidity;
    float aspect_ratio;

    // Radial Structure (4-5): radial_symmetry, radial_gradient
    float radial_symmetry;
    float radial_gradient;

    // Energy & Texture (6-8): snr, entropy, ring_energy
    float snr;
    float entropy;
    float ring_energy;

    // Frequency & Phase (9-11): sharpness, laplacian_density, phase_coherence
    float sharpness;
    float laplacian_density;
    float phase_coherence;

    // Extended (12-16): contrast, mean_intensity, std_intensity, edge_density, corner_count
    float contrast;
    float mean_intensity;
    float std_intensity;
    float edge_density;
    float corner_count;

    FeatureVector toFeatureVector() const {
        FeatureVector fv;
        fv.data[0]  = area;
        fv.data[1]  = circularity;
        fv.data[2]  = solidity;
        fv.data[3]  = aspect_ratio;
        fv.data[4]  = radial_symmetry;
        fv.data[5]  = radial_gradient;
        fv.data[6]  = snr;
        fv.data[7]  = entropy;
        fv.data[8]  = ring_energy;
        fv.data[9]  = sharpness;
        fv.data[10] = laplacian_density;
        fv.data[11] = phase_coherence;
        fv.data[12] = contrast;
        fv.data[13] = mean_intensity;
        fv.data[14] = std_intensity;
        fv.data[15] = edge_density;
        fv.data[16] = corner_count;
        return fv;
    }

    std::vector<float> toVector() const {
        return {area, circularity, solidity, aspect_ratio,
                radial_symmetry, radial_gradient,
                snr, entropy, ring_energy,
                sharpness, laplacian_density, phase_coherence,
                contrast, mean_intensity, std_intensity, edge_density, corner_count};
    }
};

// ===== Candidate Region =====
struct CandidateRegion {
    cv::Rect bbox;
    std::vector<cv::Point> contour;
    cv::Mat roi_image;
    CandidateFeature features;
    uint64_t frame_id;
    float detection_score;  // From edge/threshold detection
};

// ===== Detection Result =====
enum class DecisionCode { ACCEPT, REJECT, LOW_CONFIDENCE };

struct ClassifierDecision {
    DecisionCode code;
    float confidence;
    float entropy;
    float margin;
    int tier;  // Which tier made the decision
};

struct DetectionResult {
    cv::Rect bbox;
    ClassifierDecision decision;
    FeatureVector features;
    uint64_t detection_id;
    uint64_t frame_id;
    cv::Point2f center;
};

// ===== Camera Frame =====
struct CameraFrame {
    cv::Mat image;
    uint64_t frame_id;
    double timestamp;
    int camera_id;
    std::vector<DetectionResult> detections;
};

// ===== 3D Reconstruction Types =====
struct Point3D {
    double x, y, z;
    double confidence;
    uint64_t point_id;
    std::vector<cv::Point2f> projections;  // One per camera
    std::vector<int> camera_ids;
};

struct CameraCalibration {
    int camera_id;
    cv::Mat intrinsics;      // 3x3
    cv::Mat distortion;      // 4-8 coefficients
    cv::Mat rvec, tvec;      // Rotation and translation from world
    cv::Mat P;               // 3x4 projection matrix
};

struct EpipolarGeometry {
    int cam1_id, cam2_id;
    cv::Mat F;               // Fundamental matrix
    cv::Mat E;               // Essential matrix
};

// ===== Tracking =====
struct Track {
    uint64_t track_id;
    std::vector<Point3D> history;
    double last_update_time;
    int missed_frames;
    bool active;
};

} // namespace bullet_detection
