#include "tier/ImprovedModels.h"
#include "core/MathUtils.h"
#include <algorithm>
#include <numeric>
#include <random>
#include <cmath>

namespace bullet_detection {

// ===== IMPROVED GMM WITH EM ALGORITHM =====

ImprovedGMM::ImprovedGMM(int n_components)
    : n_components_(n_components) {
    if (n_components < 1) n_components_ = 2;
    if (n_components > 10) n_components_ = 10;
}

ClassifierDecision ImprovedGMM::predict(const FeatureVector& features) const {
    if (!trained_) {
        return {DecisionCode::LOW_CONFIDENCE, 0.5f, 0.5f, 0.0f, 1};
    }

    float score_pos = 0.0f, score_neg = 0.0f;

    // Compute weighted density for positive class
    for (int k = 0; k < n_components_; ++k) {
        float density = gaussianDensity(features, means_pos_[k], covariances_pos_[k]);
        score_pos += weights_pos_[k] * density;
    }

    // Compute weighted density for negative class
    for (int k = 0; k < n_components_; ++k) {
        float density = gaussianDensity(features, means_neg_[k], covariances_neg_[k]);
        score_neg += weights_neg_[k] * density;
    }

    float confidence = score_pos / (score_pos + score_neg + 1e-6f);
    confidence = std::max(0.0f, std::min(1.0f, confidence));

    float entropy = -confidence * std::log(std::max(1e-6f, confidence)) -
                    (1.0f - confidence) * std::log(std::max(1e-6f, 1.0f - confidence));

    DecisionCode code = (confidence > 0.65f) ? DecisionCode::ACCEPT : DecisionCode::LOW_CONFIDENCE;

    return {code, confidence, entropy, confidence - 0.5f, 1};
}

void ImprovedGMM::train(
    const std::vector<FeatureVector>& positive_samples,
    const std::vector<FeatureVector>& negative_samples,
    int max_iterations,
    float convergence_threshold
) {
    if (positive_samples.empty() || negative_samples.empty()) {
        return;
    }

    // Train EM for positive class
    auto gmm_pos = trainEM(positive_samples, max_iterations, convergence_threshold);
    means_pos_ = gmm_pos.means;
    covariances_pos_ = gmm_pos.covariances;
    weights_pos_ = gmm_pos.weights;
    log_likelihood_pos_ = gmm_pos.log_likelihood;

    // Train EM for negative class
    auto gmm_neg = trainEM(negative_samples, max_iterations, convergence_threshold);
    means_neg_ = gmm_neg.means;
    covariances_neg_ = gmm_neg.covariances;
    weights_neg_ = gmm_neg.weights;
    log_likelihood_neg_ = gmm_neg.log_likelihood;

    trained_ = true;
}

ImprovedGMM::GMM ImprovedGMM::trainEM(
    const std::vector<FeatureVector>& samples,
    int max_iterations,
    float convergence_threshold
) {
    GMM gmm;
    gmm.means.resize(n_components_);
    gmm.covariances.resize(n_components_);
    gmm.weights.resize(n_components_, 1.0f / n_components_);

    // Initialize means using k-means
    gmm.means = initializeMeansWithKMeans(samples);

    // Initialize covariances as identity matrices scaled
    for (int k = 0; k < n_components_; ++k) {
        gmm.covariances[k] = cv::Mat(FeatureVector::DIM, 1, CV_32F);
        gmm.covariances[k].setTo(0.1f);  // Diagonal values
    }

    float prev_likelihood = -1e10f;

    // EM iterations
    for (int iter = 0; iter < max_iterations; ++iter) {
        // E-step: Compute responsibilities
        auto responsibilities = eStep(samples, gmm);

        // M-step: Update parameters
        mStep(samples, responsibilities, gmm);

        // Compute log-likelihood
        gmm.log_likelihood = computeLogLikelihood(samples, gmm);

        // Check convergence
        float likelihood_change = gmm.log_likelihood - prev_likelihood;
        if (std::abs(likelihood_change) < convergence_threshold) {
            break;
        }

        prev_likelihood = gmm.log_likelihood;
    }

    return gmm;
}

std::vector<std::vector<float>> ImprovedGMM::eStep(
    const std::vector<FeatureVector>& samples,
    const GMM& gmm
) const {
    int n_samples = samples.size();
    std::vector<std::vector<float>> responsibilities(n_samples, std::vector<float>(n_components_, 0.0f));

    for (int i = 0; i < n_samples; ++i) {
        float prob_sum = 0.0f;

        // Compute responsibilities
        for (int k = 0; k < n_components_; ++k) {
            float density = gaussianDensity(samples[i], gmm.means[k], gmm.covariances[k]);
            responsibilities[i][k] = gmm.weights[k] * density;
            prob_sum += responsibilities[i][k];
        }

        // Normalize
        if (prob_sum > 1e-10f) {
            for (int k = 0; k < n_components_; ++k) {
                responsibilities[i][k] /= prob_sum;
            }
        }
    }

    return responsibilities;
}

void ImprovedGMM::mStep(
    const std::vector<FeatureVector>& samples,
    const std::vector<std::vector<float>>& responsibilities,
    GMM& gmm
) const {
    int n_samples = samples.size();

    for (int k = 0; k < n_components_; ++k) {
        // Compute effective sample size
        float Nk = 0.0f;
        for (int i = 0; i < n_samples; ++i) {
            Nk += responsibilities[i][k];
        }

        // Update weight
        gmm.weights[k] = Nk / n_samples;

        // Update mean
        cv::Mat new_mean = cv::Mat::zeros(FeatureVector::DIM, 1, CV_32F);
        for (int i = 0; i < n_samples; ++i) {
            for (int d = 0; d < FeatureVector::DIM; ++d) {
                new_mean.at<float>(d, 0) += responsibilities[i][k] * samples[i].data[d];
            }
        }
        if (Nk > 1e-6f) {
            new_mean /= Nk;
        }
        gmm.means[k] = new_mean;

        // Update covariance (diagonal)
        cv::Mat new_cov = cv::Mat::zeros(FeatureVector::DIM, 1, CV_32F);
        for (int i = 0; i < n_samples; ++i) {
            for (int d = 0; d < FeatureVector::DIM; ++d) {
                float diff = samples[i].data[d] - gmm.means[k].at<float>(d, 0);
                new_cov.at<float>(d, 0) += responsibilities[i][k] * diff * diff;
            }
        }
        if (Nk > 1e-6f) {
            new_cov /= Nk;
            // Add small regularization to prevent singularity
            new_cov += 0.01f;
        }
        gmm.covariances[k] = new_cov;
    }
}

float ImprovedGMM::gaussianDensity(
    const FeatureVector& x,
    const cv::Mat& mean,
    const cv::Mat& cov
) const {
    float exponent = 0.0f;
    float denominator = 1.0f;

    const float M_PI = 3.14159265358979f;

    for (int d = 0; d < FeatureVector::DIM; ++d) {
        float diff = x.data[d] - mean.at<float>(d, 0);
        float c = std::max(1e-6f, cov.at<float>(d, 0));

        exponent -= (diff * diff) / (2.0f * c);
        denominator *= std::sqrt(2.0f * M_PI * c);
    }

    float density = std::exp(exponent) / (denominator + 1e-10f);
    return std::max(1e-10f, density);
}

float ImprovedGMM::computeLogLikelihood(
    const std::vector<FeatureVector>& samples,
    const GMM& gmm
) const {
    float log_likelihood = 0.0f;

    for (const auto& sample : samples) {
        float prob = 0.0f;
        for (int k = 0; k < n_components_; ++k) {
            float density = gaussianDensity(sample, gmm.means[k], gmm.covariances[k]);
            prob += gmm.weights[k] * density;
        }

        log_likelihood += std::log(std::max(1e-10f, prob));
    }

    return log_likelihood;
}

std::vector<cv::Mat> ImprovedGMM::initializeMeansWithKMeans(
    const std::vector<FeatureVector>& samples
) {
    std::vector<cv::Mat> means(n_components_);

    // Random initialization (simple k-means++)
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, samples.size() - 1);

    means[0] = samples[dis(gen)].toMat().clone();

    for (int k = 1; k < n_components_; ++k) {
        float max_dist = 0.0f;
        int best_idx = 0;

        for (size_t i = 0; i < samples.size(); ++i) {
            float min_dist = 1e10f;
            for (int j = 0; j < k; ++j) {
                float dist_sq = 0.0f;
                for (int d = 0; d < FeatureVector::DIM; ++d) {
                    float diff = samples[i].data[d] - means[j].at<float>(d, 0);
                    dist_sq += diff * diff;
                }
                min_dist = std::min(min_dist, dist_sq);
            }

            if (min_dist > max_dist) {
                max_dist = min_dist;
                best_idx = i;
            }
        }

        means[k] = samples[best_idx].toMat().clone();
    }

    return means;
}

// ===== IMPROVED DECISION TREE =====

ImprovedDecisionTree::ImprovedDecisionTree(int max_depth)
    : max_depth_(max_depth) {}

float ImprovedDecisionTree::predict(const FeatureVector& features) const {
    if (!trained_ || !root_) {
        return 0.5f;
    }
    return traverseTree(features, root_.get());
}

void ImprovedDecisionTree::train(
    const std::vector<FeatureVector>& positive_samples,
    const std::vector<FeatureVector>& negative_samples
) {
    if (positive_samples.empty() || negative_samples.empty()) {
        return;
    }

    // Combine samples with labels
    std::vector<FeatureVector> all_samples;
    std::vector<int> labels;

    all_samples.insert(all_samples.end(), positive_samples.begin(), positive_samples.end());
    for (size_t i = 0; i < positive_samples.size(); ++i) {
        labels.push_back(1);
    }

    all_samples.insert(all_samples.end(), negative_samples.begin(), negative_samples.end());
    for (size_t i = 0; i < negative_samples.size(); ++i) {
        labels.push_back(0);
    }

    // Build tree
    root_ = buildTree(all_samples, labels, 0);
    trained_ = true;
}

std::unique_ptr<ImprovedDecisionTree::Node> ImprovedDecisionTree::buildTree(
    const std::vector<FeatureVector>& samples,
    const std::vector<int>& labels,
    int depth
) {
    auto node = std::make_unique<Node>();

    // Stopping criteria
    if (depth >= max_depth_ || samples.size() < 2) {
        node->is_leaf = true;

        // Compute majority class
        int pos_count = 0;
        for (int label : labels) {
            if (label == 1) pos_count++;
        }
        node->leaf_value = static_cast<float>(pos_count) / labels.size();
        return node;
    }

    // Find best split
    auto split = findBestSplit(samples, labels);

    if (split.gain < 1e-6f) {
        // No good split found
        node->is_leaf = true;
        int pos_count = 0;
        for (int label : labels) {
            if (label == 1) pos_count++;
        }
        node->leaf_value = static_cast<float>(pos_count) / labels.size();
        return node;
    }

    // Split samples
    std::vector<FeatureVector> left_samples, right_samples;
    std::vector<int> left_labels, right_labels;

    for (size_t i = 0; i < samples.size(); ++i) {
        if (samples[i].data[split.feature] < split.threshold) {
            left_samples.push_back(samples[i]);
            left_labels.push_back(labels[i]);
        } else {
            right_samples.push_back(samples[i]);
            right_labels.push_back(labels[i]);
        }
    }

    if (left_samples.empty() || right_samples.empty()) {
        // Empty split
        node->is_leaf = true;
        int pos_count = 0;
        for (int label : labels) {
            if (label == 1) pos_count++;
        }
        node->leaf_value = static_cast<float>(pos_count) / labels.size();
        return node;
    }

    // Recursive building
    node->is_leaf = false;
    node->feature_idx = split.feature;
    node->threshold = split.threshold;
    node->left = buildTree(left_samples, left_labels, depth + 1);
    node->right = buildTree(right_samples, right_labels, depth + 1);

    return node;
}

ImprovedDecisionTree::SplitInfo ImprovedDecisionTree::findBestSplit(
    const std::vector<FeatureVector>& samples,
    const std::vector<int>& labels
) const {
    SplitInfo best_split;

    // Try each feature
    for (int feature = 0; feature < FeatureVector::DIM; ++feature) {
        // Collect unique thresholds (sorted feature values)
        std::vector<float> thresholds;
        for (const auto& sample : samples) {
            thresholds.push_back(sample.data[feature]);
        }
        std::sort(thresholds.begin(), thresholds.end());
        thresholds.erase(std::unique(thresholds.begin(), thresholds.end()), thresholds.end());

        // Try each threshold
        for (size_t t = 0; t < thresholds.size() - 1; ++t) {
            float threshold = (thresholds[t] + thresholds[t + 1]) / 2.0f;

            // Split
            std::vector<int> left_labels, right_labels;
            for (size_t i = 0; i < samples.size(); ++i) {
                if (samples[i].data[feature] < threshold) {
                    left_labels.push_back(labels[i]);
                } else {
                    right_labels.push_back(labels[i]);
                }
            }

            if (left_labels.empty() || right_labels.empty()) continue;

            // Compute information gain
            float gain = computeInformationGain(labels, left_labels, right_labels);

            if (gain > best_split.gain) {
                best_split.gain = gain;
                best_split.feature = feature;
                best_split.threshold = threshold;
            }
        }
    }

    return best_split;
}

float ImprovedDecisionTree::computeInformationGain(
    const std::vector<int>& parent_labels,
    const std::vector<int>& left_labels,
    const std::vector<int>& right_labels
) const {
    float parent_entropy = computeEntropy(parent_labels);

    float left_weight = static_cast<float>(left_labels.size()) / parent_labels.size();
    float right_weight = static_cast<float>(right_labels.size()) / parent_labels.size();

    float left_entropy = computeEntropy(left_labels);
    float right_entropy = computeEntropy(right_labels);

    float weighted_child_entropy = left_weight * left_entropy + right_weight * right_entropy;

    return parent_entropy - weighted_child_entropy;
}

float ImprovedDecisionTree::computeEntropy(const std::vector<int>& labels) const {
    if (labels.empty()) return 0.0f;

    int pos_count = 0;
    for (int label : labels) {
        if (label == 1) pos_count++;
    }

    float p_pos = static_cast<float>(pos_count) / labels.size();
    float p_neg = 1.0f - p_pos;

    if (p_pos < 1e-6f || p_neg < 1e-6f) return 0.0f;

    return -p_pos * std::log(p_pos) - p_neg * std::log(p_neg);
}

float ImprovedDecisionTree::traverseTree(
    const FeatureVector& features,
    const Node* node
) const {
    if (node->is_leaf) {
        return node->leaf_value;
    }

    if (features.data[node->feature_idx] < node->threshold) {
        if (node->left) {
            return traverseTree(features, node->left.get());
        }
    } else {
        if (node->right) {
            return traverseTree(features, node->right.get());
        }
    }

    return 0.5f;  // Fallback
}

// ===== IMPROVED MLP WITH PROPER BACKPROPAGATION =====

ImprovedMLP::ImprovedMLP(const std::vector<int>& layer_sizes)
    : layer_sizes_(layer_sizes) {
    if (layer_sizes.size() < 2) return;

    weights_.resize(layer_sizes.size() - 1);
    biases_.resize(layer_sizes.size() - 1);
    weight_velocities_.resize(layer_sizes.size() - 1);
    bias_velocities_.resize(layer_sizes.size() - 1);

    std::random_device rd;
    std::mt19937 gen(42);

    // He initialization for ReLU layers
    for (size_t i = 0; i < layer_sizes.size() - 1; ++i) {
        float std_dev = std::sqrt(2.0f / layer_sizes[i]);
        std::normal_distribution<float> dist(0.0f, std_dev);

        weights_[i] = cv::Mat(layer_sizes[i + 1], layer_sizes[i], CV_32F);
        biases_[i] = cv::Mat(layer_sizes[i + 1], 1, CV_32F);
        weight_velocities_[i] = cv::Mat::zeros(layer_sizes[i + 1], layer_sizes[i], CV_32F);
        bias_velocities_[i] = cv::Mat::zeros(layer_sizes[i + 1], 1, CV_32F);

        for (int r = 0; r < weights_[i].rows; ++r) {
            for (int c = 0; c < weights_[i].cols; ++c) {
                weights_[i].at<float>(r, c) = dist(gen);
            }
        }

        biases_[i].setTo(0.0f);
    }
}

ClassifierDecision ImprovedMLP::predict(const FeatureVector& features) const {
    auto cache = forward(features);
    auto output_mat = getSingleOutput(cache);
    float output = output_mat.at<float>(0, 0);

    DecisionCode code = (output > 0.65f) ? DecisionCode::ACCEPT : DecisionCode::LOW_CONFIDENCE;
    float entropy = -output * std::log(std::max(1e-6f, output)) -
                    (1.0f - output) * std::log(std::max(1e-6f, 1.0f - output));

    return {code, output, entropy, output - 0.5f, 2};
}

void ImprovedMLP::train(
    const std::vector<FeatureVector>& positive_samples,
    const std::vector<FeatureVector>& negative_samples,
    int epochs,
    float learning_rate,
    bool use_momentum
) {
    for (int epoch = 0; epoch < epochs; ++epoch) {
        for (const auto& sample : positive_samples) {
            backward(sample, 1.0f, forward(sample), learning_rate);
        }
        for (const auto& sample : negative_samples) {
            backward(sample, 0.0f, forward(sample), learning_rate);
        }
    }

    trained_ = true;
}

ImprovedMLP::ForwardCache ImprovedMLP::forward(const FeatureVector& features) const {
    ForwardCache cache;
    cache.z_values.resize(weights_.size());
    cache.a_values.resize(weights_.size() + 1);

    // Input layer
    cache.a_values[0] = cv::Mat(FeatureVector::DIM, 1, CV_32F);
    for (int i = 0; i < FeatureVector::DIM; ++i) {
        cache.a_values[0].at<float>(i, 0) = features.data[i];
    }

    // Hidden and output layers
    for (size_t i = 0; i < weights_.size(); ++i) {
        cache.z_values[i] = weights_[i] * cache.a_values[i] + biases_[i];

        if (i < weights_.size() - 1) {
            cache.a_values[i + 1] = relu(cache.z_values[i]);
        } else {
            cache.a_values[i + 1] = cv::Mat(1, 1, CV_32F);
            cache.a_values[i + 1].at<float>(0, 0) = sigmoid(cache.z_values[i].at<float>(0, 0));
        }
    }

    return cache;
}

cv::Mat ImprovedMLP::getSingleOutput(const ForwardCache& cache) const {
    if (cache.a_values.empty()) {
        return cv::Mat(1, 1, CV_32F, cv::Scalar(0.5f));
    }
    return cache.a_values.back();
}

void ImprovedMLP::backward(
    const FeatureVector& features,
    float target,
    const ForwardCache& cache,
    float learning_rate
) {
    // Compute output error
    float output = cache.a_values.back().at<float>(0, 0);
    float error = output - target;

    // Backpropagate through layers
    cv::Mat delta(1, 1, CV_32F);
    delta.at<float>(0, 0) = error * sigmoidDerivative(cache.z_values.back().at<float>(0, 0));

    // Update output layer
    cv::Mat weight_grad = delta * cache.a_values[cache.a_values.size() - 2].t();
    weights_.back() -= learning_rate * weight_grad;
    biases_.back() -= learning_rate * delta;

    // Backpropagate through hidden layers
    for (int i = weights_.size() - 2; i >= 0; --i) {
        delta = weights_[i + 1].t() * delta;
        delta = delta.mul(reluDerivative(cache.z_values[i]));

        cv::Mat weight_grad = delta * cache.a_values[i].t();
        weights_[i] -= learning_rate * weight_grad;
        biases_[i] -= learning_rate * delta;
    }
}

cv::Mat ImprovedMLP::relu(const cv::Mat& x) const {
    cv::Mat result = x.clone();
    for (int i = 0; i < result.total(); ++i) {
        result.at<float>(i) = std::max(0.0f, result.at<float>(i));
    }
    return result;
}

cv::Mat ImprovedMLP::reluDerivative(const cv::Mat& z) const {
    cv::Mat deriv(z.size(), CV_32F);
    for (int i = 0; i < z.total(); ++i) {
        deriv.at<float>(i) = (z.at<float>(i) > 0.0f) ? 1.0f : 0.0f;
    }
    return deriv;
}

float ImprovedMLP::sigmoid(float x) const {
    if (x > 500.0f) return 1.0f;
    if (x < -500.0f) return 0.0f;
    return 1.0f / (1.0f + std::exp(-x));
}

float ImprovedMLP::sigmoidDerivative(float x) const {
    float s = sigmoid(x);
    return s * (1.0f - s);
}

bool ImprovedMLP::saveWeights(const std::string& path) const {
    // TODO: Implement file saving
    return false;
}

bool ImprovedMLP::loadWeights(const std::string& path) {
    // TODO: Implement file loading
    return false;
}

} // namespace bullet_detection
