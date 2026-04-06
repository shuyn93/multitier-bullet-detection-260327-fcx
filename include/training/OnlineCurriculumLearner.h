#pragma once

#include <vector>
#include <string>
#include <memory>
#include <map>
#include <deque>
#include <fstream>
#include <cmath>
#include <chrono>
#include "core/Types.h"
#include "tier/Tiers.h"
#include "tier/ImprovedModels.h"
#include "training/IncrementalTrainer.h"

namespace bullet_detection {

// ===== ONLINE CURRICULUM LEARNING CONFIGURATION =====

struct OnlineCurriculumConfig {
    // Loop control
    int max_iterations = 10;
    int samples_per_iteration = 1000;
    
    // Training configuration
    float learning_rate = 0.001f;
    int epochs_per_iteration = 50;
    float min_learning_rate = 0.0001f;
    
    // Curriculum strategy
    bool use_curriculum = true;
    bool focus_on_hard_samples = true;
    
    // Data mixing
    float new_data_ratio = 0.70f;      // 70% new
    float old_data_ratio = 0.30f;      // 30% old (replay buffer)
    
    // Convergence criteria
    float improvement_threshold = 0.001f;  // Stop if improvement < 0.1%
    int convergence_patience = 3;          // Stop after 3 iterations without improvement
    
    // Data generation parameters
    bool generate_hard_samples = true;
    bool increase_diversity = true;
    int max_dataset_size = 50000;
    
    // Validation
    float validation_split = 0.20f;
    bool validate_on_every_iteration = true;
};

// ===== ITERATION METRICS =====

struct IterationMetrics {
    int iteration_number;
    size_t samples_added;
    size_t total_dataset_size;
    
    float accuracy_before;
    float accuracy_after;
    float accuracy_improvement;
    
    float precision_before;
    float precision_after;
    float recall_before;
    float recall_after;
    float f1_before;
    float f1_after;
    
    float loss_before;
    float loss_after;
    
    int hard_samples_identified;
    int easy_samples_generated;
    int medium_samples_generated;
    int hard_samples_generated;
    
    float training_time_seconds;
    float data_generation_time_seconds;
    
    std::string notes;
    
    IterationMetrics()
        : iteration_number(0), samples_added(0), total_dataset_size(0),
          accuracy_before(0.0f), accuracy_after(0.0f), accuracy_improvement(0.0f),
          precision_before(0.0f), precision_after(0.0f),
          recall_before(0.0f), recall_after(0.0f),
          f1_before(0.0f), f1_after(0.0f),
          loss_before(0.0f), loss_after(0.0f),
          hard_samples_identified(0), easy_samples_generated(0),
          medium_samples_generated(0), hard_samples_generated(0),
          training_time_seconds(0.0f), data_generation_time_seconds(0.0f) {}
};

// ===== ONLINE TRAINING HISTORY =====

struct OnlineTrainingHistory {
    std::vector<IterationMetrics> iterations;
    std::vector<float> accuracy_trend;
    std::vector<float> recall_trend;
    std::vector<float> loss_trend;
    std::vector<size_t> dataset_size_trend;
    
    float best_accuracy = 0.0f;
    int best_iteration = 0;
    
    size_t total_samples_generated = 0;
    float total_training_time = 0.0f;
};

// ===== HARD SAMPLE DETECTOR =====

class HardSampleDetector {
public:
    struct HardSample {
        FeatureVector features;
        int true_label;
        int predicted_label;
        float prediction_confidence;
        float margin_to_decision_boundary;  // Distance to 0.5 confidence
        
        HardSample() : true_label(-1), predicted_label(-1), 
                      prediction_confidence(0.0f), margin_to_decision_boundary(0.0f) {}
    };
    
    explicit HardSampleDetector(const Tier1Classifier& classifier);
    
    // Identify hard samples from dataset
    std::vector<HardSample> detectHardSamples(
        const std::vector<CurriculumSample>& samples,
        float margin_threshold = 0.1f
    );
    
    // Classify sample difficulty
    float computeHardness(
        const FeatureVector& features,
        int true_label
    );

private:
    const Tier1Classifier& classifier_;
};

// ===== DATA AUGMENTATION COORDINATOR =====

class DataAugmentationCoordinator {
public:
    DataAugmentationCoordinator();
    
    // Generate new curriculum samples based on hard samples
    bool generateHardSamples(
        const std::vector<HardSampleDetector::HardSample>& hard_samples,
        size_t num_to_generate,
        std::vector<CurriculumSample>& output_samples
    );
    
    // Increase diversity in generated samples
    void augmentDiversity(
        std::vector<CurriculumSample>& samples,
        int variation_level = 1
    );
    
    // Validate generated samples
    bool validateSamples(
        const std::vector<CurriculumSample>& samples
    );
    
private:
    // Synthetic data generation
    CurriculumSample generateSyntheticVariation(
        const HardSampleDetector::HardSample& base_sample
    );
};

// ===== ONLINE CURRICULUM LEARNER (MAIN ENGINE) =====

class OnlineCurriculumLearner {
public:
    OnlineCurriculumLearner();
    
    // Main online learning loop
    bool runOnlineCurriculumLoop(
        const std::string& initial_dataset_path,
        const std::string& model_dir,
        const std::string& scaler_path,
        const std::string& data_generator_script,
        const OnlineCurriculumConfig& config,
        OnlineTrainingHistory& history
    );
    
    // Individual steps
    
    // Step 1: Load current model
    bool loadCurrentModel(
        Tier1Classifier& tier1,
        LightweightMLP& tier2,
        ScalerState& scaler,
        const std::string& model_dir,
        const std::string& scaler_path
    );
    
    // Step 2: Incremental training
    bool incrementalTrain(
        Tier1Classifier& tier1,
        LightweightMLP& tier2,
        const std::string& dataset_path,
        const ScalerState& scaler,
        const IncrementalTrainingConfig& train_config,
        IterationMetrics& metrics
    );
    
    // Step 3: Evaluate performance
    bool evaluatePerformance(
        const Tier1Classifier& tier1,
        const std::vector<CurriculumSample>& validation_data,
        IterationMetrics& metrics
    );
    
    // Step 4: Identify hard samples
    std::vector<HardSampleDetector::HardSample> identifyHardSamples(
        const Tier1Classifier& tier1,
        const std::vector<CurriculumSample>& dataset
    );
    
    // Step 5: Generate new data
    bool generateNewData(
        const std::vector<HardSampleDetector::HardSample>& hard_samples,
        size_t num_samples,
        const std::string& generator_script,
        std::vector<CurriculumSample>& new_samples,
        IterationMetrics& metrics
    );
    
    // Step 6: Curriculum integration (tag samples)
    void integrateInCurriculum(
        std::vector<CurriculumSample>& samples
    );
    
    // Step 7: Create replay buffer
    void createReplayBuffer(
        const std::vector<CurriculumSample>& old_data,
        const std::vector<CurriculumSample>& new_data,
        std::vector<CurriculumSample>& mixed_batch,
        float new_data_ratio
    );
    
    // Step 8: Validate data
    bool validateDataIntegrity(
        const std::vector<CurriculumSample>& samples
    );
    
    // Step 9: Save updated models
    bool saveUpdatedModels(
        const Tier1Classifier& tier1,
        const LightweightMLP& tier2,
        const std::string& model_dir,
        int iteration
    );
    
    // Step 10: Logging
    bool logIterationResults(
        const std::string& log_file,
        const IterationMetrics& metrics,
        const OnlineTrainingHistory& history
    );
    
    // Convergence check
    bool checkConvergence(
        const OnlineTrainingHistory& history,
        const OnlineCurriculumConfig& config
    );
    
    // Load and manage dataset
    bool loadDataset(
        const std::string& csv_path,
        std::vector<CurriculumSample>& samples,
        const ScalerState& scaler
    );
    
    // Split dataset into train/validation
    void splitDataset(
        const std::vector<CurriculumSample>& full_dataset,
        std::vector<CurriculumSample>& train_data,
        std::vector<CurriculumSample>& validation_data,
        float validation_ratio
    );

private:
    std::unique_ptr<HardSampleDetector> hard_sample_detector_;
    std::unique_ptr<DataAugmentationCoordinator> augmentation_coordinator_;
};

} // namespace bullet_detection
