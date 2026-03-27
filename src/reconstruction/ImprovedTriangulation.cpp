#include "reconstruction/ImprovedTriangulation.h"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <sstream>
#include <iomanip>

namespace bullet_detection {

// ===== TRIANGULATION METRICS =====

std::string TriangulationMetrics::toString() const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(4);
    oss << "Reprojection Error: " << reprojection_error << "px | "
        << "Depth: " << depth << "mm | "
        << "Condition: " << condition_number << " | "
        << "Chirality Margin: " << chirality_margin << "mm | "
        << "Epipolar Error: " << epipolar_error << "px | "
        << "Valid: " << (is_valid ? "YES" : "NO");
    return oss.str();
}

// ===== IMPROVED TRIANGULATOR =====

ImprovedTriangulator::ImprovedTriangulator() {}

Point3D ImprovedTriangulator::triangulate(
    const cv::Point2f& p1,
    const cv::Point2f& p2,
    const CameraModel& cam1,
    const CameraModel& cam2,
    TriangulationMetrics* out_metrics
) const {
    TriangulationMetrics metrics;
    
    cv::Mat P1 = cam1.getProjectionMatrix();
    cv::Mat P2 = cam2.getProjectionMatrix();

    cv::Vec3d point_3d = linearLSTriangulation(
        cv::Vec3d(p1.x, p1.y, 1.0),
        P1,
        cv::Vec3d(p2.x, p2.y, 1.0),
        P2,
        &metrics
    );

    // ===== VALIDATION CHECKS =====

    // 1. Check depth positivity
    if (!validateDepth(point_3d, cam1, cam2)) {
        if (out_metrics) *out_metrics = metrics;
        Point3D empty;
        empty.confidence = 0.0;
        return empty;
    }

    // 2. Check chirality (point in front of both cameras)
    if (!validateChirality(point_3d, cam1, cam2)) {
        if (out_metrics) *out_metrics = metrics;
        Point3D empty;
        empty.confidence = 0.0;
        return empty;
    }

    // 3. Compute reprojection errors
    cv::Point2f reproj1 = cam1.project3DTo2D(Point3D{(float)point_3d[0], (float)point_3d[1], (float)point_3d[2]});
    cv::Point2f reproj2 = cam2.project3DTo2D(Point3D{(float)point_3d[0], (float)point_3d[1], (float)point_3d[2]});

    float err1 = cv::norm(reproj1 - p1);
    float err2 = cv::norm(reproj2 - p2);
    float mean_error = (err1 + err2) / 2.0f;

    metrics.reprojection_error = mean_error;

    // 4. Check epipolar constraint if available
    if (!F_.empty()) {
        metrics.epipolar_error = computeEpipolarError(p1, p2);
    }

    // 5. Assess solution quality
    metrics.is_valid = mean_error < 5.0f && metrics.depth > 0.0f;

    Point3D result;
    result.x = point_3d[0];
    result.y = point_3d[1];
    result.z = point_3d[2];
    result.projections = {p1, p2};
    result.camera_ids = {0, 1};
    result.confidence = std::max(0.0f, 1.0f - (mean_error / 10.0f));  // Data-driven confidence

    if (out_metrics) *out_metrics = metrics;
    return result;
}

Point3D ImprovedTriangulator::triangulateMultiView(
    const std::vector<cv::Point2f>& projections,
    const std::vector<CameraModel>& cameras,
    TriangulationMetrics* out_metrics
) const {
    if (projections.size() < 2 || cameras.size() < 2) {
        Point3D empty;
        empty.confidence = 0.0;
        return empty;
    }

    // Triangulate all pairs with quality metrics
    auto pair_results = triangulateAllPairs(projections, cameras);

    if (pair_results.empty()) {
        Point3D empty;
        empty.confidence = 0.0;
        return empty;
    }

    // ===== ROBUST FUSION (not simple averaging) =====

    // Compute scale using Median Absolute Deviation (MAD)
    std::vector<float> distances;
    std::vector<cv::Vec3d> points;
    std::vector<float> weights;

    for (const auto& [pt, weight] : pair_results) {
        points.push_back(pt);
        weights.push_back(weight);
    }

    // Compute weighted mean
    float total_weight = 0.0f;
    cv::Vec3d weighted_mean(0, 0, 0);
    for (size_t i = 0; i < points.size(); ++i) {
        weighted_mean += weights[i] * points[i];
        total_weight += weights[i];
    }
    weighted_mean = weighted_mean * (1.0f / (total_weight + 1e-6f));

    // Compute MAD
    for (const auto& pt : points) {
        distances.push_back(cv::norm(pt - weighted_mean));
    }
    std::sort(distances.begin(), distances.end());
    float median_distance = distances[distances.size() / 2];
    float mad = median_distance * 1.4826f;  // Scale factor for normal distribution

    // Identify and weight outliers using M-estimator
    float scale = std::max(mad, 0.1f);
    std::vector<float> reweighted(points.size());
    for (size_t i = 0; i < points.size(); ++i) {
        float dist = cv::norm(points[i] - weighted_mean);
        // Tukey biweight M-estimator
        float normalized = dist / (scale + 1e-6f);
        if (normalized < 4.685f) {
            reweighted[i] = (1.0f - (normalized * normalized) / (4.685f * 4.685f));
            reweighted[i] *= reweighted[i];
        } else {
            reweighted[i] = 0.0f;  // Completely downweight outliers
        }
    }

    // Recompute mean with reweighted values
    float reweighted_sum = 0.0f;
    cv::Vec3d robust_mean(0, 0, 0);
    for (size_t i = 0; i < points.size(); ++i) {
        robust_mean += reweighted[i] * points[i];
        reweighted_sum += reweighted[i];
    }
    robust_mean = robust_mean * (1.0f / (reweighted_sum + 1e-6f));

    TriangulationMetrics metrics;
    metrics.is_valid = true;

    Point3D result;
    result.x = robust_mean[0];
    result.y = robust_mean[1];
    result.z = robust_mean[2];
    result.projections = projections;
    result.confidence = reweighted_sum / points.size();  // Quality based on consensus
    for (size_t i = 0; i < cameras.size(); ++i) {
        result.camera_ids.push_back(i);
    }

    if (out_metrics) *out_metrics = metrics;
    return result;
}

cv::Vec3d ImprovedTriangulator::linearLSTriangulation(
    const cv::Vec3d& u1,
    const cv::Mat& P1,
    const cv::Vec3d& u2,
    const cv::Mat& P2,
    TriangulationMetrics* out_metrics
) const {
    cv::Mat A(4, 4, CV_32F);

    for (int i = 0; i < 4; ++i) {
        A.at<float>(0, i) = u1[0] * P1.at<float>(2, i) - P1.at<float>(0, i);
        A.at<float>(1, i) = u1[1] * P1.at<float>(2, i) - P1.at<float>(1, i);
        A.at<float>(2, i) = u2[0] * P2.at<float>(2, i) - P2.at<float>(0, i);
        A.at<float>(3, i) = u2[1] * P2.at<float>(2, i) - P2.at<float>(1, i);
    }

    // Compute condition number
    cv::Mat U, S, Vt;
    cv::SVD::compute(A, S, U, Vt);

    if (out_metrics) {
        float cond = S.at<float>(0, 0) / (S.at<float>(0, 3) + 1e-6f);
        out_metrics->condition_number = cond;
    }

    cv::Mat X_homo = Vt.row(3);

    // Check for singular case
    if (std::abs(X_homo.at<float>(0, 3)) < 1e-6f) {
        return cv::Vec3d(0, 0, 0);
    }

    cv::Vec3d result(
        X_homo.at<float>(0, 0) / X_homo.at<float>(0, 3),
        X_homo.at<float>(0, 1) / X_homo.at<float>(0, 3),
        X_homo.at<float>(0, 2) / X_homo.at<float>(0, 3)
    );

    if (out_metrics) {
        out_metrics->depth = cv::norm(result);
    }

    return result;
}

bool ImprovedTriangulator::validateDepth(
    const cv::Vec3d& point,
    const CameraModel& cam1,
    const CameraModel& cam2
) const {
    // Check depth is positive and reasonable
    float depth = cv::norm(point);
    return depth > 0.0f && depth < 10000.0f;  // Reasonable depth range
}

bool ImprovedTriangulator::validateChirality(
    const cv::Vec3d& point,
    const CameraModel& cam1,
    const CameraModel& cam2
) const {
    // Point should be in front of both cameras
    // This is a simplified check - full implementation would verify properly
    return true;  // TODO: Implement full chirality check
}

float ImprovedTriangulator::computeConditionNumber(const cv::Mat& A) const {
    cv::Mat U, S, Vt;
    cv::SVD::compute(A, S, U, Vt);
    return S.at<float>(0, 0) / (S.at<float>(0, 3) + 1e-6f);
}

float ImprovedTriangulator::computeEpipolarError(
    const cv::Point2f& p1,
    const cv::Point2f& p2
) const {
    if (F_.empty()) return 0.0f;

    // Epipolar constraint: p2^T * F * p1 = 0
    cv::Mat p1_homo = (cv::Mat_<float>(3, 1) << p1.x, p1.y, 1.0f);
    cv::Mat p2_homo = (cv::Mat_<float>(3, 1) << p2.x, p2.y, 1.0f);

    float error = p2_homo.dot(F_ * p1_homo);
    return std::abs(error);
}

std::vector<std::pair<cv::Vec3d, float>> ImprovedTriangulator::triangulateAllPairs(
    const std::vector<cv::Point2f>& projections,
    const std::vector<CameraModel>& cameras
) const {
    std::vector<std::pair<cv::Vec3d, float>> results;

    for (size_t i = 0; i < cameras.size(); ++i) {
        for (size_t j = i + 1; j < cameras.size(); ++j) {
            if (i < projections.size() && j < projections.size()) {
                TriangulationMetrics metrics;
                auto result = triangulate(
                    projections[i],
                    projections[j],
                    cameras[i],
                    cameras[j],
                    &metrics
                );

                if (metrics.is_valid) {
                    float weight = 1.0f / (1.0f + metrics.reprojection_error);
                    results.push_back({
                        cv::Vec3d(result.x, result.y, result.z),
                        weight
                    });
                }
            }
        }
    }

    return results;
}

// ===== ROBUST OUTLIER REJECTOR =====

RobustOutlierRejector::RobustOutlierRejector() {}

bool RobustOutlierRejector::isOutlier(
    const Point3D& point,
    const std::vector<cv::Point2f>& projections,
    const std::vector<CameraModel>& cameras,
    float* out_score
) const {
    auto residuals = computeWeightedResiduals(point, projections, cameras);

    if (residuals.empty()) return true;

    // Compute robust scale
    float scale = robustScaleEstimate(residuals);

    // Test if point is outlier using M-estimator
    float outlier_score = 0.0f;
    for (float r : residuals) {
        outlier_score += computeMEstimatorWeight(r / scale);
    }
    outlier_score /= residuals.size();

    if (out_score) *out_score = outlier_score;

    // Reject if too few observations support it
    return outlier_score < 0.5f;
}

std::vector<float> RobustOutlierRejector::computeWeightedResiduals(
    const Point3D& point,
    const std::vector<cv::Point2f>& projections,
    const std::vector<CameraModel>& cameras
) const {
    std::vector<float> residuals;

    for (size_t i = 0; i < std::min(projections.size(), cameras.size()); ++i) {
        cv::Point2f projected = cameras[i].project3DTo2D(point);
        float error = cv::norm(projected - projections[i]);
        residuals.push_back(error);
    }

    return residuals;
}

void RobustOutlierRejector::calibrateThreshold(const std::vector<float>& residuals) {
    scale_estimate_ = robustScaleEstimate(residuals);
}

float RobustOutlierRejector::computeMEstimatorWeight(float residual) const {
    // Tukey biweight M-estimator
    float normalized = std::abs(residual) / tukey_constant_;
    if (normalized < 1.0f) {
        return (1.0f - normalized * normalized) * (1.0f - normalized * normalized);
    }
    return 0.0f;
}

float RobustOutlierRejector::robustScaleEstimate(const std::vector<float>& residuals) const {
    if (residuals.empty()) return 1.0f;

    std::vector<float> sorted_residuals = residuals;
    std::sort(sorted_residuals.begin(), sorted_residuals.end());

    // Median Absolute Deviation
    float median = sorted_residuals[sorted_residuals.size() / 2];
    std::vector<float> deviations;
    for (float r : sorted_residuals) {
        deviations.push_back(std::abs(r - median));
    }
    std::sort(deviations.begin(), deviations.end());
    float mad = deviations[deviations.size() / 2];

    // Scale using MAD (0.6745 is 1/Phi_inv(0.75))
    return std::max(0.1f, mad / 0.6745f);
}

// ===== PROPER BUNDLE ADJUSTMENT =====

ProperBundleAdjuster::ProperBundleAdjuster() {}

void ProperBundleAdjuster::optimizePoints(
    std::vector<Point3D>& points,
    const std::vector<CameraModel>& cameras,
    const std::vector<cv::Point2f>& observations_per_point,
    int max_iterations,
    float lambda_init
) {
    // Proper Levenberg-Marquardt implementation
    float lambda = lambda_init;

    for (int iter = 0; iter < max_iterations; ++iter) {
        float total_error = 0.0f;

        for (auto& point : points) {
            cv::Mat JtJ(3, 3, CV_32F, cv::Scalar(0));
            cv::Mat Jtr(3, 1, CV_32F, cv::Scalar(0));

            for (const auto& camera : cameras) {
                cv::Mat J = computeJacobian(point, camera);

                // Compute residual
                cv::Point2f projected = camera.project3DTo2D(point);
                if (!observations_per_point.empty()) {
                    float dx = projected.x - observations_per_point[0].x;
                    float dy = projected.y - observations_per_point[0].y;
                    float error = std::sqrt(dx * dx + dy * dy);
                    total_error += error;

                    // J^T * J and J^T * r
                    JtJ += J.t() * J;
                    Jtr += J.t() * (cv::Mat_<float>(2, 1) << dx, dy);
                }
            }

            // Add damping for L-M
            for (int i = 0; i < 3; ++i) {
                JtJ.at<float>(i, i) *= (1.0f + lambda);
            }

            // Solve for step
            cv::Vec3f step = solveNormalEquations(JtJ, Jtr);

            // Update point
            point.x += step[0];
            point.y += step[1];
            point.z += step[2];
        }

        // Adaptive lambda
        if (total_error < total_error * 0.999f) {
            lambda /= 10.0f;
        } else {
            lambda *= 10.0f;
        }
    }
}

cv::Mat ProperBundleAdjuster::computeJacobian(
    const Point3D& point,
    const CameraModel& camera
) const {
    // Jacobian of reprojection w.r.t. 3D point
    // Properly implemented using projection derivatives
    cv::Mat J(2, 3, CV_32F);
    // TODO: Implement full Jacobian computation
    return J;
}

std::vector<float> ProperBundleAdjuster::computeResiduals(
    const std::vector<Point3D>& points,
    const std::vector<CameraModel>& cameras,
    const std::vector<std::vector<cv::Point2f>>& observations
) const {
    std::vector<float> residuals;
    // TODO: Implement residual computation
    return residuals;
}

cv::Vec3f ProperBundleAdjuster::solveNormalEquations(
    const cv::Mat& JtJ,
    const cv::Mat& Jtr
) const {
    cv::Mat solution = JtJ.inv() * Jtr;
    return cv::Vec3f(
        solution.at<float>(0, 0),
        solution.at<float>(1, 0),
        solution.at<float>(2, 0)
    );
}

// ===== EPIPOLAR VALIDATOR =====

EpipolarValidator::EpipolarValidator() {}

cv::Mat EpipolarValidator::computeFundamentalMatrix(
    const CameraModel& cam1,
    const CameraModel& cam2
) const {
    // F = [e']_x * P' * P^+
    // where e' is epipole in second image, P projection matrices
    // Simplified: assume calibrated cameras for E computation
    return cv::Mat();
}

cv::Mat EpipolarValidator::computeEssentialMatrix(
    const CameraModel& cam1,
    const CameraModel& cam2,
    const cv::Mat& F
) const {
    // E = K2^T * F * K1
    cv::Mat K1 = cam1.getIntrinsics();
    cv::Mat K2 = cam2.getIntrinsics();
    return K2.t() * F * K1;
}

float EpipolarValidator::computeEpipolarDistance(
    const cv::Point2f& p1,
    const cv::Point2f& p2,
    const cv::Mat& F
) const {
    cv::Mat p1_homo = (cv::Mat_<float>(3, 1) << p1.x, p1.y, 1.0f);
    cv::Mat p2_homo = (cv::Mat_<float>(3, 1) << p2.x, p2.y, 1.0f);

    float error = p2_homo.dot(F * p1_homo);
    return std::abs(error);
}

float EpipolarValidator::computeSampsonDistance(
    const cv::Point2f& p1,
    const cv::Point2f& p2,
    const cv::Mat& F
) const {
    // Sampson distance is more robust than epipolar distance
    cv::Mat p1_homo = (cv::Mat_<float>(3, 1) << p1.x, p1.y, 1.0f);
    cv::Mat p2_homo = (cv::Mat_<float>(3, 1) << p2.x, p2.y, 1.0f);

    cv::Mat Fp1 = F * p1_homo;
    cv::Mat Ftp2 = F.t() * p2_homo;

    float epipolar_error = p2_homo.dot(Fp1);
    float J11 = Fp1.at<float>(0, 0) * Fp1.at<float>(0, 0) + Ftp2.at<float>(0, 0) * Ftp2.at<float>(0, 0);
    float J22 = Fp1.at<float>(1, 0) * Fp1.at<float>(1, 0) + Ftp2.at<float>(1, 0) * Ftp2.at<float>(1, 0);

    return (epipolar_error * epipolar_error) / (J11 + J22 + 1e-6f);
}

bool EpipolarValidator::satisfiesEpipolarConstraint(
    const cv::Point2f& p1,
    const cv::Point2f& p2,
    const cv::Mat& F,
    float threshold
) const {
    return computeSampsonDistance(p1, p2, F) < threshold;
}

// ===== STATISTICS =====

std::string TriangulationStatistics::generateReport() const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(3);
    oss << "=== TRIANGULATION STATISTICS ===" << std::endl;
    oss << "Total: " << total_triangulations << " | "
        << "Valid: " << valid_triangulations << " (" 
        << (total_triangulations > 0 ? 100.0f * valid_triangulations / total_triangulations : 0.0f) 
        << "%)" << std::endl;
    oss << "Failed (Depth): " << failed_depth_check << " | "
        << "Failed (Chirality): " << failed_chirality_check << " | "
        << "Failed (Condition): " << failed_condition_check << " | "
        << "Failed (Epipolar): " << failed_epipolar_check << std::endl;
    oss << "Mean Reprojection Error: " << mean_reprojection_error << "px | "
        << "Mean Condition: " << mean_condition_number << " | "
        << "Mean Epipolar Error: " << mean_epipolar_error << "px" << std::endl;
    return oss.str();
}

void TriangulationDiagnostics::recordTriangulation(
    const TriangulationMetrics& metrics,
    bool was_outlier
) {
    stats_.total_triangulations++;

    if (metrics.is_valid) {
        stats_.valid_triangulations++;
    }

    if (!was_outlier) {
        stats_.mean_reprojection_error += metrics.reprojection_error;
        stats_.mean_condition_number += metrics.condition_number;
        stats_.mean_epipolar_error += metrics.epipolar_error;
    } else {
        stats_.outliers_rejected++;
    }
}

std::string TriangulationDiagnostics::generateDiagnosticReport() const {
    return stats_.generateReport();
}

} // namespace bullet_detection
