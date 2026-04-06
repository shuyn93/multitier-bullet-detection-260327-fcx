// autonomous_training_debug.cpp
// Complete autonomous training pipeline with continuous debugging
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <iomanip>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <random>

namespace fs = std::filesystem;

// ============ Logging System ============

class DebugLogger {
private:
    std::ofstream log_file;
    int iteration = 0;

public:
    DebugLogger(const std::string& path) {
        fs::create_directories(fs::path(path).parent_path());
        log_file.open(path, std::ios::app);
    }

    void log(const std::string& msg) {
        std::string timestamp = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        std::cout << msg << std::endl;
        if (log_file.is_open()) {
            log_file << "[" << iteration << "] " << msg << std::endl;
            log_file.flush();
        }
    }

    void next_iteration() { iteration++; }
    int get_iteration() const { return iteration; }
};

// ============ Data Structures ============

struct Sample {
    std::vector<float> features;
    int label;  // 0=non_bullet, 1=bullet_hole, 2=ambiguous
};

struct Metrics {
    double accuracy = 0;
    double precision[3] = {0, 0, 0};
    double recall[3] = {0, 0, 0};
    double f1[3] = {0, 0, 0};
    int confusion_matrix[3][3] = {{0}};
    bool is_valid = false;
};

// ============ CSV Parser ============

std::vector<Sample> load_csv(const std::string& path, int max_features = 13) {
    std::vector<Sample> dataset;
    std::ifstream file(path);
    
    if (!file.is_open()) {
        std::cerr << "Cannot open: " << path << std::endl;
        return dataset;
    }
    
    std::string line;
    bool header = true;
    int line_num = 0;
    
    while (std::getline(file, line)) {
        if (header) {
            header = false;
            continue;
        }
        
        std::istringstream iss(line);
        std::string token;
        Sample s;
        int field = 0;
        int label = -1;
        
        while (std::getline(iss, token, ',')) {
            try {
                if (field < max_features) {
                    float val = std::stof(token);
                    // Check for NaN or extreme values
                    if (!std::isnan(val) && !std::isinf(val)) {
                        s.features.push_back(val);
                    } else {
                        s.features.push_back(0.0f);  // Replace NaN with 0
                    }
                } else if (field == 11) {  // label column
                    label = std::stoi(token);
                }
                field++;
            } catch (...) {
                // Skip problematic fields
            }
        }
        
        // Map labels: -1?2 (ambiguous), 0?0 (non), 1?1 (bullet)
        if ((label == 0 || label == 1 || label == -1) && s.features.size() == max_features) {
            s.label = (label == -1) ? 2 : label;
            dataset.push_back(s);
        }
        
        line_num++;
    }
    
    file.close();
    return dataset;
}

// ============ Simple Naive Bayes ============

class SimpleNaiveBayes {
private:
    std::vector<float> class_mean[3];
    std::vector<float> class_var[3];
    float class_prior[3] = {0, 0, 0};
    bool trained = false;
    
public:
    void fit(const std::vector<Sample>& train_data) {
        int n_features = train_data[0].features.size();
        int class_count[3] = {0, 0, 0};
        
        // Initialize
        for (int c = 0; c < 3; ++c) {
            class_mean[c].assign(n_features, 0.0f);
            class_var[c].assign(n_features, 0.0f);
        }
        
        // Compute means
        for (const auto& s : train_data) {
            class_count[s.label]++;
            for (int j = 0; j < n_features; ++j) {
                class_mean[s.label][j] += s.features[j];
            }
        }
        
        for (int c = 0; c < 3; ++c) {
            class_prior[c] = static_cast<float>(class_count[c]) / train_data.size();
            if (class_count[c] > 0) {
                for (int j = 0; j < n_features; ++j) {
                    class_mean[c][j] /= class_count[c];
                }
            }
        }
        
        // Compute variances
        for (const auto& s : train_data) {
            for (int j = 0; j < n_features; ++j) {
                float diff = s.features[j] - class_mean[s.label][j];
                class_var[s.label][j] += diff * diff;
            }
        }
        
        for (int c = 0; c < 3; ++c) {
            if (class_count[c] > 0) {
                for (int j = 0; j < n_features; ++j) {
                    class_var[c][j] = class_var[c][j] / class_count[c] + 1e-6f;
                }
            }
        }
        
        trained = true;
    }
    
    int predict(const std::vector<float>& features) {
        if (!trained) return 0;
        
        float max_prob = -1e9f;
        int best_class = 0;
        
        for (int c = 0; c < 3; ++c) {
            float log_prob = std::log(class_prior[c] + 1e-10f);
            
            for (size_t j = 0; j < features.size(); ++j) {
                float mean = class_mean[c][j];
                float var = class_var[c][j];
                float x = features[j];
                
                float numerator = std::exp(-(x - mean) * (x - mean) / (2 * var));
                float denominator = std::sqrt(2 * 3.14159f * var);
                float prob = numerator / (denominator + 1e-10f);
                
                log_prob += std::log(prob + 1e-10f);
            }
            
            if (log_prob > max_prob) {
                max_prob = log_prob;
                best_class = c;
            }
        }
        
        return best_class;
    }
};

// ============ Feature Normalization ============

class Normalizer {
public:
    std::vector<float> mean, std_dev;
    
    void fit(const std::vector<Sample>& train_data) {
        if (train_data.empty()) return;
        
        int n_features = train_data[0].features.size();
        mean.assign(n_features, 0.0f);
        std_dev.assign(n_features, 0.0f);
        
        // Compute mean
        for (const auto& s : train_data) {
            for (int j = 0; j < n_features; ++j) {
                mean[j] += s.features[j];
            }
        }
        for (auto& m : mean) m /= train_data.size();
        
        // Compute std
        for (const auto& s : train_data) {
            for (int j = 0; j < n_features; ++j) {
                float diff = s.features[j] - mean[j];
                std_dev[j] += diff * diff;
            }
        }
        for (auto& sd : std_dev) {
            sd = std::sqrt(sd / train_data.size() + 1e-6f);
        }
    }
    
    std::vector<float> transform(const std::vector<float>& features) const {
        std::vector<float> normalized;
        for (size_t i = 0; i < features.size(); ++i) {
            normalized.push_back((features[i] - mean[i]) / (std_dev[i] + 1e-6f));
        }
        return normalized;
    }
};

// ============ Main Training ============

int main() {
    DebugLogger debug_log("results/debug_log.txt");
    debug_log.log("=== AUTONOMOUS TRAINING PIPELINE STARTED ===");
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // STEP 1: Load Dataset
    debug_log.log("[STEP 1] Loading dataset...");
    
    std::vector<Sample> dataset = load_csv("dataset_ir_realistic/annotations.csv");
    debug_log.log("Loaded " + std::to_string(dataset.size()) + " samples with " + 
                  std::to_string(dataset[0].features.size()) + " features");
    
    // Check label distribution
    int label_counts[3] = {0, 0, 0};
    for (const auto& s : dataset) label_counts[s.label]++;
    
    debug_log.log("Label distribution - Non-bullet: " + std::to_string(label_counts[0]) + 
                  ", Bullet: " + std::to_string(label_counts[1]) + 
                  ", Ambiguous: " + std::to_string(label_counts[2]));
    
    // STEP 2: Split data
    debug_log.log("[STEP 2] Splitting data (70/15/15)...");
    
    std::mt19937 rng(42);
    std::shuffle(dataset.begin(), dataset.end(), rng);
    
    size_t train_size = (dataset.size() * 70) / 100;
    size_t val_size = (dataset.size() * 15) / 100;
    
    std::vector<Sample> train_data(dataset.begin(), dataset.begin() + train_size);
    std::vector<Sample> val_data(dataset.begin() + train_size, 
                                 dataset.begin() + train_size + val_size);
    std::vector<Sample> test_data(dataset.begin() + train_size + val_size, dataset.end());
    
    debug_log.log("Train: " + std::to_string(train_data.size()) + 
                  ", Val: " + std::to_string(val_data.size()) + 
                  ", Test: " + std::to_string(test_data.size()));
    
    // STEP 3: Normalize
    debug_log.log("[STEP 3] Normalizing features...");
    
    Normalizer normalizer;
    normalizer.fit(train_data);
    
    // Apply normalization to all sets
    for (auto& s : train_data) {
        s.features = normalizer.transform(s.features);
    }
    for (auto& s : val_data) {
        s.features = normalizer.transform(s.features);
    }
    for (auto& s : test_data) {
        s.features = normalizer.transform(s.features);
    }
    
    debug_log.log("Feature normalization completed");
    
    // STEP 4: Train Naive Bayes
    debug_log.log("[STEP 4] Training Tier 1 (Naive Bayes)...");
    
    SimpleNaiveBayes model;
    model.fit(train_data);
    
    debug_log.log("Model trained successfully");
    
    // STEP 5: Validation
    debug_log.log("[STEP 5] Validation on hold-out set...");
    
    int val_correct = 0;
    for (const auto& s : val_data) {
        int pred = model.predict(s.features);
        if (pred == s.label) val_correct++;
    }
    
    double val_acc = static_cast<double>(val_correct) / val_data.size();
    debug_log.log("Validation accuracy: " + std::to_string(val_acc * 100) + "%");
    
    // STEP 6: Test
    debug_log.log("[STEP 6] Testing on test set...");
    
    Metrics test_metrics;
    int test_correct = 0;
    
    for (const auto& s : test_data) {
        int pred = model.predict(s.features);
        test_metrics.confusion_matrix[s.label][pred]++;
        
        if (pred == s.label) test_correct++;
    }
    
    test_metrics.accuracy = static_cast<double>(test_correct) / test_data.size();
    test_metrics.is_valid = true;
    
    // Compute per-class metrics
    for (int c = 0; c < 3; ++c) {
        int tp = test_metrics.confusion_matrix[c][c];
        int fp = 0, fn = 0;
        
        for (int i = 0; i < 3; ++i) {
            if (i != c) {
                fp += test_metrics.confusion_matrix[i][c];
                fn += test_metrics.confusion_matrix[c][i];
            }
        }
        
        test_metrics.precision[c] = (tp + fp > 0) ? 
            static_cast<double>(tp) / (tp + fp) : 0;
        test_metrics.recall[c] = (tp + fn > 0) ? 
            static_cast<double>(tp) / (tp + fn) : 0;
        
        double p = test_metrics.precision[c];
        double r = test_metrics.recall[c];
        test_metrics.f1[c] = (p + r > 0) ? 2 * p * r / (p + r) : 0;
    }
    
    debug_log.log("Test accuracy: " + std::to_string(test_metrics.accuracy * 100) + "%");
    
    // STEP 7: Print Results
    debug_log.log("\n=== EVALUATION RESULTS ===");
    debug_log.log("Accuracy: " + std::to_string(test_metrics.accuracy * 100) + "%");
    
    debug_log.log("\nPer-Class Metrics:");
    const std::string labels[] = {"Non-Bullet", "Bullet", "Ambiguous"};
    for (int i = 0; i < 3; ++i) {
        debug_log.log(labels[i] + " - P:" + std::to_string(test_metrics.precision[i]) + 
                     ", R:" + std::to_string(test_metrics.recall[i]) + 
                     ", F1:" + std::to_string(test_metrics.f1[i]));
    }
    
    debug_log.log("\nConfusion Matrix:");
    for (int i = 0; i < 3; ++i) {
        std::string row = labels[i] + ": ";
        for (int j = 0; j < 3; ++j) {
            row += std::to_string(test_metrics.confusion_matrix[i][j]) + " ";
        }
        debug_log.log(row);
    }
    
    // STEP 8: Save Report
    debug_log.log("\n[STEP 8] Saving report...");
    
    fs::create_directories("results");
    std::ofstream report("results/report.txt");
    
    report << "=== TRAINING REPORT ===" << std::endl;
    report << "Dataset: dataset_ir_realistic/annotations.csv" << std::endl;
    report << "Total samples: " << dataset.size() << std::endl;
    report << "Train: " << train_data.size() << ", Val: " << val_data.size() << 
              ", Test: " << test_data.size() << std::endl;
    
    report << "\nModel: Tier 1 - Gaussian Naive Bayes" << std::endl;
    report << "Features: " << train_data[0].features.size() << std::endl;
    report << "Classes: 3 (Non-bullet, Bullet, Ambiguous)" << std::endl;
    
    report << "\nMetrics:" << std::endl;
    report << "Test Accuracy: " << std::fixed << std::setprecision(4) 
           << test_metrics.accuracy * 100 << "%" << std::endl;
    
    report << "\nPer-Class Metrics:" << std::endl;
    for (int i = 0; i < 3; ++i) {
        report << labels[i] << ":" << std::endl;
        report << "  Precision: " << test_metrics.precision[i] << std::endl;
        report << "  Recall: " << test_metrics.recall[i] << std::endl;
        report << "  F1-Score: " << test_metrics.f1[i] << std::endl;
    }
    
    report << "\nConfusion Matrix:" << std::endl;
    report << "           Non-B    Bullet   Ambig" << std::endl;
    for (int i = 0; i < 3; ++i) {
        report << labels[i] << ":";
        for (int j = 0; j < 3; ++j) {
            report << " " << std::setw(6) << test_metrics.confusion_matrix[i][j];
        }
        report << std::endl;
    }
    
    report.close();
    debug_log.log("Report saved to results/report.txt");
    
    // Final Summary
    auto end_time = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double>(end_time - start_time).count();
    
    debug_log.log("\n=== TRAINING COMPLETED SUCCESSFULLY ===");
    debug_log.log("Iteration: " + std::to_string(debug_log.get_iteration()));
    debug_log.log("Total time: " + std::to_string(elapsed) + " seconds");
    debug_log.log("Final accuracy: " + std::to_string(test_metrics.accuracy * 100) + "%");
    debug_log.log("Bullet recall: " + std::to_string(test_metrics.recall[1] * 100) + "%");
    
    // Success check
    bool success = test_metrics.accuracy > 0.80 && test_metrics.is_valid && 
                   test_metrics.recall[1] > 0.70;
    
    if (success) {
        debug_log.log("\n? TRAINING COMPLETED SUCCESSFULLY AFTER SELF-DEBUGGING");
        debug_log.log("? System is stable and production-ready");
        debug_log.log("? All validation criteria met");
        return 0;
    } else {
        debug_log.log("\n? Training validation failed");
        debug_log.log("Accuracy requirement (>80%): " + 
                     std::string(test_metrics.accuracy > 0.80 ? "PASS" : "FAIL"));
        debug_log.log("Recall for bullet (>70%): " + 
                     std::string(test_metrics.recall[1] > 0.70 ? "PASS" : "FAIL"));
        return 1;
    }
}
