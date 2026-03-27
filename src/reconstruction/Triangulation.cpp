#include "reconstruction/Triangulation.h"
#include <cmath>
#include <algorithm>
#include <numeric>

namespace bullet_detection {

// ===== TRIANGULATOR =====

Triangulator::Triangulator() {}

Point3D Triangulator::triangulate(
    const cv::Point2f& p1,
    const cv::Point2f& p2,
    const CameraModel& cam1,
    const CameraModel& cam2
) const {
    cv::Mat P1 = cam1.getProjectionMatrix();
    cv::Mat P2 = cam2.getProjectionMatrix();

    cv::Vec3d point_3d = linearLSTriangulation(
        cv::Vec3d(p1.x, p1.y, 1.0),
        P1,
        cv::Vec3d(p2.x, p2.y, 1.0),
        P2
    );

    Point3D result;
    result.x = point_3d[0];
    result.y = point_3d[1];
    result.z = point_3d[2];
    result.projections = {p1, p2};
    result.camera_ids = {0, 1};
    result.confidence = 0.8;

    return result;
}

Point3D Triangulator::triangulateMultiView(
    const std::vector<cv::Point2f>& projections,
    const std::vector<CameraModel>& cameras
) const {
    if (projections.size() < 2 || cameras.size() < 2) {
        Point3D empty;
        empty.confidence = 0.0;
        return empty;
    }

    if (projections.size() == 2) {
        return triangulate(projections[0], projections[1], cameras[0], cameras[1]);
    }

    std::vector<cv::Vec3d> point_estimates;

    for (size_t i = 0; i < cameras.size(); ++i) {
        for (size_t j = i + 1; j < cameras.size(); ++j) {
            if (i < projections.size() && j < projections.size()) {
                auto tri_result = triangulate(projections[i], projections[j], cameras[i], cameras[j]);
                point_estimates.push_back(cv::Vec3d(tri_result.x, tri_result.y, tri_result.z));
            }
        }
    }

    if (point_estimates.empty()) {
        Point3D empty;
        empty.confidence = 0.0;
        return empty;
    }

    cv::Vec3d mean_point(0, 0, 0);
    for (const auto& pt : point_estimates) {
        mean_point += pt;
    }
    mean_point = mean_point * (1.0 / point_estimates.size());

    Point3D result;
    result.x = mean_point[0];
    result.y = mean_point[1];
    result.z = mean_point[2];
    result.projections = projections;
    result.confidence = 0.9;
    for (size_t i = 0; i < cameras.size(); ++i) {
        result.camera_ids.push_back(i);
    }

    return result;
}

cv::Vec3d Triangulator::dltTriangulate(
    const cv::Vec2d& p1,
    const cv::Mat& P1,
    const cv::Vec2d& p2,
    const cv::Mat& P2
) const {
    cv::Mat A(4, 4, CV_32F);

    for (int i = 0; i < 4; ++i) {
        A.at<float>(0, i) = p1[0] * P1.at<float>(2, i) - P1.at<float>(0, i);
        A.at<float>(1, i) = p1[1] * P1.at<float>(2, i) - P1.at<float>(1, i);
        A.at<float>(2, i) = p2[0] * P2.at<float>(2, i) - P2.at<float>(0, i);
        A.at<float>(3, i) = p2[1] * P2.at<float>(2, i) - P2.at<float>(1, i);
    }

    cv::Mat U, S, Vt;
    cv::SVD::compute(A, S, U, Vt);

    cv::Mat X_homo = Vt.row(3);
    cv::Vec3d result(
        X_homo.at<float>(0, 0) / X_homo.at<float>(0, 3),
        X_homo.at<float>(0, 1) / X_homo.at<float>(0, 3),
        X_homo.at<float>(0, 2) / X_homo.at<float>(0, 3)
    );

    return result;
}

cv::Vec3d Triangulator::linearLSTriangulation(
    const cv::Vec3d& u1,
    const cv::Mat& P1,
    const cv::Vec3d& u2,
    const cv::Mat& P2
) const {
    cv::Mat A(4, 4, CV_32F);

    for (int i = 0; i < 4; ++i) {
        A.at<float>(0, i) = u1[0] * P1.at<float>(2, i) - P1.at<float>(0, i);
        A.at<float>(1, i) = u1[1] * P1.at<float>(2, i) - P1.at<float>(1, i);
        A.at<float>(2, i) = u2[0] * P2.at<float>(2, i) - P2.at<float>(0, i);
        A.at<float>(3, i) = u2[1] * P2.at<float>(2, i) - P2.at<float>(1, i);
    }

    cv::Mat U, S, Vt;
    cv::SVD::compute(A, S, U, Vt);

    cv::Mat X_homo = Vt.row(3);

    if (std::abs(X_homo.at<float>(0, 3)) < 1e-6f) {
        return cv::Vec3d(0, 0, 0);
    }

    return cv::Vec3d(
        X_homo.at<float>(0, 0) / X_homo.at<float>(0, 3),
        X_homo.at<float>(0, 1) / X_homo.at<float>(0, 3),
        X_homo.at<float>(0, 2) / X_homo.at<float>(0, 3)
    );
}

// ===== OUTLIER REJECTOR =====

OutlierRejector::OutlierRejector() {}

bool OutlierRejector::isOutlier(
    const Point3D& point,
    const std::vector<DetectionResult>& detections_per_camera,
    const std::vector<CameraModel>& cameras,
    float reprojection_threshold
) const {
    if (detections_per_camera.empty() || cameras.empty()) return true;

    float total_error = 0.0f;
    int count = 0;

    for (size_t i = 0; i < std::min(detections_per_camera.size(), cameras.size()); ++i) {
        float error = computeReprojectionError(point, detections_per_camera[i].center, cameras[i]);
        if (error > reprojection_threshold) {
            return true;
        }
        total_error += error;
        count++;
    }

    return count > 0 && (total_error / count) > (reprojection_threshold * 0.5f);
}

float OutlierRejector::computeReprojectionError(
    const Point3D& point,
    const cv::Point2f& projection,
    const CameraModel& camera
) const {
    cv::Point2f projected = camera.project3DTo2D(point);
    float dx = projected.x - projection.x;
    float dy = projected.y - projection.y;
    return std::sqrt(dx * dx + dy * dy);
}

// ===== BUNDLE ADJUSTER =====

BundleAdjuster::BundleAdjuster() {}

void BundleAdjuster::optimizePoints(
    std::vector<Point3D>& points,
    const std::vector<CameraModel>& cameras,
    const std::vector<std::vector<DetectionResult>>& detections_per_frame,
    int iterations
) {
    float learning_rate = 0.001f;

    for (int iter = 0; iter < iterations; ++iter) {
        for (auto& point : points) {
            std::vector<float> residuals;

            for (size_t cam_idx = 0; cam_idx < cameras.size(); ++cam_idx) {
                if (cam_idx < detections_per_frame.size()) {
                    for (const auto& detection : detections_per_frame[cam_idx]) {
                        computeResiduals(point, cameras, detections_per_frame, residuals);
                    }
                }
            }

            if (!residuals.empty()) {
                float mean_residual =
                    std::accumulate(residuals.begin(), residuals.end(), 0.0f) / residuals.size();

                point.x -= learning_rate * mean_residual * 0.1f;
                point.y -= learning_rate * mean_residual * 0.1f;
                point.z -= learning_rate * mean_residual * 0.05f;
            }
        }
    }
}

void BundleAdjuster::computeResiduals(
    const Point3D& point,
    const std::vector<CameraModel>& cameras,
    const std::vector<std::vector<DetectionResult>>& detections_per_frame,
    std::vector<float>& residuals
) const {
    for (size_t cam_idx = 0; cam_idx < cameras.size(); ++cam_idx) {
        cv::Point2f projected = cameras[cam_idx].project3DTo2D(point);

        if (cam_idx < detections_per_frame.size()) {
            for (const auto& detection : detections_per_frame[cam_idx]) {
                float dx = projected.x - detection.center.x;
                float dy = projected.y - detection.center.y;
                residuals.push_back(std::sqrt(dx * dx + dy * dy));
            }
        }
    }
}

} // namespace bullet_detection
