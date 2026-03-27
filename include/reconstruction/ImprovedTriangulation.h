#pragma once

#include <vector>
#include <opencv2/opencv.hpp>
#include "core/Types.h"
#include "calibration/CameraModel.h"

namespace bullet_detection {

// ===== TRIANGULATION QUALITY METRICS =====

struct TriangulationMetrics {
    float reprojection_error = 0.0f;      // Per-camera reprojection error
    float depth = 0.0f;                   // Depth of point
    float condition_number = 0.0f;        // Matrix condition number
    float chirality_margin = 0.0f;        // How far in front of cameras
    float epipolar_error = 0.0f;          // Epipolar constraint error
    bool is_valid = false;                // Overall validity flag
    
    std::string toString() const;
};

// ===== IMPROVED TRIANGULATOR =====

class ImprovedTriangulator {
public:
    ImprovedTriangulator();

    // Enhanced triangulation with quality metrics
    Point3D triangulate(
        const cv::Point2f& p1,
        const cv::Point2f& p2,
        const CameraModel& cam1,
        const CameraModel& cam2,
        TriangulationMetrics* out_metrics = nullptr
    ) const;

    // Robust multi-view triangulation with outlier rejection
    Point3D triangulateMultiView(
        const std::vector<cv::Point2f>& projections,
        const std::vector<CameraModel>& cameras,
        TriangulationMetrics* out_metrics = nullptr
    ) const;

    // Set fundamental/essential matrices for epipolar validation
    void setFundamentalMatrix(const cv::Mat& F) { F_ = F; }
    void setEssentialMatrix(const cv::Mat& E) { E_ = E; }

private:
    cv::Vec3d linearLSTriangulation(
        const cv::Vec3d& u1,
        const cv::Mat& P1,
        const cv::Vec3d& u2,
        const cv::Mat& P2,
        TriangulationMetrics* out_metrics = nullptr
    ) const;

    // Validation checks
    bool validateDepth(const cv::Vec3d& point, const CameraModel& cam1, const CameraModel& cam2) const;
    bool validateChirality(const cv::Vec3d& point, const CameraModel& cam1, const CameraModel& cam2) const;
    float computeConditionNumber(const cv::Mat& A) const;
    float computeEpipolarError(const cv::Point2f& p1, const cv::Point2f& p2) const;

    // Robust multi-view
    std::vector<std::pair<cv::Vec3d, float>> triangulateAllPairs(
        const std::vector<cv::Point2f>& projections,
        const std::vector<CameraModel>& cameras
    ) const;

    cv::Mat F_, E_;  // Fundamental/Essential matrices
};

// ===== ROBUST OUTLIER REJECTION (ELISAC-inspired) =====

class RobustOutlierRejector {
public:
    RobustOutlierRejector();

    // ELISAC-inspired robust outlier rejection
    bool isOutlier(
        const Point3D& point,
        const std::vector<cv::Point2f>& projections,
        const std::vector<CameraModel>& cameras,
        float* out_score = nullptr
    ) const;

    // Compute residuals with robust weighting
    std::vector<float> computeWeightedResiduals(
        const Point3D& point,
        const std::vector<cv::Point2f>& projections,
        const std::vector<CameraModel>& cameras
    ) const;

    // Set threshold based on data
    void calibrateThreshold(const std::vector<float>& residuals);

private:
    // M-estimator for robust weighting
    float tukey_constant_ = 4.685f;  // Tukey biweight constant
    float scale_estimate_ = 1.0f;     // Robust scale (MAD)

    float computeMEstimatorWeight(float residual) const;
    float robustScaleEstimate(const std::vector<float>& residuals) const;
};

// ===== PROPER BUNDLE ADJUSTMENT (Levenberg-Marquardt) =====

class ProperBundleAdjuster {
public:
    ProperBundleAdjuster();

    // Levenberg-Marquardt optimization
    void optimizePoints(
        std::vector<Point3D>& points,
        const std::vector<CameraModel>& cameras,
        const std::vector<cv::Point2f>& observations_per_point,
        int max_iterations = 10,
        float lambda_init = 0.001f
    );

private:
    // Compute Jacobian of reprojection w.r.t. 3D point
    cv::Mat computeJacobian(
        const Point3D& point,
        const CameraModel& camera
    ) const;

    // Compute residuals
    std::vector<float> computeResiduals(
        const std::vector<Point3D>& points,
        const std::vector<CameraModel>& cameras,
        const std::vector<std::vector<cv::Point2f>>& observations
    ) const;

    // Solve linear system for step
    cv::Vec3f solveNormalEquations(
        const cv::Mat& JtJ,
        const cv::Mat& Jtr
    ) const;
};

// ===== EPIPOLAR CONSTRAINT VALIDATION =====

class EpipolarValidator {
public:
    EpipolarValidator();

    // Compute fundamental matrix from camera pair
    cv::Mat computeFundamentalMatrix(
        const CameraModel& cam1,
        const CameraModel& cam2
    ) const;

    // Compute essential matrix
    cv::Mat computeEssentialMatrix(
        const CameraModel& cam1,
        const CameraModel& cam2,
        const cv::Mat& F
    ) const;

    // Validate epipolar constraint
    float computeEpipolarDistance(
        const cv::Point2f& p1,
        const cv::Point2f& p2,
        const cv::Mat& F
    ) const;

    // Sampson distance (more robust)
    float computeSampsonDistance(
        const cv::Point2f& p1,
        const cv::Point2f& p2,
        const cv::Mat& F
    ) const;

    // Check if point satisfies epipolar constraint
    bool satisfiesEpipolarConstraint(
        const cv::Point2f& p1,
        const cv::Point2f& p2,
        const cv::Mat& F,
        float threshold = 2.0f
    ) const;
};

// ===== STATISTICS & DIAGNOSTICS =====

struct TriangulationStatistics {
    int total_triangulations = 0;
    int valid_triangulations = 0;
    int failed_depth_check = 0;
    int failed_chirality_check = 0;
    int failed_condition_check = 0;
    int failed_epipolar_check = 0;
    int outliers_rejected = 0;

    float mean_reprojection_error = 0.0f;
    float mean_condition_number = 0.0f;
    float mean_epipolar_error = 0.0f;

    std::string generateReport() const;
};

class TriangulationDiagnostics {
public:
    // Record triangulation result
    void recordTriangulation(
        const TriangulationMetrics& metrics,
        bool was_outlier
    );

    // Get statistics
    TriangulationStatistics getStatistics() const { return stats_; }

    // Generate diagnostic report
    std::string generateDiagnosticReport() const;

private:
    TriangulationStatistics stats_;
};

} // namespace bullet_detection
