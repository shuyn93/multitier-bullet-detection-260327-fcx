#pragma once

#include <vector>
#include <string>
#include <memory>
#include <map>
#include <fstream>
#include <cmath>
#include "core/Types.h"
#include "tier/Tiers.h"
#include "tier/ImprovedModels.h"

namespace bullet_detection {

// ===== CURRICULUM LEARNING MANAGER =====

struct CurriculumSample {
    FeatureVector features;
    int label;
    float difficulty_score;  // 0.0 = easy, 1.0 = hard
    bool is_misclassified;
    
    CurriculumSample() 
        : label(0), difficulty_score(0.5f), is_misclassified(false) {}
};

class CurriculumManager {
public:
    explicit CurriculumManager(const Tier1Classifier& tier1_baseline);
    
    // Compute difficulty scores for new samples
    void computeDifficultyScores(std::vector<CurriculumSample>& samples);
    
    // Sort samples by difficulty (easy -> hard)
    void sortByCurriculum(std::vector<CurriculumSample>& samples);
    
    // Get easy samples (confidence near 0.5, low uncertainty)
    std::vector<CurriculumSample> getEasySamples(
        const std::vector<CurriculumSample>& samples,
        float ratio = 0.33f
    );
    
    // Get medium samples
    std::vector<CurriculumSample> getMediumSamples(
        const std::vector<CurriculumSample>& samples,
        float ratio = 0.33f
    );
    
    // Get hard samples (misclassified or uncertain)
    std::vector<CurriculumSample> getHardSamples(
        const std::vector<CurriculumSample>& samples,
        float ratio = 0.34f
    );

private:
    const Tier1Classifier& tier1_baseline_;
};

// ===== SCALER STATE FOR NORMALIZATION =====

struct ScalerState {
    std::vector<double> mean;
    std::vector<double> std_dev;
    int n_features;
    
    ScalerState() : n_features(0) {}
    
    void normalize(std::vector<double>& features) const {
        if (features.size() != static_cast<size_t>(n_features)) {
            return;
        }
        for (size_t i = 0; i < features.size(); ++i) {
            if (std_dev[i] > 1e-8) {
                features[i] = (features[i] - mean[i]) / std_dev[i];
            }
        }
    }
    
    void normalize(FeatureVector& fv) const {
        if (n_features != FeatureVector::DIM) {
            return;
        }
        for (int i = 0; i < FeatureVector::DIM; ++i) {
            if (std_dev[i] > 1e-8) {
                fv.data[i] = (fv.data[i] - static_cast<float>(mean[i])) 
                           / static_cast<float>(std_dev[i]);
            }
        }
    }
    
    bool save(const std::string& path) const;
    bool load(const std::string& path);
};

// ===== REPLAY BUFFER FOR STABILITY =====

class ReplayBuffer {
public:
    explicit ReplayBuffer(size_t max_size = 1000);
    
    void add(const CurriculumSample& sample);
    void clear();
    std::vector<CurriculumSample> sample(size_t batch_size);
    size_t size() const { return buffer_.size(); }
    bool isEmpty() const { return buffer_.empty(); }

private:
    std::vector<CurriculumSample> buffer_;
    size_t max_size_;
};

// ===== INCREMENTAL TRAINING ENGINE =====

struct IncrementalTrainingConfig {
    // Learning strategy
    float learning_rate = 0.001f;
    int max_epochs = 50;
    float min_learning_rate = 0.0001f;
    
    // Curriculum learning
    bool use_curriculum = true;
    bool prioritize_hard_examples = true;
    
    // Stability control
    float replay_buffer_ratio = 0.20f;  // 20% old data
    float new_data_ratio = 0.80f;       // 80% new data
    size_t max_replay_buffer_size = 2000;
    
    // Validation
    float performance_drop_threshold = 0.05f;  // 5% drop triggers alert
    bool validate_on_old_data = true;
};

struct IncrementalTrainingMetrics {
    // Dataset information
    size_t new_samples_count;
    size_t old_samples_count_used;
    size_t total_training_samples;
    
    // Performance metrics
    float initial_accuracy;
    float final_accuracy;
    float accuracy_change;
    float precision_before;
    float precision_after;
    float recall_before;
    float recall_after;
    
    // Training metrics
    float initial_loss;
    float final_loss;
    int epochs_trained;
    float training_time_seconds;
    
    // Stability metrics
    bool is_stable;
    float max_performance_drop;
    std::string stability_status;
    
    IncrementalTrainingMetrics()
        : new_samples_count(0), old_samples_count_used(0), total_training_samples(0),
          initial_accuracy(0.0f), final_accuracy(0.0f), accuracy_change(0.0f),
          precision_before(0.0f), precision_after(0.0f),
          recall_before(0.0f), recall_after(0.0f),
          initial_loss(1.0f), final_loss(1.0f), epochs_trained(0),
          training_time_seconds(0.0f), is_stable(true), max_performance_drop(0.0f) {}
};

class IncrementalTrainer {
public:
    IncrementalTrainer();
    
    // Main incremental training pipeline
    bool performIncrementalTraining(
        const std::string& new_data_path,
        const std::string& scaler_path,
        const std::string& model_output_dir,
        const IncrementalTrainingConfig& config,
        IncrementalTrainingMetrics& metrics
    );
    
    // Step 1: Load existing models and scaler
    bool loadExistingModels(
        Tier1Classifier& tier1,
        LightweightMLP& tier2,
        ScalerState& scaler,
        const std::string& model_dir,
        const std::string& scaler_path
    );
    
    // Step 2: Load new data
    bool loadNewData(
        const std::string& csv_path,
        std::vector<CurriculumSample>& samples,
        const ScalerState& scaler
    );
    
    // Step 3: Curriculum filtering
    void applyAndSortCurriculum(
        std::vector<CurriculumSample>& samples,
        Tier1Classifier& tier1,
        bool prioritize_hard = true
    );
    
    // Step 4: Incremental fine-tuning
    void finetuneModels(
        Tier1Classifier& tier1,
        LightweightMLP& tier2,
        const std::vector<CurriculumSample>& new_samples,
        const std::vector<CurriculumSample>& replay_samples,
        const IncrementalTrainingConfig& config,
        IncrementalTrainingMetrics& metrics
    );
    
    // Step 5: Stability validation
    bool validateStability(
        const Tier1Classifier& tier1,
        const LightweightMLP& tier2,
        const std::vector<CurriculumSample>& old_validation_data,
        const std::vector<CurriculumSample>& new_validation_data,
        const IncrementalTrainingConfig& config,
        IncrementalTrainingMetrics& metrics
    );
    
    // Step 6: Save updated models
    bool saveUpdatedModels(
        const Tier1Classifier& tier1,
        const LightweightMLP& tier2,
        const std::string& output_dir,
        int version = 0
    );
    
    // Helper: Evaluate classifier on samples
    float evaluateAccuracy(
        const Tier1Classifier& classifier,
        const std::vector<CurriculumSample>& samples
    );
    
    // Helper: Create balanced replay buffer from old data
    void createReplayBuffer(
        const std::vector<CurriculumSample>& old_data,
        ReplayBuffer& buffer,
        size_t target_size
    );

private:
    std::unique_ptr<CurriculumManager> curriculum_manager_;
};

} // namespace bullet_detection
