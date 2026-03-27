#pragma once
#include <opencv2/opencv.hpp>
#include "core/Types.h"

namespace bullet_detection {

class CameraModel {
public:
    CameraModel();
    explicit CameraModel(const CameraCalibration& calib);

    void setIntrinsics(const cv::Mat& K);
    void setExtrinsics(const cv::Mat& R, const cv::Mat& t);
    void setDistortion(const cv::Mat& D);

    cv::Point2f project3DTo2D(const Point3D& point3d) const;
    cv::Mat getProjectionMatrix() const;
    cv::Mat getIntrinsics() const { return K_; }
    cv::Mat getRotation() const { return R_; }
    cv::Mat getTranslation() const { return t_; }

    void computeProjectionMatrix();

private:
    cv::Mat K_, R_, t_, D_;
    cv::Mat P_;  // 3x4 projection matrix

    void updateEigen();
};

class EpipolarMatcher {
public:
    EpipolarMatcher();

    void setFundamentalMatrix(const cv::Mat& F) { F_ = F; }
    void setEssentialMatrix(const cv::Mat& E) { E_ = E; }

    std::vector<cv::Point2f> findEpipolarMatches(
        const cv::Point2f& point2d_cam1,
        const cv::Mat& image_cam2,
        float search_radius = 10.0f
    ) const;

    float computeEpipolarDistance(
        const cv::Point2f& p1,
        const cv::Point2f& p2
    ) const;

private:
    cv::Mat F_, E_;  // Fundamental and Essential matrices
};

} // namespace bullet_detection
