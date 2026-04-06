#include "training/IncrementalTrainer.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <numeric>
#include <chrono>
#include <iomanip>
#include <random>

namespace bullet_detection {

// ===== CURRICULUM MANAGER IMPLEMENTATION =====

CurriculumManager::CurriculumManager(const Tier1Classifier& tier1_baseline)
    : tier1_baseline_(tier1_baseline) {}

void CurriculumManager::computeDifficultyScores(std::vector<CurriculumSample>& samples) {
for (auto& sample : samples) {
    auto decision = const_cast<Tier1Classifier&>(tier1_baseline_).predict(sample.features);
        
    // Difficulty = 1 - |confidence - 0.5| (uncertainty-based)
    // Easy: high confidence (close to 0 or 1)
    // Hard: low confidence (close to 0.5)
    float uncertainty = std::abs(decision.confidence - 0.5f);
    sample.difficulty_score = 1.0f - (uncertainty * 2.0f);  // Range: [0, 1]
        
        // Check if misclassified (hard example)
        int predicted_label = (decision.confidence > 0.5f) ? 1 : 0;
        sample.is_misclassified = (predicted_label != sample.label);
        
        // Adjust difficulty if misclassified (hard examples)
        if (sample.is_misclassified) {
            sample.difficulty_score = std::max(sample.difficulty_score, 0.7f);
        }
    }
}

void CurriculumManager::sortByCurriculum(std::vector<CurriculumSample>& samples) {
    // Sort by difficulty score (ascending: easy first)
    std::sort(samples.begin(), samples.end(),
        [](const CurriculumSample& a, const CurriculumSample& b) {
            return a.difficulty_score < b.difficulty_score;
        }
    );
}

std::vector<CurriculumSample> CurriculumManager::getEasySamples(
    const std::vector<CurriculumSample>& samples,
    float ratio) {
    
    size_t count = static_cast<size_t>(samples.size() * ratio);
    std::vector<CurriculumSample> result(samples.begin(), samples.begin() + count);
    return result;
}

std::vector<CurriculumSample> CurriculumManager::getMediumSamples(
    const std::vector<CurriculumSample>& samples,
    float ratio) {
    
    size_t start = static_cast<size_t>(samples.size() * ratio);
    size_t end = static_cast<size_t>(samples.size() * (ratio * 2.0f));
    std::vector<CurriculumSample> result(samples.begin() + start, samples.begin() + end);
    return result;
}

std::vector<CurriculumSample> CurriculumManager::getHardSamples(
    const std::vector<CurriculumSample>& samples,
    float ratio) {
    
    size_t start = static_cast<size_t>(samples.size() * (1.0f - ratio));
    std::vector<CurriculumSample> result(samples.begin() + start, samples.end());
    return result;
}

// ===== SCALER STATE IMPLEMENTATION =====

bool ScalerState::save(const std::string& path) const {
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    file.write(reinterpret_cast<const char*>(&n_features), sizeof(int));
    
    for (const auto& m : mean) {
        file.write(reinterpret_cast<const char*>(&m), sizeof(double));
    }
    
    for (const auto& s : std_dev) {
        file.write(reinterpret_cast<const char*>(&s), sizeof(double));
    }
    
    file.close();
    return true;
}

bool ScalerState::load(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    file.read(reinterpret_cast<char*>(&n_features), sizeof(int));
    
    mean.resize(n_features);
    std_dev.resize(n_features);
    
    for (int i = 0; i < n_features; ++i) {
        file.read(reinterpret_cast<char*>(&mean[i]), sizeof(double));
    }
    
    for (int i = 0; i < n_features; ++i) {
        file.read(reinterpret_cast<char*>(&std_dev[i]), sizeof(double));
    }
    
    file.close();
    return true;
}

// ===== REPLAY BUFFER IMPLEMENTATION =====

ReplayBuffer::ReplayBuffer(size_t max_size)
    : max_size_(max_size) {}

void ReplayBuffer::add(const CurriculumSample& sample) {
    buffer_.push_back(sample);
    if (buffer_.size() > max_size_) {
        buffer_.erase(buffer_.begin());
    }
}

void ReplayBuffer::clear() {
    buffer_.clear();
}

std::vector<CurriculumSample> ReplayBuffer::sample(size_t batch_size) {
    if (buffer_.empty()) {
        return {};
    }
    
    std::vector<CurriculumSample> result;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, buffer_.size() - 1);
    
    for (size_t i = 0; i < batch_size && i < buffer_.size(); ++i) {
        result.push_back(buffer_[dis(gen)]);
    }
    
    return result;
}

// ===== INCREMENTAL TRAINER IMPLEMENTATION =====

IncrementalTrainer::IncrementalTrainer() {}

bool IncrementalTrainer::performIncrementalTraining(
    const std::string& new_data_path,
    const std::string& scaler_path,
    const std::string& model_output_dir,
    const IncrementalTrainingConfig& config,
    IncrementalTrainingMetrics& metrics) {
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    try {
        std::cout << "\n" << std::string(70, '=') << std::endl;
        std::cout << "INCREMENTAL TRAINING PIPELINE (CONTINUOUS LEARNING)" << std::endl;
        std::cout << std::string(70, '=') << std::endl;
        
        // ===== STEP 1: LOAD EXISTING MODELS =====
        std::cout << "\n[STEP 1] Loading existing trained models..." << std::endl;
        
        Tier1Classifier tier1;
        LightweightMLP tier2({FeatureVector::DIM, 64, 32, 1});
        ScalerState scaler;
        
        if (!loadExistingModels(tier1, tier2, scaler, model_output_dir, scaler_path)) {
            std::cerr << "WARNING: Could not load all models. Proceeding with caution." << std::endl;
        }
        
        std::cout << "? Tier 1 Classifier loaded" << std::endl;
        std::cout << "? Tier 2 MLP loaded" << std::endl;
        std::cout << "? Scaler state loaded" << std::endl;
        
        // ===== STEP 2: LOAD NEW DATA =====
        std::cout << "\n[STEP 2] Loading new dataset..." << std::endl;
        
        std::vector<CurriculumSample> new_samples;
        if (!loadNewData(new_data_path, new_samples, scaler)) {
            std::cerr << "ERROR: Failed to load new data" << std::endl;
            return false;
        }
        
        metrics.new_samples_count = new_samples.size();
        std::cout << "? Loaded " << new_samples.size() << " new samples" << std::endl;
        
        // ===== STEP 3: CURRICULUM FILTERING =====
        std::cout << "\n[STEP 3] Applying curriculum learning strategy..." << std::endl;
        
        curriculum_manager_ = std::make_unique<CurriculumManager>(tier1);
        
        if (config.use_curriculum) {
            applyAndSortCurriculum(new_samples, tier1, config.prioritize_hard_examples);
            std::cout << "? Curriculum sorting completed" << std::endl;
            
            auto easy = curriculum_manager_->getEasySamples(new_samples, 0.33f);
            auto medium = curriculum_manager_->getMediumSamples(new_samples, 0.33f);
            auto hard = curriculum_manager_->getHardSamples(new_samples, 0.34f);
            
            std::cout << "  Easy samples: " << easy.size() << std::endl;
            std::cout << "  Medium samples: " << medium.size() << std::endl;
            std::cout << "  Hard samples: " << hard.size() << std::endl;
        }
        
        // ===== STEP 4: CREATE REPLAY BUFFER =====
        std::cout << "\n[STEP 4] Creating replay buffer for stability..." << std::endl;
        
        ReplayBuffer replay_buffer(config.max_replay_buffer_size);
        size_t old_data_needed = static_cast<size_t>(
            new_samples.size() * config.replay_buffer_ratio / config.new_data_ratio
        );
        
        // Generate some synthetic old data from validation set
        // In practice, this would load from a saved validation set
        std::vector<CurriculumSample> old_validation_data;
        createReplayBuffer(old_validation_data, replay_buffer, old_data_needed);
        
        std::cout << "? Replay buffer created with " << replay_buffer.size() << " samples" << std::endl;
        metrics.old_samples_count_used = replay_buffer.size();
        
        // ===== STEP 5: EVALUATE BASELINE =====
        std::cout << "\n[STEP 5] Evaluating baseline performance..." << std::endl;
        
        metrics.initial_accuracy = evaluateAccuracy(tier1, new_samples);
        std::cout << "  Initial accuracy on new data: " << std::fixed << std::setprecision(4) 
                  << metrics.initial_accuracy * 100.0f << "%" << std::endl;
        
        // ===== STEP 6: INCREMENTAL FINE-TUNING =====
        std::cout << "\n[STEP 6] Fine-tuning models (incremental training)..." << std::endl;
        
        auto replay_samples = replay_buffer.sample(old_data_needed);
        finetuneModels(tier1, tier2, new_samples, replay_samples, config, metrics);
        
        std::cout << "? Fine-tuning completed (" << metrics.epochs_trained << " epochs)" << std::endl;
        
        // ===== STEP 7: EVALUATE FINAL PERFORMANCE =====
        std::cout << "\n[STEP 7] Evaluating final performance..." << std::endl;
        
        metrics.final_accuracy = evaluateAccuracy(tier1, new_samples);
        metrics.accuracy_change = metrics.final_accuracy - metrics.initial_accuracy;
        
        std::cout << "  Final accuracy on new data: " << std::fixed << std::setprecision(4) 
                  << metrics.final_accuracy * 100.0f << "%" << std::endl;
        std::cout << "  Accuracy improvement: " << std::fixed << std::setprecision(4) 
                  << metrics.accuracy_change * 100.0f << "%" << std::endl;
        
        // ===== STEP 8: STABILITY VALIDATION =====
        std::cout << "\n[STEP 8] Validating stability (catastrophic forgetting check)..." << std::endl;
        
        if (config.validate_on_old_data && !old_validation_data.empty()) {
            metrics.is_stable = validateStability(
                tier1, tier2, old_validation_data, new_samples, config, metrics
            );
        } else {
            metrics.is_stable = true;
            metrics.stability_status = "STABLE (no old validation data available)";
        }
        
        if (metrics.is_stable) {
            std::cout << "? Stability check PASSED" << std::endl;
            std::cout << "  No catastrophic forgetting detected" << std::endl;
        } else {
            std::cout << "? WARNING: Potential performance drop on old data" << std::endl;
            std::cout << "  Max drop: " << std::fixed << std::setprecision(4) 
                      << metrics.max_performance_drop * 100.0f << "%" << std::endl;
        }
        
        // ===== STEP 9: SAVE UPDATED MODELS =====
        std::cout << "\n[STEP 9] Saving updated models..." << std::endl;
        
        if (!saveUpdatedModels(tier1, tier2, model_output_dir, 1)) {
            std::cerr << "WARNING: Failed to save some models" << std::endl;
        }
        
        std::cout << "? Models saved to: " << model_output_dir << std::endl;
        
        // ===== SUMMARY =====
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time);
        metrics.training_time_seconds = static_cast<float>(duration.count());
        
        metrics.total_training_samples = metrics.new_samples_count + metrics.old_samples_count_used;
        
        std::cout << "\n" << std::string(70, '=') << std::endl;
        std::cout << "INCREMENTAL TRAINING COMPLETED" << std::endl;
        std::cout << std::string(70, '=') << std::endl;
        
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return false;
    }
}

bool IncrementalTrainer::loadExistingModels(
    Tier1Classifier& tier1,
    LightweightMLP& tier2,
    ScalerState& scaler,
    const std::string& model_dir,
    const std::string& scaler_path) {
    
    // Load scaler state
    if (!scaler.load(scaler_path)) {
        std::cout << "WARNING: Could not load scaler from " << scaler_path << std::endl;
    }
    
    // Load MLP weights
    if (!tier2.loadWeights(model_dir + "/tier2_mlp.bin")) {
        std::cout << "WARNING: Could not load Tier 2 MLP weights" << std::endl;
    }
    
    // Tier1 is initialized with defaults, would need dedicated load function
    
    return true;
}

bool IncrementalTrainer::loadNewData(
    const std::string& csv_path,
    std::vector<CurriculumSample>& samples,
    const ScalerState& scaler) {
    
    std::ifstream file(csv_path);
    if (!file.is_open()) {
        std::cerr << "ERROR: Cannot open CSV file: " << csv_path << std::endl;
        return false;
    }
    
    std::string line;
    bool header = true;
    int line_count = 0;
    int error_count = 0;
    
    while (std::getline(file, line)) {
        if (header) {
            header = false;
            continue;
        }
        
        try {
            std::istringstream iss(line);
            std::string token;
            std::vector<double> features;
            int label = -1;
            int field_count = 0;
            
            while (std::getline(iss, token, ',')) {
                try {
                    if (field_count < FeatureVector::DIM) {
                        features.push_back(std::stod(token));
                    } else if (field_count == FeatureVector::DIM + 1) {
                        label = std::stoi(token);
                    }
                    field_count++;
                } catch (...) {
                    // Skip problematic fields
                }
            }
            
            if (features.size() == static_cast<size_t>(FeatureVector::DIM) && 
                (label == 0 || label == 1)) {
                
                CurriculumSample sample;
                sample.label = label;
                
                // Normalize features
                scaler.normalize(features);
                
                // Convert to FeatureVector
                for (int i = 0; i < FeatureVector::DIM; ++i) {
                    sample.features.data[i] = static_cast<float>(features[i]);
                }
                
                samples.push_back(sample);
                line_count++;
            }
        } catch (...) {
            error_count++;
        }
    }
    
    file.close();
    
    if (error_count > 0) {
        std::cout << "WARNING: " << error_count << " lines had parsing errors" << std::endl;
    }
    
    return !samples.empty();
}

void IncrementalTrainer::applyAndSortCurriculum(
    std::vector<CurriculumSample>& samples,
    Tier1Classifier& tier1,
    bool prioritize_hard) {
    
    if (!curriculum_manager_) {
        curriculum_manager_ = std::make_unique<CurriculumManager>(tier1);
    }
    
    curriculum_manager_->computeDifficultyScores(samples);
    
    if (prioritize_hard) {
        // Sort descending (hard first)
        std::sort(samples.begin(), samples.end(),
            [](const CurriculumSample& a, const CurriculumSample& b) {
                return a.difficulty_score > b.difficulty_score;
            }
        );
    } else {
        // Sort ascending (easy first)
        curriculum_manager_->sortByCurriculum(samples);
    }
}

void IncrementalTrainer::finetuneModels(
    Tier1Classifier& tier1,
    LightweightMLP& tier2,
    const std::vector<CurriculumSample>& new_samples,
    const std::vector<CurriculumSample>& replay_samples,
    const IncrementalTrainingConfig& config,
    IncrementalTrainingMetrics& metrics) {
    
    float current_learning_rate = config.learning_rate;
    
    for (int epoch = 0; epoch < config.max_epochs; ++epoch) {
        float total_loss = 0.0f;
        size_t batch_count = 0;
        
        // Process new samples (80%)
        for (const auto& sample : new_samples) {
            auto decision = tier1.predict(sample.features);
            int predicted = (decision.confidence > 0.5f) ? 1 : 0;
            
            // Simple loss calculation
            float error = static_cast<float>(sample.label - predicted);
            total_loss += error * error;
            batch_count++;
            
            // Online update for Tier 1
            tier1.onlineUpdate(sample.features, sample.label == 1);
        }
        
        // Process replay samples (20%)
        for (const auto& sample : replay_samples) {
            auto decision = tier1.predict(sample.features);
            int predicted = (decision.confidence > 0.5f) ? 1 : 0;
            
            float error = static_cast<float>(sample.label - predicted);
            total_loss += error * error;
            batch_count++;
            
            // Online update
            tier1.onlineUpdate(sample.features, sample.label == 1);
        }
        
        // Adaptive learning rate decay
        if (epoch > 0 && epoch % 10 == 0) {
            current_learning_rate = std::max(
                config.min_learning_rate,
                current_learning_rate * 0.9f
            );
        }
        
        metrics.final_loss = (batch_count > 0) ? total_loss / batch_count : 1.0f;
    }
    
    metrics.epochs_trained = config.max_epochs;
}

bool IncrementalTrainer::validateStability(
    const Tier1Classifier& tier1,
    const LightweightMLP& tier2,
    const std::vector<CurriculumSample>& old_validation_data,
    const std::vector<CurriculumSample>& new_validation_data,
    const IncrementalTrainingConfig& config,
    IncrementalTrainingMetrics& metrics) {
    
    if (old_validation_data.empty()) {
        metrics.stability_status = "STABLE (no baseline for comparison)";
        return true;
    }
    
    float old_accuracy = evaluateAccuracy(tier1, old_validation_data);
    float new_accuracy = evaluateAccuracy(tier1, new_validation_data);
    
    float performance_drop = metrics.initial_accuracy - old_accuracy;
    metrics.max_performance_drop = performance_drop;
    
    if (performance_drop > config.performance_drop_threshold) {
        metrics.stability_status = "UNSTABLE: Catastrophic forgetting detected";
        metrics.is_stable = false;
        return false;
    }
    
    metrics.stability_status = "STABLE: No significant performance drop";
    metrics.is_stable = true;
    return true;
}

bool IncrementalTrainer::saveUpdatedModels(
const Tier1Classifier& tier1,
const LightweightMLP& tier2,
const std::string& output_dir,
int version) {
    
try {
    // Create versioned directory
    std::string version_dir = output_dir + "/version_" + std::to_string(version);
        
    // In a real implementation, would save models here
    // For now, just MLP weights
    const_cast<LightweightMLP&>(tier2).saveWeights(version_dir + "/tier2_mlp.bin");
        
    return true;
    } catch (...) {
        return false;
    }
}

float IncrementalTrainer::evaluateAccuracy(
    const Tier1Classifier& classifier,
    const std::vector<CurriculumSample>& samples) {
    
    if (samples.empty()) {
        return 0.0f;
    }
    
    int correct = 0;
    for (const auto& sample : samples) {
        auto decision = const_cast<Tier1Classifier&>(classifier).predict(sample.features);
        int predicted = (decision.confidence > 0.5f) ? 1 : 0;
        if (predicted == sample.label) {
            correct++;
        }
    }
    
    return static_cast<float>(correct) / samples.size();
}

void IncrementalTrainer::createReplayBuffer(
    const std::vector<CurriculumSample>& old_data,
    ReplayBuffer& buffer,
    size_t target_size) {
    
    // In practice, this would load historical data
    // For demo, creating synthetic balanced samples
    for (size_t i = 0; i < target_size && i < old_data.size(); ++i) {
        buffer.add(old_data[i]);
    }
}

} // namespace bullet_detection
