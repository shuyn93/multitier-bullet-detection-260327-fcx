#pragma once
#include "core/Types.h"
#include <cmath>

namespace bullet_detection {

class ConfidenceCalibrator {
public:
    ConfidenceCalibrator();

    // Softmax temperature scaling
    static float softmaxTemperatureScale(
        float logit,
        float temperature = 1.0f
    );

    // Entropy penalty (uncertainty)
    static float entropyPenalty(float confidence);

    // Margin penalty (distance from decision boundary)
    static float marginPenalty(float margin);

    // Combined penalty
    float computeFinalConfidence(
        float raw_confidence,
        float entropy,
        float margin
    ) const;

private:
    float entropy_weight_ = 0.3f;
    float margin_weight_ = 0.2f;
};

inline float ConfidenceCalibrator::softmaxTemperatureScale(
    float logit,
    float temperature
) {
    if (temperature <= 0.0f) temperature = 1.0f;
    return 1.0f / (1.0f + std::exp(-logit / temperature));
}

inline float ConfidenceCalibrator::entropyPenalty(float confidence) {
    if (confidence <= 0.0f || confidence >= 1.0f) return 0.0f;
    return -confidence * std::log(confidence) - (1.0f - confidence) * std::log(1.0f - confidence);
}

inline float ConfidenceCalibrator::marginPenalty(float margin) {
    // Penalty increases as margin approaches 0 (uncertainty)
    return std::exp(-margin * margin);
}

} // namespace bullet_detection
