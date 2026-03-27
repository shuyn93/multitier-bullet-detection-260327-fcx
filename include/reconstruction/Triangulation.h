#pragma once
#include "core/Types.h"
#include "calibration/CameraModel.h"
#include <vector>

namespace bullet_detection {

class Triangulator {
public:
    Triangulator();

    // Direct Linear Triangulation (DLT)
    Point3D triangulate(
        const cv::Point2f& p1,
        const cv::Point2f& p2,
        const CameraModel& cam1,
        const CameraModel& cam2
    ) const;

    // Multi-view triangulation (angular error minimization)
    Point3D triangulateMultiView(
        const std::vector<cv::Point2f>& projections,
        const std::vector<CameraModel>& cameras
    ) const;

private:
    cv::Vec3d dltTriangulate(
        const cv::Vec2d& p1,
        const cv::Mat& P1,
        const cv::Vec2d& p2,
        const cv::Mat& P2
    ) const;

    cv::Vec3d linearLSTriangulation(
        const cv::Vec3d& u1,
        const cv::Mat& P1,
        const cv::Vec3d& u2,
        const cv::Mat& P2
    ) const;
};

class OutlierRejector {
    // ELISAC or simplified RANSAC for 3D point verification
public:
    OutlierRejector();

    bool isOutlier(
        const Point3D& point,
        const std::vector<DetectionResult>& detections_per_camera,
        const std::vector<CameraModel>& cameras,
        float reprojection_threshold = 2.0f
    ) const;

    float computeReprojectionError(
        const Point3D& point,
        const cv::Point2f& projection,
        const CameraModel& camera
    ) const;
};

class BundleAdjuster {
public:
    BundleAdjuster();

    void optimizePoints(
        std::vector<Point3D>& points,
        const std::vector<CameraModel>& cameras,
        const std::vector<std::vector<DetectionResult>>& detections_per_frame,
        int iterations = 5
    );

private:
    void computeResiduals(
        const Point3D& point,
        const std::vector<CameraModel>& cameras,
        const std::vector<std::vector<DetectionResult>>& detections_per_frame,
        std::vector<float>& residuals
    ) const;
};

} // namespace bullet_detection
