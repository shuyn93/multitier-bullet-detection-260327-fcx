#pragma once
#include "core/Types.h"
#include "confidence/Calibration.h"
#include <vector>
#include <memory>
#include <opencv2/opencv.hpp>

namespace bullet_detection {

// ===== TIER 1: FAST MODELS =====

class NaiveBayesGaussian {
public:
    NaiveBayesGaussian();

    ClassifierDecision predict(const FeatureVector& features);
    void train(const std::vector<FeatureVector>& positive_samples,
               const std::vector<FeatureVector>& negative_samples);
    void onlineUpdate(const FeatureVector& features, bool is_positive, float learning_rate = 0.01f);

private:
    cv::Mat mean_pos_, mean_neg_;
    cv::Mat var_pos_, var_neg_;
    float prior_pos_ = 0.5f;
    bool trained_ = false;

    float gaussianPdf(float x, float mean, float var) const;
};

class GaussianMixtureModel {
public:
    GaussianMixtureModel(int n_components = 2);

    ClassifierDecision predict(const FeatureVector& features);
    void train(const std::vector<FeatureVector>& positive_samples,
               const std::vector<FeatureVector>& negative_samples);

private:
    int n_components_;
    std::vector<cv::Mat> means_pos_, means_neg_;
    std::vector<cv::Mat> covariances_pos_, covariances_neg_;
    std::vector<float> weights_pos_, weights_neg_;
    bool trained_ = false;

    float evaluateGaussian(const FeatureVector& features,
                          const cv::Mat& mean,
                          const cv::Mat& cov) const;
};

class TreeEnsemble {
public:
    TreeEnsemble(int n_trees = 10, int max_depth = 5);
    
    // Prevent copying of internal tree structures
    TreeEnsemble(const TreeEnsemble&) = delete;
    TreeEnsemble& operator=(const TreeEnsemble&) = delete;
    
    // Allow move semantics
    TreeEnsemble(TreeEnsemble&&) noexcept = default;
    TreeEnsemble& operator=(TreeEnsemble&&) noexcept = default;

    ClassifierDecision predict(const FeatureVector& features) const;
    void train(const std::vector<FeatureVector>& positive_samples,
               const std::vector<FeatureVector>& negative_samples);

private:
    struct DecisionNode {
        int feature_idx = 0;
        float threshold = 0.0f;
        int left_idx = -1;   // Index of left child (or leaf value index)
        int right_idx = -1;  // Index of right child (or leaf value index)
        float leaf_value = 0.0f;
        bool is_leaf = false;
    };

    int n_trees_;
    int max_depth_;
    std::vector<std::vector<DecisionNode>> trees_;
    bool trained_ = false;

    float predictTree(const FeatureVector& features, const std::vector<DecisionNode>& tree) const;
};

class Tier1Classifier {
public:
    Tier1Classifier();

    ClassifierDecision predict(const FeatureVector& features);
    void onlineUpdate(const FeatureVector& features, bool is_bullet_hole);

    void trainModels(const std::vector<FeatureVector>& positive_samples,
                     const std::vector<FeatureVector>& negative_samples);

private:
    NaiveBayesGaussian nb_;
    GaussianMixtureModel gmm_;
    TreeEnsemble forest_;
    ConfidenceCalibrator calibrator_;

    float nb_weight_ = 0.4f;
    float gmm_weight_ = 0.35f;
    float forest_weight_ = 0.25f;
};

// ===== TIER 2: LIGHTWEIGHT MLP =====

class LightweightMLP {
public:
    LightweightMLP(const std::vector<int>& layer_sizes);

    ClassifierDecision predict(const FeatureVector& features);
    void train(const std::vector<FeatureVector>& positive_samples,
               const std::vector<FeatureVector>& negative_samples,
               int epochs = 100,
               float learning_rate = 0.01f);

    bool loadWeights(const std::string& path);
    bool saveWeights(const std::string& path);

private:
    std::vector<cv::Mat> weights_;
    std::vector<cv::Mat> biases_;
    std::vector<int> layer_sizes_;
    ConfidenceCalibrator calibrator_;

    cv::Mat relu(const cv::Mat& x) const;
    cv::Mat relu_derivative(const cv::Mat& x) const;
    float sigmoid(float x) const;
    cv::Mat forward(const FeatureVector& features);

    void backpropagate(const FeatureVector& features, bool is_positive, float learning_rate);
};

class Tier2Classifier {
public:
    Tier2Classifier();

    ClassifierDecision predict(const FeatureVector& features);

    void trainModel(const std::vector<FeatureVector>& positive_samples,
                    const std::vector<FeatureVector>& negative_samples);

private:
    LightweightMLP mlp_;
};

// ===== TIER 3: SVM ENSEMBLE (ASYNC) =====

class SimpleSVM {
public:
    SimpleSVM();

    DecisionCode predict(const FeatureVector& features) const;
    void train(const std::vector<FeatureVector>& positive_samples,
               const std::vector<FeatureVector>& negative_samples);

private:
    std::vector<FeatureVector> support_vectors_;
    std::vector<float> alpha_;
    float b_ = 0.0f;
    float C_ = 1.0f;  // Regularization parameter
    float gamma_ = 0.1f;  // RBF kernel parameter

    float rbfKernel(const FeatureVector& x1, const FeatureVector& x2) const;
};

class Tier3Classifier {
public:
    Tier3Classifier();

    DecisionCode predict(const FeatureVector& features);

    void trainModel(const std::vector<FeatureVector>& positive_samples,
                    const std::vector<FeatureVector>& negative_samples);

private:
    std::vector<SimpleSVM> svm_ensemble_;
    int n_svms_ = 3;
};

} // namespace bullet_detection
