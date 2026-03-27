#include "tier/Tiers.h"
#include <random>
#include <algorithm>
#include <numeric>
#include <cmath>
#include "core/ErrorHandler.h"
#include "core/FeatureValidator.h"

namespace bullet_detection {

// ===== NAIVE BAYES GAUSSIAN =====

NaiveBayesGaussian::NaiveBayesGaussian()
    : mean_pos_(FeatureVector::DIM, 1, CV_32F),
      mean_neg_(FeatureVector::DIM, 1, CV_32F),
      var_pos_(FeatureVector::DIM, 1, CV_32F),
      var_neg_(FeatureVector::DIM, 1, CV_32F) {
    mean_pos_.setTo(0.0f);
    mean_neg_.setTo(0.0f);
    var_pos_.setTo(0.1f);
    var_neg_.setTo(0.1f);
    trained_ = false;
}

ClassifierDecision NaiveBayesGaussian::predict(const FeatureVector& features) {
    if (!trained_) {
        ErrorLogger::getInstance().log(
            ErrorLogger::LogLevel::WARNING,
            "NaiveBayesGaussian: Model not trained, returning LOW_CONFIDENCE"
        );
        return {DecisionCode::LOW_CONFIDENCE, 0.5f, 0.5f, 0.0f, 1};
    }

    // Validate input features
    if (!FeatureValidator::isValid(features)) {
        ErrorLogger::getInstance().log(
            ErrorLogger::LogLevel::WARNING,
            "NaiveBayesGaussian: Invalid feature values detected"
        );
        return {DecisionCode::LOW_CONFIDENCE, 0.5f, 0.5f, 0.0f, 1};
    }

    // Compute log-likelihood using safe numerical operations
    float log_prob_pos = std::log(std::max(1e-6f, prior_pos_));
    float log_prob_neg = std::log(std::max(1e-6f, 1.0f - prior_pos_));

    for (int i = 0; i < FeatureVector::DIM; ++i) {
        float pdf_pos = gaussianPdf(features.data[i], mean_pos_.at<float>(i, 0), var_pos_.at<float>(i, 0));
        float pdf_neg = gaussianPdf(features.data[i], mean_neg_.at<float>(i, 0), var_neg_.at<float>(i, 0));

        log_prob_pos += pdf_pos;
        log_prob_neg += pdf_neg;
    }

    // Prevent overflow: use log-sum-exp trick
    float max_log = std::max(log_prob_pos, log_prob_neg);
    float prob_pos = std::exp(std::max(-700.0f, log_prob_pos - max_log));
    float prob_neg = std::exp(std::max(-700.0f, log_prob_neg - max_log));

    float total = prob_pos + prob_neg;
    if (total < 1e-10f) total = 1e-10f;

    float confidence = prob_pos / total;

    // Compute entropy safely
    float conf_clamped = std::max(1e-6f, std::min(1.0f - 1e-6f, confidence));
    float entropy = -conf_clamped * std::log(conf_clamped) -
                    (1.0f - conf_clamped) * std::log(1.0f - conf_clamped);

    DecisionCode code = (confidence > 0.7f) ? DecisionCode::ACCEPT : DecisionCode::LOW_CONFIDENCE;

    return {code, confidence, entropy, confidence - 0.5f, 1};
}

void NaiveBayesGaussian::train(
    const std::vector<FeatureVector>& positive_samples,
    const std::vector<FeatureVector>& negative_samples
) {
    if (positive_samples.empty() || negative_samples.empty()) return;

    mean_pos_.setTo(0.0f);
    mean_neg_.setTo(0.0f);

    for (const auto& sample : positive_samples) {
        for (int i = 0; i < FeatureVector::DIM; ++i) {
            mean_pos_.at<float>(i, 0) += sample.data[i];
        }
    }
    mean_pos_ /= static_cast<float>(positive_samples.size());

    for (const auto& sample : negative_samples) {
        for (int i = 0; i < FeatureVector::DIM; ++i) {
            mean_neg_.at<float>(i, 0) += sample.data[i];
        }
    }
    mean_neg_ /= static_cast<float>(negative_samples.size());

    var_pos_.setTo(0.0f);
    var_neg_.setTo(0.0f);

    for (const auto& sample : positive_samples) {
        for (int i = 0; i < FeatureVector::DIM; ++i) {
            float diff = sample.data[i] - mean_pos_.at<float>(i, 0);
            var_pos_.at<float>(i, 0) += diff * diff;
        }
    }
    var_pos_ /= static_cast<float>(positive_samples.size());
    var_pos_ += 0.001f;

    for (const auto& sample : negative_samples) {
        for (int i = 0; i < FeatureVector::DIM; ++i) {
            float diff = sample.data[i] - mean_neg_.at<float>(i, 0);
            var_neg_.at<float>(i, 0) += diff * diff;
        }
    }
    var_neg_ /= static_cast<float>(negative_samples.size());
    var_neg_ += 0.001f;

    prior_pos_ = static_cast<float>(positive_samples.size()) /
                 (positive_samples.size() + negative_samples.size());

    trained_ = true;
}

void NaiveBayesGaussian::onlineUpdate(
    const FeatureVector& features,
    bool is_positive,
    float learning_rate
) {
    if (!trained_) return;

    cv::Mat& target_mean = is_positive ? mean_pos_ : mean_neg_;
    cv::Mat& target_var = is_positive ? var_pos_ : var_neg_;

    for (int i = 0; i < FeatureVector::DIM; ++i) {
        float delta = features.data[i] - target_mean.at<float>(i, 0);
        target_mean.at<float>(i, 0) += learning_rate * delta;
        target_var.at<float>(i, 0) += learning_rate * (delta * delta - target_var.at<float>(i, 0));
    }
}

float NaiveBayesGaussian::gaussianPdf(float x, float mean, float var) const {
    // Ensure variance is not too small
    var = std::max(1e-6f, var);

    // Compute log-probability to avoid numerical issues
    float exponent = -((x - mean) * (x - mean)) / (2.0f * var);
    float denominator = std::sqrt(2.0f * 3.14159265358979f * var);

    // Return log of PDF for numerical stability
    return exponent - std::log(denominator + 1e-10f);
}

// ===== GAUSSIAN MIXTURE MODEL =====

GaussianMixtureModel::GaussianMixtureModel(int n_components)
    : n_components_(n_components) {}

ClassifierDecision GaussianMixtureModel::predict(const FeatureVector& features) {
    if (!trained_) {
        return {DecisionCode::LOW_CONFIDENCE, 0.5f, 0.5f, 0.0f, 1};
    }

    float score_pos = 0.0f, score_neg = 0.0f;

    for (int i = 0; i < n_components_; ++i) {
        score_pos += weights_pos_[i] * evaluateGaussian(features, means_pos_[i], covariances_pos_[i]);
        score_neg += weights_neg_[i] * evaluateGaussian(features, means_neg_[i], covariances_neg_[i]);
    }

    float confidence = score_pos / (score_pos + score_neg + 1e-6f);
    float entropy = -confidence * std::log(std::max(1e-6f, confidence)) -
                    (1.0f - confidence) * std::log(std::max(1e-6f, 1.0f - confidence));

    DecisionCode code = (confidence > 0.65f) ? DecisionCode::ACCEPT : DecisionCode::LOW_CONFIDENCE;

    return {code, confidence, entropy, confidence - 0.5f, 1};
}

void GaussianMixtureModel::train(
    const std::vector<FeatureVector>& positive_samples,
    const std::vector<FeatureVector>& negative_samples
) {
    means_pos_.resize(n_components_);
    means_neg_.resize(n_components_);
    covariances_pos_.resize(n_components_);
    covariances_neg_.resize(n_components_);
    weights_pos_.resize(n_components_, 1.0f / n_components_);
    weights_neg_.resize(n_components_, 1.0f / n_components_);

    for (int c = 0; c < n_components_; ++c) {
        if (c < static_cast<int>(positive_samples.size())) {
            means_pos_[c] = positive_samples[c].toMat().clone();
        } else {
            means_pos_[c] = cv::Mat(FeatureVector::DIM, 1, CV_32F);
            cv::randu(means_pos_[c], 0.0f, 1.0f);
        }

        if (c < static_cast<int>(negative_samples.size())) {
            means_neg_[c] = negative_samples[c].toMat().clone();
        } else {
            means_neg_[c] = cv::Mat(FeatureVector::DIM, 1, CV_32F);
            cv::randu(means_neg_[c], 0.0f, 1.0f);
        }

        covariances_pos_[c] = cv::Mat(FeatureVector::DIM, 1, CV_32F, cv::Scalar(0.05f));
        covariances_neg_[c] = cv::Mat(FeatureVector::DIM, 1, CV_32F, cv::Scalar(0.05f));
    }

    trained_ = true;
}

float GaussianMixtureModel::evaluateGaussian(
    const FeatureVector& features,
    const cv::Mat& mean,
    const cv::Mat& cov
) const {
    float exponent = 0.0f;
    float denominator = 1.0f;

    for (int i = 0; i < FeatureVector::DIM; ++i) {
        float diff = features.data[i] - mean.at<float>(i, 0);
        float c = cov.at<float>(i, 0);
        exponent += (diff * diff) / (2.0f * c);
        denominator *= std::sqrt(2.0f * 3.14159f * c);
    }

    return std::exp(-exponent) / (denominator + 1e-10f);
}

// ===== TREE ENSEMBLE =====

TreeEnsemble::TreeEnsemble(int n_trees, int max_depth)
    : n_trees_(n_trees), max_depth_(max_depth) {}

ClassifierDecision TreeEnsemble::predict(const FeatureVector& features) const {
    if (!trained_ || trees_.empty()) {
        return {DecisionCode::LOW_CONFIDENCE, 0.5f, 0.5f, 0.0f, 1};
    }

    float score = 0.0f;
    for (const auto& tree : trees_) {
        score += predictTree(features, tree);
    }
    float confidence = score / n_trees_;

    DecisionCode code = (confidence > 0.6f) ? DecisionCode::ACCEPT : DecisionCode::LOW_CONFIDENCE;

    return {code, confidence, 0.0f, confidence - 0.5f, 1};
}

void TreeEnsemble::train(
    const std::vector<FeatureVector>& positive_samples,
    const std::vector<FeatureVector>& negative_samples
) {
    trees_.resize(n_trees_);
    trained_ = true;
}

float TreeEnsemble::predictTree(const FeatureVector& features,
                                 const std::vector<DecisionNode>& tree) const {
    float sum = 0.0f;
    for (int i = 0; i < FeatureVector::DIM; ++i) {
        sum += features.data[i];
    }
    return sum / FeatureVector::DIM;
}

// ===== TIER 1 CLASSIFIER =====

Tier1Classifier::Tier1Classifier()
    : nb_(), gmm_(2), forest_(5, 5) {}

ClassifierDecision Tier1Classifier::predict(const FeatureVector& features) {
    auto nb_decision = nb_.predict(features);
    auto gmm_decision = gmm_.predict(features);
    auto forest_decision = forest_.predict(features);

    float weighted_confidence = (nb_weight_ * nb_decision.confidence +
                                 gmm_weight_ * gmm_decision.confidence +
                                 forest_weight_ * forest_decision.confidence);

    float entropy = (nb_decision.entropy + gmm_decision.entropy) / 2.0f;
    float margin = weighted_confidence - 0.5f;

    DecisionCode code;
    if (weighted_confidence > 0.75f) {
        code = DecisionCode::ACCEPT;
    } else if (weighted_confidence > 0.5f) {
        code = DecisionCode::LOW_CONFIDENCE;
    } else {
        code = DecisionCode::REJECT;
    }

    return {code, weighted_confidence, entropy, margin, 1};
}

void Tier1Classifier::onlineUpdate(const FeatureVector& features, bool is_bullet_hole) {
    nb_.onlineUpdate(features, is_bullet_hole);
}

void Tier1Classifier::trainModels(
    const std::vector<FeatureVector>& positive_samples,
    const std::vector<FeatureVector>& negative_samples
) {
    nb_.train(positive_samples, negative_samples);
    gmm_.train(positive_samples, negative_samples);
    forest_.train(positive_samples, negative_samples);
}

// ===== LIGHTWEIGHT MLP =====

LightweightMLP::LightweightMLP(const std::vector<int>& layer_sizes)
    : layer_sizes_(layer_sizes) {
    if (layer_sizes.size() < 2) return;

    weights_.resize(layer_sizes.size() - 1);
    biases_.resize(layer_sizes.size() - 1);

    std::random_device rd;
    std::mt19937 gen(42);
    std::normal_distribution<float> dist(0.0f, 0.01f);

    for (size_t i = 0; i < layer_sizes.size() - 1; ++i) {
        weights_[i] = cv::Mat(layer_sizes[i + 1], layer_sizes[i], CV_32F);
        biases_[i] = cv::Mat(layer_sizes[i + 1], 1, CV_32F);

        for (int r = 0; r < weights_[i].rows; ++r) {
            for (int c = 0; c < weights_[i].cols; ++c) {
                weights_[i].at<float>(r, c) = dist(gen);
            }
        }
        biases_[i].setTo(0.0f);
    }
}

ClassifierDecision LightweightMLP::predict(const FeatureVector& features) {
    auto output = forward(features);
    float confidence = output.at<float>(0, 0);

    DecisionCode code = (confidence > 0.65f) ? DecisionCode::ACCEPT : DecisionCode::LOW_CONFIDENCE;
    float entropy = -confidence * std::log(std::max(1e-6f, confidence)) -
                    (1.0f - confidence) * std::log(std::max(1e-6f, 1.0f - confidence));

    return {code, confidence, entropy, confidence - 0.5f, 2};
}

void LightweightMLP::train(
    const std::vector<FeatureVector>& positive_samples,
    const std::vector<FeatureVector>& negative_samples,
    int epochs,
    float learning_rate
) {
    for (int epoch = 0; epoch < epochs; ++epoch) {
        for (const auto& sample : positive_samples) {
            backpropagate(sample, true, learning_rate);
        }
        for (const auto& sample : negative_samples) {
            backpropagate(sample, false, learning_rate);
        }
    }
}

bool LightweightMLP::loadWeights(const std::string& path) {
    return false;
}

bool LightweightMLP::saveWeights(const std::string& path) {
    return false;
}

cv::Mat LightweightMLP::relu(const cv::Mat& x) const {
    return cv::max(x, 0.0f);
}

cv::Mat LightweightMLP::relu_derivative(const cv::Mat& x) const {
    cv::Mat deriv(x.size(), CV_32F);
    for (int i = 0; i < x.total(); ++i) {
        deriv.at<float>(i) = (x.at<float>(i) > 0.0f) ? 1.0f : 0.0f;
    }
    return deriv;
}

float LightweightMLP::sigmoid(float x) const {
    return 1.0f / (1.0f + std::exp(-x));
}

cv::Mat LightweightMLP::forward(const FeatureVector& features) {
    cv::Mat x(FeatureVector::DIM, 1, CV_32F);
    for (int i = 0; i < FeatureVector::DIM; ++i) {
        x.at<float>(i, 0) = features.data[i];
    }

    for (size_t i = 0; i < weights_.size(); ++i) {
        x = weights_[i] * x + biases_[i];
        if (i < weights_.size() - 1) {
            x = relu(x);
        } else {
            x.at<float>(0, 0) = sigmoid(x.at<float>(0, 0));
        }
    }

    return x;
}

void LightweightMLP::backpropagate(const FeatureVector& features, bool is_positive,
                                    float learning_rate) {
    auto output = forward(features);
    float target = is_positive ? 1.0f : 0.0f;
    float error = output.at<float>(0, 0) - target;

    biases_.back().at<float>(0, 0) -= learning_rate * error;
}

// ===== TIER 2 CLASSIFIER =====

Tier2Classifier::Tier2Classifier()
    : mlp_({FeatureVector::DIM, 16, 8, 1}) {}

ClassifierDecision Tier2Classifier::predict(const FeatureVector& features) {
    return mlp_.predict(features);
}

void Tier2Classifier::trainModel(
    const std::vector<FeatureVector>& positive_samples,
    const std::vector<FeatureVector>& negative_samples
) {
    mlp_.train(positive_samples, negative_samples, 50, 0.01f);
}

// ===== SIMPLE SVM =====

SimpleSVM::SimpleSVM() {}

DecisionCode SimpleSVM::predict(const FeatureVector& features) const {
    float decision = b_;
    for (size_t i = 0; i < support_vectors_.size(); ++i) {
        decision += alpha_[i] * rbfKernel(support_vectors_[i], features);
    }
    return (decision > 0.0f) ? DecisionCode::ACCEPT : DecisionCode::REJECT;
}

void SimpleSVM::train(
    const std::vector<FeatureVector>& positive_samples,
    const std::vector<FeatureVector>& negative_samples
) {
    support_vectors_.insert(support_vectors_.end(), positive_samples.begin(),
                           positive_samples.end());
    support_vectors_.insert(support_vectors_.end(), negative_samples.begin(),
                           negative_samples.end());

    alpha_.assign(support_vectors_.size(), 0.1f);
    for (size_t i = positive_samples.size(); i < support_vectors_.size(); ++i) {
        alpha_[i] = -0.1f;
    }
}

float SimpleSVM::rbfKernel(const FeatureVector& x1, const FeatureVector& x2) const {
    float sum_sq = 0.0f;
    for (int i = 0; i < FeatureVector::DIM; ++i) {
        float diff = x1.data[i] - x2.data[i];
        sum_sq += diff * diff;
    }
    return std::exp(-gamma_ * sum_sq);
}

// ===== TIER 3 CLASSIFIER =====

Tier3Classifier::Tier3Classifier() {
    svm_ensemble_.resize(n_svms_);
}

DecisionCode Tier3Classifier::predict(const FeatureVector& features) {
    int accept_votes = 0;
    for (const auto& svm : svm_ensemble_) {
        if (svm.predict(features) == DecisionCode::ACCEPT) {
            accept_votes++;
        }
    }
    return (accept_votes >= n_svms_ / 2 + 1) ? DecisionCode::ACCEPT : DecisionCode::REJECT;
}

void Tier3Classifier::trainModel(
    const std::vector<FeatureVector>& positive_samples,
    const std::vector<FeatureVector>& negative_samples
) {
    for (auto& svm : svm_ensemble_) {
        svm.train(positive_samples, negative_samples);
    }
}

} // namespace bullet_detection

