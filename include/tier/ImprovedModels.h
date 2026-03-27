#pragma once

#include <vector>
#include <cmath>
#include <opencv2/opencv.hpp>
#include "core/Types.h"
#include "core/MathUtils.h"

namespace bullet_detection {

// ===== IMPROVED GAUSSIAN MIXTURE MODEL WITH EM =====

class ImprovedGMM {
public:
    explicit ImprovedGMM(int n_components = 3);

    ClassifierDecision predict(const FeatureVector& features) const;
    
    void train(
        const std::vector<FeatureVector>& positive_samples,
        const std::vector<FeatureVector>& negative_samples,
        int max_iterations = 20,
        float convergence_threshold = 1e-4f
    );

    bool isTrained() const { return trained_; }
    int getComponents() const { return n_components_; }

private:
    int n_components_;
    bool trained_ = false;

    // Positive class GMM
    std::vector<cv::Mat> means_pos_;
    std::vector<cv::Mat> covariances_pos_;  // Diagonal matrices
    std::vector<float> weights_pos_;
    float log_likelihood_pos_ = -1e6f;

    // Negative class GMM
    std::vector<cv::Mat> means_neg_;
    std::vector<cv::Mat> covariances_neg_;  // Diagonal matrices
    std::vector<float> weights_neg_;
    float log_likelihood_neg_ = -1e6f;

    // EM Algorithm helpers
    struct GMM {
        std::vector<cv::Mat> means;
        std::vector<cv::Mat> covariances;
        std::vector<float> weights;
        float log_likelihood = -1e6f;
    };

    GMM trainEM(
        const std::vector<FeatureVector>& samples,
        int max_iterations,
        float convergence_threshold
    );

    // E-step: Compute responsibilities
    std::vector<std::vector<float>> eStep(
        const std::vector<FeatureVector>& samples,
        const GMM& gmm
    ) const;

    // M-step: Update parameters
    void mStep(
        const std::vector<FeatureVector>& samples,
        const std::vector<std::vector<float>>& responsibilities,
        GMM& gmm
    ) const;

    // Compute multivariate Gaussian density
    float gaussianDensity(
        const FeatureVector& x,
        const cv::Mat& mean,
        const cv::Mat& cov
    ) const;

    // Compute log-likelihood
    float computeLogLikelihood(
        const std::vector<FeatureVector>& samples,
        const GMM& gmm
    ) const;

    // Initialize with k-means
    std::vector<cv::Mat> initializeMeansWithKMeans(
        const std::vector<FeatureVector>& samples
    );
};

// ===== IMPROVED DECISION TREE =====

class ImprovedDecisionTree {
public:
    explicit ImprovedDecisionTree(int max_depth = 6);

    float predict(const FeatureVector& features) const;

    void train(
        const std::vector<FeatureVector>& positive_samples,
        const std::vector<FeatureVector>& negative_samples
    );

    bool isTrained() const { return trained_; }

private:
    struct Node {
        bool is_leaf = false;
        float leaf_value = 0.5f;  // For leaves: class probability
        int feature_idx = -1;     // For splits: feature to split on
        float threshold = 0.0f;   // For splits: threshold value
        std::unique_ptr<Node> left;
        std::unique_ptr<Node> right;
    };

    int max_depth_;
    bool trained_ = false;
    std::unique_ptr<Node> root_;

    struct SplitInfo {
        float gain = 0.0f;
        int feature = -1;
        float threshold = 0.0f;
    };

    // Build tree recursively
    std::unique_ptr<Node> buildTree(
        const std::vector<FeatureVector>& samples,
        const std::vector<int>& labels,
        int depth
    );

    // Find best split for a node
    SplitInfo findBestSplit(
        const std::vector<FeatureVector>& samples,
        const std::vector<int>& labels
    ) const;

    // Compute information gain
    float computeInformationGain(
        const std::vector<int>& parent_labels,
        const std::vector<int>& left_labels,
        const std::vector<int>& right_labels
    ) const;

    // Compute entropy
    float computeEntropy(const std::vector<int>& labels) const;

    // Traverse tree for prediction
    float traverseTree(const FeatureVector& features, const Node* node) const;
};

// ===== IMPROVED MLP WITH PROPER BACKPROPAGATION =====

class ImprovedMLP {
public:
    explicit ImprovedMLP(const std::vector<int>& layer_sizes);

    ClassifierDecision predict(const FeatureVector& features) const;

    void train(
        const std::vector<FeatureVector>& positive_samples,
        const std::vector<FeatureVector>& negative_samples,
        int epochs = 100,
        float learning_rate = 0.01f,
        bool use_momentum = true
    );

    bool saveWeights(const std::string& path) const;
    bool loadWeights(const std::string& path);

private:
    std::vector<int> layer_sizes_;
    std::vector<cv::Mat> weights_;
    std::vector<cv::Mat> biases_;
    std::vector<cv::Mat> weight_velocities_;  // For momentum
    std::vector<cv::Mat> bias_velocities_;

    bool trained_ = false;
    float momentum_beta_ = 0.9f;

    // Forward pass with activation caching
    struct ForwardCache {
        std::vector<cv::Mat> z_values;  // Pre-activation
        std::vector<cv::Mat> a_values;  // Post-activation
    };

    ForwardCache forward(const FeatureVector& features) const;
    cv::Mat getSingleOutput(const ForwardCache& cache) const;

    // Backward pass
    void backward(
        const FeatureVector& features,
        float target,
        const ForwardCache& cache,
        float learning_rate
    );

    // Activation functions
    cv::Mat relu(const cv::Mat& x) const;
    cv::Mat reluDerivative(const cv::Mat& z) const;
    float sigmoid(float x) const;
    float sigmoidDerivative(float x) const;
};

} // namespace bullet_detection
