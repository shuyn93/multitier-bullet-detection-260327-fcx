#include "confidence/Calibration.h"

namespace bullet_detection {

ConfidenceCalibrator::ConfidenceCalibrator() {}

float ConfidenceCalibrator::computeFinalConfidence(
    float raw_confidence,
    float entropy,
    float margin
) const {
    float entropy_penalty = entropyPenalty(entropy);
    float margin_penalty = marginPenalty(margin);

    float adjusted_confidence = raw_confidence - entropy_weight_ * entropy_penalty -
                               margin_weight_ * margin_penalty;

    return std::max(0.0f, std::min(1.0f, adjusted_confidence));
}

} // namespace bullet_detection
