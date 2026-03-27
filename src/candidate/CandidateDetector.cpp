#include "candidate/CandidateDetector.h"
#include <algorithm>
#include <cmath>
#include "core/MathUtils.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace bullet_detection {

CandidateDetector::CandidateDetector() {}

std::vector<CandidateRegion> CandidateDetector::detectCandidates(
    const cv::Mat& frame_ir,
    uint64_t frame_id
) {
    std::vector<CandidateRegion> candidates;

    auto contours = detectContours(frame_ir);

    for (const auto& contour : contours) {
        cv::Rect bbox = cv::boundingRect(contour);

        if (bbox.width < 3 || bbox.height < 3) continue;
        if (bbox.area() < min_area_ || bbox.area() > max_area_) continue;

        cv::Mat roi = frame_ir(bbox);

        double perimeter = cv::arcLength(contour, true);
        double area = cv::contourArea(contour);
        
        // Use shared utility for circularity calculation
        float circularity = math_utils::computeCircularity(area, perimeter);

        if (circularity < min_circularity_) continue;

        float detection_score = computeDetectionScore(contour);

        CandidateRegion candidate;
        candidate.bbox = bbox;
        candidate.contour = contour;
        candidate.roi_image = roi.clone();
        candidate.frame_id = frame_id;
        candidate.detection_score = detection_score;

        candidates.push_back(candidate);
    }

    return candidates;
}

std::vector<std::vector<cv::Point>> CandidateDetector::detectContours(
    const cv::Mat& frame,
    int threshold
) {
    std::vector<std::vector<cv::Point>> contours;

    if (frame.empty()) return contours;

    cv::Mat thresh;
    cv::threshold(frame, thresh, threshold, 255, cv::THRESH_BINARY);

    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(3, 3));
    cv::morphologyEx(thresh, thresh, cv::MORPH_CLOSE, kernel, cv::Point(-1, -1), 1);

    cv::findContours(thresh.clone(), contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    return contours;
}

float CandidateDetector::computeDetectionScore(const std::vector<cv::Point>& contour) {
    if (contour.empty()) return 0.0f;

    double area = cv::contourArea(contour);
    double perimeter = cv::arcLength(contour, true);

    // Use shared utility for circularity calculation
    return math_utils::computeCircularity(area, perimeter);
}

} // namespace bullet_detection
