#pragma once
#include "core/Types.h"
#include <vector>
#include <string>

namespace bullet_detection {

class CandidateDetector {
public:
    CandidateDetector();

    std::vector<CandidateRegion> detectCandidates(
        const cv::Mat& frame_ir,
        uint64_t frame_id
    );

private:
    std::vector<std::vector<cv::Point>> detectContours(
        const cv::Mat& frame,
        int threshold = 200
    );

    float computeDetectionScore(const std::vector<cv::Point>& contour);

    int min_area_ = 25;  // pixels
    int max_area_ = 10000;
    float min_circularity_ = 0.3f;
};

} // namespace bullet_detection
