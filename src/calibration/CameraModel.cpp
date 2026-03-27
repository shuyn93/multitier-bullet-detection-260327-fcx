#include "calibration/CameraModel.h"
#include <opencv2/calib3d.hpp>

namespace bullet_detection {

CameraModel::CameraModel() {
    K_ = cv::Mat::eye(3, 3, CV_32F);
    R_ = cv::Mat::eye(3, 3, CV_32F);
    t_ = cv::Mat::zeros(3, 1, CV_32F);
    D_ = cv::Mat::zeros(4, 1, CV_32F);
    P_ = cv::Mat::zeros(3, 4, CV_32F);
    computeProjectionMatrix();
}

CameraModel::CameraModel(const CameraCalibration& calib) {
    K_ = calib.intrinsics.clone();
    R_ = calib.rvec.clone();
    t_ = calib.tvec.clone();
    D_ = calib.distortion.clone();

    if (R_.rows == 1 && R_.cols == 3) {
        cv::Mat R_mat;
        cv::Rodrigues(R_, R_mat);
        R_ = R_mat;
    }

    computeProjectionMatrix();
}

void CameraModel::setIntrinsics(const cv::Mat& K) {
    K_ = K.clone();
    computeProjectionMatrix();
}

void CameraModel::setExtrinsics(const cv::Mat& R, const cv::Mat& t) {
    R_ = R.clone();
    t_ = t.clone();
    computeProjectionMatrix();
}

void CameraModel::setDistortion(const cv::Mat& D) {
    D_ = D.clone();
}

cv::Point2f CameraModel::project3DTo2D(const Point3D& point3d) const {
    cv::Mat point_homogeneous = (cv::Mat_<float>(4, 1) << point3d.x, point3d.y, point3d.z, 1.0f);
    cv::Mat projected = P_ * point_homogeneous;

    if (projected.at<float>(2, 0) < 0.001f) {
        return cv::Point2f(-1, -1);
    }

    float x = projected.at<float>(0, 0) / projected.at<float>(2, 0);
    float y = projected.at<float>(1, 0) / projected.at<float>(2, 0);

    return cv::Point2f(x, y);
}

cv::Mat CameraModel::getProjectionMatrix() const {
    return P_.clone();
}

void CameraModel::computeProjectionMatrix() {
    P_ = K_.clone();

    cv::Mat temp(3, 4, CV_32F);
    R_.copyTo(temp.colRange(0, 3));
    t_.copyTo(temp.col(3));

    P_ = P_ * temp;
}

void CameraModel::updateEigen() {
}

// ===== EPIPOLAR MATCHER =====

EpipolarMatcher::EpipolarMatcher() {}

std::vector<cv::Point2f> EpipolarMatcher::findEpipolarMatches(
    const cv::Point2f& point2d_cam1,
    const cv::Mat& image_cam2,
    float search_radius
) const {
    std::vector<cv::Point2f> matches;

    if (F_.empty()) return matches;

    cv::Mat point_homo = (cv::Mat_<float>(3, 1) << point2d_cam1.x, point2d_cam1.y, 1.0f);
    cv::Mat line = F_ * point_homo;

    float a = line.at<float>(0, 0);
    float b = line.at<float>(1, 0);
    float c = line.at<float>(2, 0);

    float norm = std::sqrt(a * a + b * b);
    if (norm < 1e-6f) return matches;

    a /= norm;
    b /= norm;
    c /= norm;

    for (float t = -search_radius; t <= search_radius; t += 1.0f) {
        float x = point2d_cam1.x + t;
        if (x < 0 || x >= image_cam2.cols) continue;

        float y_dist = -a * x - c;
        if (std::abs(b) > 1e-6f) {
            float y = y_dist / b;
            if (y >= 0 && y < image_cam2.rows) {
                matches.push_back(cv::Point2f(x, y));
            }
        }
    }

    return matches;
}

float EpipolarMatcher::computeEpipolarDistance(
    const cv::Point2f& p1,
    const cv::Point2f& p2
) const {
    if (F_.empty()) return 1e6f;

    cv::Mat p1_homo = (cv::Mat_<float>(3, 1) << p1.x, p1.y, 1.0f);
    cv::Mat line = F_ * p1_homo;

    float a = line.at<float>(0, 0);
    float b = line.at<float>(1, 0);
    float c = line.at<float>(2, 0);

    float dist = std::abs(a * p2.x + b * p2.y + c) / std::sqrt(a * a + b * b + 1e-6f);

    return dist;
}

} // namespace bullet_detection
