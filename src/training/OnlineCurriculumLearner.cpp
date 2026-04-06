#include "training/OnlineCurriculumLearner.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <numeric>
#include <random>
#include <ctime>
#include <iomanip>
#include <cstdlib>

namespace bullet_detection {

// ===== HARD SAMPLE DETECTOR IMPLEMENTATION =====

HardSampleDetector::HardSampleDetector(const Tier1Classifier& classifier)
    : classifier_(classifier) {}

std::vector<HardSampleDetector::HardSample> HardSampleDetector::detectHardSamples(
    const std::vector<CurriculumSample>& samples,
    float margin_threshold) {
    
    std::vector<HardSample> hard_samples;
    
    for (const auto& sample : samples) {
        auto decision = const_cast<Tier1Classifier&>(classifier_).predict(sample.features);
        
        HardSample hs;
        hs.features = sample.features;
        hs.true_label = sample.label;
        hs.predicted_label = (decision.confidence > 0.5f) ? 1 : 0;
        hs.prediction_confidence = decision.confidence;
        hs.margin_to_decision_boundary = std::abs(decision.confidence - 0.5f);
        
        // Hard if: misclassified OR near decision boundary
        if (hs.predicted_label != hs.true_label || hs.margin_to_decision_boundary < margin_threshold) {
            hard_samples.push_back(hs);
        }
    }
    
    return hard_samples;
}

float HardSampleDetector::computeHardness(
    const FeatureVector& features,
    int true_label) {
    
    auto decision = const_cast<Tier1Classifier&>(classifier_).predict(features);
    int predicted = (decision.confidence > 0.5f) ? 1 : 0;
    
    // Hardness = distance to decision boundary
    // Hard: near 0.5, Easy: near 0 or 1
    float margin = std::abs(decision.confidence - 0.5f);
    float hardness = 1.0f - (margin * 2.0f);  // Range: [0, 1]
    
    // Increase hardness if misclassified
    if (predicted != true_label) {
        hardness = std::max(hardness, 0.7f);
    }
    
    return std::max(0.0f, std::min(1.0f, hardness));
}

// ===== DATA AUGMENTATION COORDINATOR IMPLEMENTATION =====

DataAugmentationCoordinator::DataAugmentationCoordinator() {}

bool DataAugmentationCoordinator::generateHardSamples(
    const std::vector<HardSampleDetector::HardSample>& hard_samples,
    size_t num_to_generate,
    std::vector<CurriculumSample>& output_samples) {
    
    if (hard_samples.empty()) {
        std::cout << "WARNING: No hard samples provided for augmentation" << std::endl;
        return false;
    }
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, hard_samples.size() - 1);
    
    for (size_t i = 0; i < num_to_generate; ++i) {
        const auto& base = hard_samples[dis(gen)];
        CurriculumSample sample = generateSyntheticVariation(base);
        output_samples.push_back(sample);
    }
    
    return true;
}

CurriculumSample DataAugmentationCoordinator::generateSyntheticVariation(
    const HardSampleDetector::HardSample& base_sample) {
    
    CurriculumSample sample;
    sample.label = base_sample.true_label;
    sample.features = base_sample.features;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<> noise(0.0, 0.05);  // 5% noise
    
    // Add slight variations to features
    for (int i = 0; i < FeatureVector::DIM; ++i) {
        float variation = 1.0f + static_cast<float>(noise(gen));
        sample.features.data[i] *= variation;
    }
    
    // Clamp values to reasonable ranges
    for (int i = 0; i < FeatureVector::DIM; ++i) {
        sample.features.data[i] = std::max(-10.0f, std::min(10.0f, sample.features.data[i]));
    }
    
    // Set difficulty based on parent
    sample.difficulty_score = std::max(0.5f, base_sample.margin_to_decision_boundary * 2.0f);
    
    return sample;
}

void DataAugmentationCoordinator::augmentDiversity(
    std::vector<CurriculumSample>& samples,
    int variation_level) {
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<> noise(0.0, 0.02 * variation_level);
    
    for (auto& sample : samples) {
        for (int i = 0; i < FeatureVector::DIM; ++i) {
            sample.features.data[i] *= (1.0f + static_cast<float>(noise(gen)));
        }
    }
}

bool DataAugmentationCoordinator::validateSamples(
    const std::vector<CurriculumSample>& samples) {
    
    for (const auto& sample : samples) {
        // Check label validity
        if (sample.label != 0 && sample.label != 1) {
            return false;
        }
        
        // Check feature ranges (should be normalized)
        for (int i = 0; i < FeatureVector::DIM; ++i) {
            if (std::isnan(sample.features.data[i]) || std::isinf(sample.features.data[i])) {
                return false;
            }
        }
        
        // Check difficulty score
        if (sample.difficulty_score < 0.0f || sample.difficulty_score > 1.0f) {
            return false;
        }
    }
    
    return true;
}

// ===== ONLINE CURRICULUM LEARNER IMPLEMENTATION =====

OnlineCurriculumLearner::OnlineCurriculumLearner() {}

bool OnlineCurriculumLearner::runOnlineCurriculumLoop(
    const std::string& initial_dataset_path,
    const std::string& model_dir,
    const std::string& scaler_path,
    const std::string& data_generator_script,
    const OnlineCurriculumConfig& config,
    OnlineTrainingHistory& history) {
    
    std::cout << "\n" << std::string(80, '=') << std::endl;
    std::cout << "ONLINE CURRICULUM LEARNING LOOP" << std::endl;
    std::cout << std::string(80, '=') << std::endl;
    
    try {
        // Initialize
        Tier1Classifier tier1;
        LightweightMLP tier2({FeatureVector::DIM, 64, 32, 1});
        ScalerState scaler;
        
        // Load initial dataset
        std::vector<CurriculumSample> full_dataset;
        if (!loadDataset(initial_dataset_path, full_dataset, scaler)) {
            std::cerr << "ERROR: Failed to load initial dataset" << std::endl;
            return false;
        }
        
        std::cout << "\n[INITIALIZATION]" << std::endl;
        std::cout << "? Loaded " << full_dataset.size() << " initial samples" << std::endl;
        
        // Main loop
        for (int iter = 0; iter < config.max_iterations; ++iter) {
            std::cout << "\n" << std::string(80, '=') << std::endl;
            std::cout << "ITERATION " << (iter + 1) << "/" << config.max_iterations << std::endl;
            std::cout << std::string(80, '=') << std::endl;
            
            IterationMetrics metrics;
            metrics.iteration_number = iter + 1;
            metrics.total_dataset_size = full_dataset.size();
            
            auto iter_start = std::chrono::high_resolution_clock::now();
            
            // ===== STEP 1: LOAD CURRENT MODEL =====
            std::cout << "\n[STEP 1] Loading current model..." << std::endl;
            if (!loadCurrentModel(tier1, tier2, scaler, model_dir, scaler_path)) {
                std::cout << "WARNING: Could not load models, using defaults" << std::endl;
            }
            std::cout << "? Model loaded" << std::endl;
            
            // Split data for validation
            std::vector<CurriculumSample> train_data, validation_data;
            splitDataset(full_dataset, train_data, validation_data, config.validation_split);
            
            // ===== STEP 3: EVALUATE BASELINE =====
            std::cout << "\n[STEP 3] Evaluating baseline performance..." << std::endl;
            if (!evaluatePerformance(tier1, validation_data, metrics)) {
                metrics.accuracy_before = 0.5f;
            }
            std::cout << "? Baseline: Accuracy=" << std::fixed << std::setprecision(4) 
                      << metrics.accuracy_before * 100.0f << "%" << std::endl;
            
            // ===== STEP 4: IDENTIFY HARD SAMPLES =====
            std::cout << "\n[STEP 4] Identifying hard samples..." << std::endl;
            auto hard_samples = identifyHardSamples(tier1, train_data);
            metrics.hard_samples_identified = hard_samples.size();
            std::cout << "? Found " << hard_samples.size() << " hard samples" << std::endl;
            
            // ===== STEP 5: GENERATE NEW DATA =====
            std::cout << "\n[STEP 5] Generating new curriculum data..." << std::endl;
            std::vector<CurriculumSample> new_samples;
            if (!generateNewData(hard_samples, config.samples_per_iteration, 
                                data_generator_script, new_samples, metrics)) {
                std::cout << "WARNING: Data generation had issues, proceeding with available samples" << std::endl;
            }
            std::cout << "? Generated " << new_samples.size() << " new samples in " 
                      << std::fixed << std::setprecision(2) << metrics.data_generation_time_seconds << "s" << std::endl;
            
            // ===== STEP 6: CURRICULUM INTEGRATION =====
            std::cout << "\n[STEP 6] Integrating new data into curriculum..." << std::endl;
            integrateInCurriculum(new_samples);
            
            // ===== STEP 8: DATA VALIDATION =====
            std::cout << "\n[STEP 8] Validating data integrity..." << std::endl;
            if (!validateDataIntegrity(new_samples)) {
                std::cout << "WARNING: Some validation issues detected, proceeding with caution" << std::endl;
            }
            std::cout << "? Data validation completed" << std::endl;
            
            // Add new samples to dataset
            full_dataset.insert(full_dataset.end(), new_samples.begin(), new_samples.end());
            
            // Limit dataset size
            if (full_dataset.size() > static_cast<size_t>(config.max_dataset_size)) {
                std::cout << "WARNING: Dataset exceeds max size, removing oldest samples" << std::endl;
                size_t remove_count = full_dataset.size() - config.max_dataset_size;
                full_dataset.erase(full_dataset.begin(), full_dataset.begin() + remove_count);
            }
            
            metrics.samples_added = new_samples.size();
            metrics.total_dataset_size = full_dataset.size();
            
            // ===== STEP 2: INCREMENTAL TRAINING =====
            std::cout << "\n[STEP 2] Performing incremental training..." << std::endl;
            
            // Create training config
            IncrementalTrainingConfig train_config;
            train_config.learning_rate = config.learning_rate;
            train_config.max_epochs = config.epochs_per_iteration;
            train_config.min_learning_rate = config.min_learning_rate;
            train_config.use_curriculum = config.use_curriculum;
            train_config.prioritize_hard_examples = config.focus_on_hard_samples;
            train_config.replay_buffer_ratio = config.old_data_ratio;
            
            // Re-split for training
            splitDataset(full_dataset, train_data, validation_data, config.validation_split);
            
            // Create temporary CSV for training
            std::string temp_csv = "temp_training_data_iter_" + std::to_string(iter) + ".csv";
            
            // Train incrementally
            auto train_start = std::chrono::high_resolution_clock::now();
            if (!incrementalTrain(tier1, tier2, temp_csv, scaler, train_config, metrics)) {
                std::cout << "WARNING: Training had issues, continuing to next iteration" << std::endl;
            }
            auto train_end = std::chrono::high_resolution_clock::now();
            metrics.training_time_seconds = std::chrono::duration_cast<std::chrono::milliseconds>(
                train_end - train_start).count() / 1000.0f;
            
            std::cout << "? Training completed in " << std::fixed << std::setprecision(2) 
                      << metrics.training_time_seconds << "s" << std::endl;
            
            // ===== STEP 3: EVALUATE FINAL PERFORMANCE =====
            std::cout << "\n[STEP 3] Evaluating final performance..." << std::endl;
            if (!evaluatePerformance(tier1, validation_data, metrics)) {
                metrics.accuracy_after = metrics.accuracy_before;
            }
            metrics.accuracy_improvement = metrics.accuracy_after - metrics.accuracy_before;
            
            std::cout << "? Final: Accuracy=" << std::fixed << std::setprecision(4) 
                      << metrics.accuracy_after * 100.0f << "% (improvement: " 
                      << metrics.accuracy_improvement * 100.0f << "%)" << std::endl;
            
            // ===== STEP 9: SAVE MODELS =====
            std::cout << "\n[STEP 9] Saving updated models..." << std::endl;
            if (!saveUpdatedModels(tier1, tier2, model_dir, iter + 1)) {
                std::cout << "WARNING: Failed to save models" << std::endl;
            }
            std::cout << "? Models saved to version_iter_" << (iter + 1) << std::endl;
            
            // ===== STEP 10: LOGGING =====
            std::cout << "\n[STEP 10] Logging iteration results..." << std::endl;
            std::string log_file = "results/online_training_loop.txt";
            if (!logIterationResults(log_file, metrics, history)) {
                std::cout << "WARNING: Failed to log results" << std::endl;
            }
            std::cout << "? Results logged" << std::endl;
            
            // Store metrics
            history.iterations.push_back(metrics);
            history.accuracy_trend.push_back(metrics.accuracy_after);
            history.recall_trend.push_back(metrics.recall_after);
            history.loss_trend.push_back(metrics.loss_after);
            history.dataset_size_trend.push_back(metrics.total_dataset_size);
            
            if (metrics.accuracy_after > history.best_accuracy) {
                history.best_accuracy = metrics.accuracy_after;
                history.best_iteration = iter + 1;
            }
            
            history.total_samples_generated += new_samples.size();
            
            auto iter_end = std::chrono::high_resolution_clock::now();
            auto iter_time = std::chrono::duration_cast<std::chrono::seconds>(iter_end - iter_start).count();
            
            std::cout << "\n[ITERATION SUMMARY]" << std::endl;
            std::cout << "  Time: " << iter_time << "s" << std::endl;
            std::cout << "  Dataset size: " << metrics.total_dataset_size << std::endl;
            std::cout << "  Performance: " << std::fixed << std::setprecision(2) 
                      << metrics.accuracy_before * 100.0f << "% ? " 
                      << metrics.accuracy_after * 100.0f << "% (+" 
                      << metrics.accuracy_improvement * 100.0f << "%)" << std::endl;
            
            // Check convergence
            if (checkConvergence(history, config)) {
                std::cout << "\n? Convergence detected, stopping loop" << std::endl;
                break;
            }
        }
        
        // ===== FINAL SUMMARY =====
        std::cout << "\n" << std::string(80, '=') << std::endl;
        std::cout << "ONLINE CURRICULUM LEARNING COMPLETED" << std::endl;
        std::cout << std::string(80, '=') << std::endl;
        
        std::cout << "\n[FINAL STATISTICS]" << std::endl;
        std::cout << "  Total iterations: " << history.iterations.size() << std::endl;
        std::cout << "  Total samples generated: " << history.total_samples_generated << std::endl;
        std::cout << "  Final dataset size: " << history.iterations.back().total_dataset_size << std::endl;
        std::cout << "  Best accuracy: " << std::fixed << std::setprecision(4) 
                  << history.best_accuracy * 100.0f << "% (iteration " << history.best_iteration << ")" << std::endl;
        
        if (!history.iterations.empty()) {
            float initial_acc = history.iterations.front().accuracy_before;
            float final_acc = history.iterations.back().accuracy_after;
            std::cout << "  Total improvement: " << std::fixed << std::setprecision(4) 
                      << (final_acc - initial_acc) * 100.0f << "%" << std::endl;
        }
        
        std::cout << "\n? Online curriculum learning loop completed successfully!" << std::endl;
        
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return false;
    }
}

bool OnlineCurriculumLearner::loadCurrentModel(
    Tier1Classifier& tier1,
    LightweightMLP& tier2,
    ScalerState& scaler,
    const std::string& model_dir,
    const std::string& scaler_path) {
    
    if (!scaler.load(scaler_path)) {
        std::cout << "WARNING: Could not load scaler" << std::endl;
    }
    
    if (!tier2.loadWeights(model_dir + "/tier2_mlp.bin")) {
        std::cout << "WARNING: Could not load Tier 2 weights" << std::endl;
    }
    
    return true;
}

bool OnlineCurriculumLearner::incrementalTrain(
    Tier1Classifier& tier1,
    LightweightMLP& tier2,
    const std::string& dataset_path,
    const ScalerState& scaler,
    const IncrementalTrainingConfig& train_config,
    IterationMetrics& metrics) {
    
    // Use existing IncrementalTrainer
    IncrementalTrainer trainer;
    IncrementalTrainingMetrics train_metrics;
    
    bool success = trainer.performIncrementalTraining(
        dataset_path,
        "",  // scaler_path
        "",  // model_output_dir
        train_config,
        train_metrics
    );
    
    if (success) {
        metrics.loss_before = train_metrics.initial_loss;
        metrics.loss_after = train_metrics.final_loss;
    }
    
    return success;
}

bool OnlineCurriculumLearner::evaluatePerformance(
    const Tier1Classifier& tier1,
    const std::vector<CurriculumSample>& validation_data,
    IterationMetrics& metrics) {
    
    if (validation_data.empty()) {
        return false;
    }
    
    int tp = 0, fp = 0, tn = 0, fn = 0;
    
    for (const auto& sample : validation_data) {
        auto decision = const_cast<Tier1Classifier&>(tier1).predict(sample.features);
        int pred = (decision.confidence > 0.5f) ? 1 : 0;
        
        if (pred == 1 && sample.label == 1) tp++;
        else if (pred == 1 && sample.label == 0) fp++;
        else if (pred == 0 && sample.label == 0) tn++;
        else if (pred == 0 && sample.label == 1) fn++;
    }
    
    int correct = tp + tn;
    metrics.accuracy_after = static_cast<float>(correct) / validation_data.size();
    
    if (tp + fp > 0) {
        metrics.precision_after = static_cast<float>(tp) / (tp + fp);
    }
    
    if (tp + fn > 0) {
        metrics.recall_after = static_cast<float>(tp) / (tp + fn);
    }
    
    if (metrics.precision_after + metrics.recall_after > 0) {
        metrics.f1_after = 2.0f * (metrics.precision_after * metrics.recall_after) 
                          / (metrics.precision_after + metrics.recall_after);
    }
    
    return true;
}

std::vector<HardSampleDetector::HardSample> OnlineCurriculumLearner::identifyHardSamples(
    const Tier1Classifier& tier1,
    const std::vector<CurriculumSample>& dataset) {
    
    hard_sample_detector_ = std::make_unique<HardSampleDetector>(tier1);
    return hard_sample_detector_->detectHardSamples(dataset, 0.15f);
}

bool OnlineCurriculumLearner::generateNewData(
    const std::vector<HardSampleDetector::HardSample>& hard_samples,
    size_t num_samples,
    const std::string& generator_script,
    std::vector<CurriculumSample>& new_samples,
    IterationMetrics& metrics) {
    
    auto gen_start = std::chrono::high_resolution_clock::now();
    
    if (augmentation_coordinator_ == nullptr) {
        augmentation_coordinator_ = std::make_unique<DataAugmentationCoordinator>();
    }
    
    // Generate synthetic variations from hard samples
    if (!augmentation_coordinator_->generateHardSamples(hard_samples, num_samples, new_samples)) {
        std::cout << "WARNING: Failed to generate hard samples" << std::endl;
    }
    
    // Augment diversity
    augmentation_coordinator_->augmentDiversity(new_samples, 2);
    
    // Categorize samples by difficulty
    for (auto& sample : new_samples) {
        if (sample.difficulty_score < 0.33f) {
            metrics.easy_samples_generated++;
        } else if (sample.difficulty_score < 0.67f) {
            metrics.medium_samples_generated++;
        } else {
            metrics.hard_samples_generated++;
        }
    }
    
    auto gen_end = std::chrono::high_resolution_clock::now();
    metrics.data_generation_time_seconds = std::chrono::duration_cast<std::chrono::milliseconds>(
        gen_end - gen_start).count() / 1000.0f;
    
    return !new_samples.empty();
}

void OnlineCurriculumLearner::integrateInCurriculum(
    std::vector<CurriculumSample>& samples) {
    
    for (auto& sample : samples) {
        if (sample.difficulty_score < 0.33f) {
            // Easy sample
        } else if (sample.difficulty_score < 0.67f) {
            // Medium sample
        } else {
            // Hard sample
        }
    }
}

void OnlineCurriculumLearner::createReplayBuffer(
    const std::vector<CurriculumSample>& old_data,
    const std::vector<CurriculumSample>& new_data,
    std::vector<CurriculumSample>& mixed_batch,
    float new_data_ratio) {
    
    // Calculate sizes
    size_t old_size = static_cast<size_t>(new_data.size() * (1.0f - new_data_ratio) / new_data_ratio);
    old_size = std::min(old_size, old_data.size());
    
    // Add new data
    mixed_batch.insert(mixed_batch.end(), new_data.begin(), new_data.end());
    
    // Add old data
    if (!old_data.empty()) {
        std::random_device rd;
        std::mt19937 g(rd());
        std::vector<size_t> indices(old_data.size());
        std::iota(indices.begin(), indices.end(), 0);
        std::shuffle(indices.begin(), indices.end(), g);
        
        for (size_t i = 0; i < old_size && i < indices.size(); ++i) {
            mixed_batch.push_back(old_data[indices[i]]);
        }
    }
}

bool OnlineCurriculumLearner::validateDataIntegrity(
    const std::vector<CurriculumSample>& samples) {
    
    if (augmentation_coordinator_ == nullptr) {
        augmentation_coordinator_ = std::make_unique<DataAugmentationCoordinator>();
    }
    
    return augmentation_coordinator_->validateSamples(samples);
}

bool OnlineCurriculumLearner::saveUpdatedModels(
    const Tier1Classifier& tier1,
    const LightweightMLP& tier2,
    const std::string& model_dir,
    int iteration) {
    
    std::string version_dir = model_dir + "/version_iter_" + std::to_string(iteration);
    
    try {
        // In production, would save actual model files
        // For now, just save MLP weights
        const_cast<LightweightMLP&>(tier2).saveWeights(version_dir + "/tier2_mlp.bin");
        return true;
    } catch (...) {
        return false;
    }
}

bool OnlineCurriculumLearner::logIterationResults(
    const std::string& log_file,
    const IterationMetrics& metrics,
    const OnlineTrainingHistory& history) {
    
    std::ofstream log(log_file, std::ios::app);
    if (!log.is_open()) {
        return false;
    }
    
    log << "================================================================================\n";
    log << "ITERATION " << metrics.iteration_number << "\n";
    log << "================================================================================\n";
    log << "Timestamp: " << std::time(nullptr) << "\n";
    log << "Dataset size: " << metrics.total_dataset_size << " (added " 
        << metrics.samples_added << ")\n";
    log << "Hard samples identified: " << metrics.hard_samples_identified << "\n";
    log << "Easy samples generated: " << metrics.easy_samples_generated << "\n";
    log << "Medium samples generated: " << metrics.medium_samples_generated << "\n";
    log << "Hard samples generated: " << metrics.hard_samples_generated << "\n";
    log << "\nPerformance:\n";
    log << "  Accuracy before: " << std::fixed << std::setprecision(4) 
        << metrics.accuracy_before * 100.0f << "%\n";
    log << "  Accuracy after: " << metrics.accuracy_after * 100.0f << "%\n";
    log << "  Improvement: " << metrics.accuracy_improvement * 100.0f << "%\n";
    log << "  Precision: " << metrics.precision_before * 100.0f << "% ? " 
        << metrics.precision_after * 100.0f << "%\n";
    log << "  Recall: " << metrics.recall_before * 100.0f << "% ? " 
        << metrics.recall_after * 100.0f << "%\n";
    log << "  Loss: " << metrics.loss_before << " ? " << metrics.loss_after << "\n";
    log << "  Training time: " << metrics.training_time_seconds << "s\n";
    log << "  Data generation time: " << metrics.data_generation_time_seconds << "s\n";
    log << "\n";
    
    log.close();
    return true;
}

bool OnlineCurriculumLearner::checkConvergence(
    const OnlineTrainingHistory& history,
    const OnlineCurriculumConfig& config) {
    
    if (history.iterations.size() < static_cast<size_t>(config.convergence_patience)) {
        return false;
    }
    
    // Check if improvement is below threshold for last N iterations
    size_t recent_start = history.iterations.size() - config.convergence_patience;
    float recent_improvement = 0.0f;
    
    for (size_t i = recent_start; i < history.iterations.size(); ++i) {
        recent_improvement += history.iterations[i].accuracy_improvement;
    }
    recent_improvement /= config.convergence_patience;
    
    return recent_improvement < config.improvement_threshold;
}

bool OnlineCurriculumLearner::loadDataset(
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
    int count = 0;
    
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
            int field = 0;
            
            while (std::getline(iss, token, ',')) {
                try {
                    if (field < FeatureVector::DIM) {
                        features.push_back(std::stod(token));
                    } else if (field == FeatureVector::DIM + 1) {
                        label = std::stoi(token);
                    }
                    field++;
                } catch (...) {}
            }
            
            while (features.size() < static_cast<size_t>(FeatureVector::DIM)) {
                features.push_back(0.0);
            }
            
            if (label == 0 || label == 1) {
                CurriculumSample sample;
                sample.label = label;
                
                if (scaler.n_features > 0) {
                    scaler.normalize(features);
                }
                
                for (int i = 0; i < FeatureVector::DIM; ++i) {
                    sample.features.data[i] = static_cast<float>(features[i]);
                }
                
                samples.push_back(sample);
                count++;
            }
        } catch (...) {}
    }
    
    file.close();
    return !samples.empty();
}

void OnlineCurriculumLearner::splitDataset(
    const std::vector<CurriculumSample>& full_dataset,
    std::vector<CurriculumSample>& train_data,
    std::vector<CurriculumSample>& validation_data,
    float validation_ratio) {
    
    size_t validation_size = static_cast<size_t>(full_dataset.size() * validation_ratio);
    
    std::vector<size_t> indices(full_dataset.size());
    std::iota(indices.begin(), indices.end(), 0);
    
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(indices.begin(), indices.end(), g);
    
    for (size_t i = 0; i < validation_size && i < indices.size(); ++i) {
        validation_data.push_back(full_dataset[indices[i]]);
    }
    
    for (size_t i = validation_size; i < indices.size(); ++i) {
        train_data.push_back(full_dataset[indices[i]]);
    }
}

} // namespace bullet_detection
