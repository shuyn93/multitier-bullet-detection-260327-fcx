// training_execution.cpp - Autonomous Training Pipeline Execution
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <iomanip>
#include <cmath>
#include <string>
#include <chrono>
#include <random>
#include <cstdlib>

#include "core/ErrorHandler.h"

using namespace bullet_detection;

// ============ Data Structures ============

struct Sample {
    std::vector<double> features;
    int label;
};

struct Dataset {
    std::vector<Sample> samples;
    std::vector<double> feature_mean;
    std::vector<double> feature_std;
    
    size_t size() const { return samples.size(); }
};

struct Metrics {
    double accuracy = 0;
    double precision[3] = {0, 0, 0};
    double recall[3] = {0, 0, 0};
    double f1[3] = {0, 0, 0};
    int confusion_matrix[3][3] = {{0}};
    double avg_latency_ms = 0;
};

// ============ Data Loading ============

Dataset load_dataset(const std::string& csv_path, int n_features = 10) {
    Dataset dataset;
    
    std::ifstream file(csv_path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open CSV file: " + csv_path);
    }
    
    std::string line;
    bool header = true;
    int line_count = 0;
    
    while (std::getline(file, line)) {
        if (header) {
            header = false;
            continue;
        }
        
        std::istringstream iss(line);
        std::string token;
        Sample sample;
        int field_count = 0;
        int label = -1;
        
        while (std::getline(iss, token, ',')) {
            try {
                if (field_count < n_features) {
                    sample.features.push_back(std::stod(token));
                } else if (field_count == 11) {  // Label column
                    label = std::stoi(token);
                }
                field_count++;
            } catch (...) {
                // Skip problematic fields
            }
        }
        
        if ((label == 0 || label == 1 || label == -1) && sample.features.size() == n_features) {
            sample.label = (label == -1) ? 2 : label;
            dataset.samples.push_back(sample);
            line_count++;
        }
    }
    
    file.close();
    
    std::cout << "[DATA] Loaded " << dataset.samples.size() << " samples with " 
              << n_features << " features" << std::endl;
    
    return dataset;
}

// ============ Data Processing ============

void compute_normalization(Dataset& dataset, size_t train_size) {
    int n_features = dataset.samples[0].features.size();
    dataset.feature_mean.resize(n_features, 0);
    dataset.feature_std.resize(n_features, 0);
    
    // Compute mean from training set
    for (int j = 0; j < n_features; ++j) {
        for (size_t i = 0; i < train_size; ++i) {
            dataset.feature_mean[j] += dataset.samples[i].features[j];
        }
        dataset.feature_mean[j] /= train_size;
    }
    
    // Compute std from training set
    for (int j = 0; j < n_features; ++j) {
        for (size_t i = 0; i < train_size; ++i) {
            double diff = dataset.samples[i].features[j] - dataset.feature_mean[j];
            dataset.feature_std[j] += diff * diff;
        }
        dataset.feature_std[j] = std::sqrt(dataset.feature_std[j] / train_size + 1e-8);
    }
}

std::vector<double> normalize_features(const std::vector<double>& features,
                                       const std::vector<double>& mean,
                                       const std::vector<double>& std) {
    std::vector<double> normalized;
    for (size_t i = 0; i < features.size(); ++i) {
        normalized.push_back((features[i] - mean[i]) / (std[i] + 1e-8));
    }
    return normalized;
}

// ============ Simple Classifiers (Baseline) ============

class SimpleClassifier {
public:
    virtual ~SimpleClassifier() = default;
    virtual void fit(const std::vector<Sample>& train_set,
                    const std::vector<double>& feature_mean,
                    const std::vector<double>& feature_std) = 0;
    virtual int predict(const std::vector<double>& features) = 0;
    virtual std::vector<double> predict_proba(const std::vector<double>& features) = 0;
};

// Naive Bayes (Simple Gaussian)
class NaiveBayesClassifier : public SimpleClassifier {
private:
    std::vector<std::vector<double>> class_mean;
    std::vector<std::vector<double>> class_std;
    std::vector<double> class_priors;
    int n_features = 0;
    int n_classes = 3;
    
public:
    void fit(const std::vector<Sample>& train_set,
            const std::vector<double>& feature_mean,
            const std::vector<double>& feature_std) override {
        
        if (train_set.empty()) return;
        
        n_features = train_set[0].features.size();
        class_mean.assign(n_classes, std::vector<double>(n_features, 0));
        class_std.assign(n_classes, std::vector<double>(n_features, 0));
        class_priors.assign(n_classes, 0);
        
        // Count samples per class
        std::vector<int> class_counts(n_classes, 0);
        for (const auto& sample : train_set) {
            class_counts[sample.label]++;
        }
        
        // Compute class priors
        for (int c = 0; c < n_classes; ++c) {
            class_priors[c] = static_cast<double>(class_counts[c]) / train_set.size();
        }
        
        // Compute mean and std per class
        for (const auto& sample : train_set) {
            for (int j = 0; j < n_features; ++j) {
                class_mean[sample.label][j] += sample.features[j];
            }
        }
        
        for (int c = 0; c < n_classes; ++c) {
            if (class_counts[c] > 0) {
                for (int j = 0; j < n_features; ++j) {
                    class_mean[c][j] /= class_counts[c];
                }
            }
        }
        
        // Compute std
        for (const auto& sample : train_set) {
            for (int j = 0; j < n_features; ++j) {
                double diff = sample.features[j] - class_mean[sample.label][j];
                class_std[sample.label][j] += diff * diff;
            }
        }
        
        for (int c = 0; c < n_classes; ++c) {
            if (class_counts[c] > 0) {
                for (int j = 0; j < n_features; ++j) {
                    class_std[c][j] = std::sqrt(class_std[c][j] / class_counts[c] + 1e-8);
                }
            }
        }
    }
    
    int predict(const std::vector<double>& features) override {
        auto proba = predict_proba(features);
        return std::max_element(proba.begin(), proba.end()) - proba.begin();
    }
    
    std::vector<double> predict_proba(const std::vector<double>& features) override {
        std::vector<double> proba(n_classes, 0);
        
        for (int c = 0; c < n_classes; ++c) {
            double log_prob = std::log(class_priors[c] + 1e-10);
            
            for (int j = 0; j < n_features; ++j) {
                double mean = class_mean[c][j];
                double std = class_std[c][j];
                double diff = features[j] - mean;
                
                // Gaussian PDF
                double numerator = std::exp(-(diff * diff) / (2 * std * std));
                double denominator = std::sqrt(2 * 3.14159 * std * std);
                double pdf = numerator / (denominator + 1e-10);
                
                log_prob += std::log(pdf + 1e-10);
            }
            
            proba[c] = log_prob;
        }
        
        // Softmax
        double max_prob = *std::max_element(proba.begin(), proba.end());
        double sum = 0;
        for (auto& p : proba) {
            p = std::exp(p - max_prob);
            sum += p;
        }
        for (auto& p : proba) {
            p /= sum;
        }
        
        return proba;
    }
};

// ============ Training Pipeline ============

int main(int argc, char* argv[]) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    std::cout << "\n" << std::string(80, '=') << std::endl;
    std::cout << "MULTI-TIER BULLET HOLE DETECTION - TRAINING PIPELINE" << std::endl;
    std::cout << std::string(80, '=') << std::endl;
    
    try {
        // ===== STEP 1: DATA LOADING =====
        std::cout << "\n[STEP 1] Loading dataset..." << std::endl;
        
        std::string csv_path = "dataset_ir_realistic/annotations.csv";
        int n_features = 10;
        
        Dataset full_data = load_dataset(csv_path, n_features);
        
        // ===== STEP 2: SHUFFLE & SPLIT =====
        std::cout << "\n[STEP 2] Shuffling and splitting data..." << std::endl;
        
        std::mt19937 rng(42);  // Fixed seed for reproducibility
        std::shuffle(full_data.samples.begin(), full_data.samples.end(), rng);
        
        size_t n_samples = full_data.samples.size();
        size_t train_size = static_cast<size_t>(n_samples * 0.70);
        size_t val_size = static_cast<size_t>(n_samples * 0.15);
        size_t test_size = n_samples - train_size - val_size;
        
        std::cout << "[SPLIT] Train: " << train_size << ", Val: " << val_size 
                  << ", Test: " << test_size << std::endl;
        
        // ===== STEP 3: NORMALIZATION =====
        std::cout << "\n[STEP 3] Computing feature normalization..." << std::endl;
        
        compute_normalization(full_data, train_size);
        
        // Normalize all samples
        for (auto& sample : full_data.samples) {
            sample.features = normalize_features(sample.features, 
                                               full_data.feature_mean,
                                               full_data.feature_std);
        }
        
        std::cout << "[NORM] Feature normalization applied" << std::endl;
        
        // ===== STEP 4: TIER 1 TRAINING =====
        std::cout << "\n[STEP 4] Training Tier 1 (Naive Bayes)..." << std::endl;
        
        NaiveBayesClassifier nb_classifier;
        
        std::vector<Sample> train_set(full_data.samples.begin(),
                                     full_data.samples.begin() + train_size);
        
        nb_classifier.fit(train_set, full_data.feature_mean, full_data.feature_std);
        
        std::cout << "[TIER1] Naive Bayes classifier trained" << std::endl;
        
        // ===== STEP 5: TIER 1 VALIDATION =====
        std::cout << "\n[STEP 5] Validating Tier 1 on validation set..." << std::endl;
        
        int val_correct = 0;
        double val_latency_total = 0;
        
        for (size_t i = train_size; i < train_size + val_size; ++i) {
            auto start_pred = std::chrono::high_resolution_clock::now();
            int pred = nb_classifier.predict(full_data.samples[i].features);
            auto end_pred = std::chrono::high_resolution_clock::now();
            
            val_latency_total += std::chrono::duration<double, std::milli>(end_pred - start_pred).count();
            
            if (pred == full_data.samples[i].label) {
                val_correct++;
            }
        }
        
        double val_accuracy = static_cast<double>(val_correct) / val_size;
        double val_avg_latency = val_latency_total / val_size;
        
        std::cout << "[VAL] Accuracy: " << std::fixed << std::setprecision(4) 
                  << val_accuracy * 100 << "%" << std::endl;
        std::cout << "[VAL] Avg latency: " << val_avg_latency << " ms" << std::endl;
        
        // ===== STEP 6: TIER 1 TESTING =====
        std::cout << "\n[STEP 6] Testing Tier 1 on test set..." << std::endl;
        
        Metrics test_metrics;
        test_metrics.avg_latency_ms = 0;
        int test_correct = 0;
        double test_latency_total = 0;
        
        for (size_t i = train_size + val_size; i < n_samples; ++i) {
            auto start_pred = std::chrono::high_resolution_clock::now();
            int pred = nb_classifier.predict(full_data.samples[i].features);
            auto end_pred = std::chrono::high_resolution_clock::now();
            
            test_latency_total += std::chrono::duration<double, std::milli>(end_pred - start_pred).count();
            
            int true_label = full_data.samples[i].label;
            test_metrics.confusion_matrix[true_label][pred]++;
            
            if (pred == true_label) {
                test_correct++;
            }
        }
        
        test_metrics.accuracy = static_cast<double>(test_correct) / test_size;
        test_metrics.avg_latency_ms = test_latency_total / test_size;
        
        // Compute per-class metrics
        for (int label = 0; label < 3; ++label) {
            int tp = test_metrics.confusion_matrix[label][label];
            int fp = 0, fn = 0;
            
            for (int i = 0; i < 3; ++i) {
                if (i != label) {
                    fp += test_metrics.confusion_matrix[i][label];
                    fn += test_metrics.confusion_matrix[label][i];
                }
            }
            
            test_metrics.precision[label] = (tp + fp > 0) ? 
                static_cast<double>(tp) / (tp + fp) : 0;
            test_metrics.recall[label] = (tp + fn > 0) ? 
                static_cast<double>(tp) / (tp + fn) : 0;
            
            double p = test_metrics.precision[label];
            double r = test_metrics.recall[label];
            test_metrics.f1[label] = (p + r > 0) ? 2 * p * r / (p + r) : 0;
        }
        
        // ===== STEP 7: RESULTS PRINTING =====
        std::cout << "\n" << std::string(80, '=') << std::endl;
        std::cout << "EVALUATION RESULTS" << std::endl;
        std::cout << std::string(80, '=') << std::endl;
        
        std::cout << "\nTest Set Metrics:" << std::endl;
        std::cout << "  Accuracy: " << std::fixed << std::setprecision(4) 
                  << test_metrics.accuracy * 100 << "%" << std::endl;
        std::cout << "  Avg Latency: " << test_metrics.avg_latency_ms << " ms" << std::endl;
        
        std::cout << "\nPer-Class Metrics:" << std::endl;
        std::cout << "Label        Precision   Recall      F1-Score" << std::endl;
        std::cout << "---------------------------------------------------" << std::endl;
        
        const std::string labels[] = {"Bullet", "Non-Bullet", "Ambiguous"};
        for (int i = 0; i < 3; ++i) {
            std::cout << std::left << std::setw(12) << labels[i]
                      << std::setw(12) << std::fixed << std::setprecision(4) << test_metrics.precision[i]
                      << std::setw(12) << test_metrics.recall[i]
                      << test_metrics.f1[i] << std::endl;
        }
        
        std::cout << "\nConfusion Matrix:" << std::endl;
        std::cout << "              Pred 0      Pred 1      Pred 2" << std::endl;
        std::cout << "---------------------------------------------------" << std::endl;
        
        for (int i = 0; i < 3; ++i) {
            std::cout << std::left << std::setw(14) << ("True " + std::to_string(i));
            for (int j = 0; j < 3; ++j) {
                std::cout << std::setw(12) << test_metrics.confusion_matrix[i][j];
            }
            std::cout << std::endl;
        }
        
        // ===== STEP 8: DEBUG - PRINT 5 RANDOM SAMPLES =====
        std::cout << "\n" << std::string(80, '=') << std::endl;
        std::cout << "DEBUG - SAMPLE PREDICTIONS (5 random test samples)" << std::endl;
        std::cout << std::string(80, '=') << std::endl;
        
        std::uniform_int_distribution<size_t> dist(train_size + val_size, n_samples - 1);
        for (int i = 0; i < 5; ++i) {
            size_t idx = dist(rng);
            int pred = nb_classifier.predict(full_data.samples[idx].features);
            auto proba = nb_classifier.predict_proba(full_data.samples[idx].features);
            
            std::cout << "\nSample " << i + 1 << ":" << std::endl;
            std::cout << "  True Label: " << full_data.samples[idx].label << std::endl;
            std::cout << "  Predicted: " << pred << std::endl;
            std::cout << "  Confidence: " << std::fixed << std::setprecision(4) 
                      << *std::max_element(proba.begin(), proba.end()) * 100 << "%" << std::endl;
            std::cout << "  Features (first 5): ";
            for (int j = 0; j < std::min(5, (int)full_data.samples[idx].features.size()); ++j) {
                std::cout << std::fixed << std::setprecision(2) 
                         << full_data.samples[idx].features[j] << " ";
            }
            std::cout << std::endl;
        }
        
        // ===== STEP 9: REPORT GENERATION =====
        std::cout << "\n[STEP 9] Generating report..." << std::endl;
        
        std::ofstream report("results/training_report.txt");
        if (report.is_open()) {
            report << "=== MULTI-TIER BULLET HOLE DETECTION - TRAINING REPORT ===" << std::endl;
            report << "Date: " << std::chrono::system_clock::now().time_since_epoch().count() << std::endl;
            report << std::endl;
            
            report << "Dataset Statistics:" << std::endl;
            report << "  Total Samples: " << n_samples << std::endl;
            report << "  Training: " << train_size << std::endl;
            report << "  Validation: " << val_size << std::endl;
            report << "  Testing: " << test_size << std::endl;
            report << std::endl;
            
            report << "Tier 1 (Naive Bayes) - Validation:" << std::endl;
            report << "  Accuracy: " << std::fixed << std::setprecision(4) << val_accuracy * 100 << "%" << std::endl;
            report << "  Avg Latency: " << val_avg_latency << " ms" << std::endl;
            report << std::endl;
            
            report << "Tier 1 (Naive Bayes) - Test:" << std::endl;
            report << "  Accuracy: " << test_metrics.accuracy * 100 << "%" << std::endl;
            report << "  Avg Latency: " << test_metrics.avg_latency_ms << " ms" << std::endl;
            report << std::endl;
            
            report << "Per-Class Metrics (Test):" << std::endl;
            for (int i = 0; i < 3; ++i) {
                report << "  Class " << i << ":" << std::endl;
                report << "    Precision: " << test_metrics.precision[i] << std::endl;
                report << "    Recall: " << test_metrics.recall[i] << std::endl;
                report << "    F1-Score: " << test_metrics.f1[i] << std::endl;
            }
            
            report.close();
            std::cout << "[REPORT] Saved to results/training_report.txt" << std::endl;
        }
        
        // ===== FINAL SUMMARY =====
        auto end_time = std::chrono::high_resolution_clock::now();
        double total_time_sec = std::chrono::duration<double>(end_time - start_time).count();
        
        std::cout << "\n" << std::string(80, '=') << std::endl;
        std::cout << "TRAINING PIPELINE COMPLETED SUCCESSFULLY" << std::endl;
        std::cout << std::string(80, '=') << std::endl;
        
        std::cout << "\nFinal Summary:" << std::endl;
        std::cout << "  Total Samples: " << n_samples << std::endl;
        std::cout << "  Validation Accuracy (Tier 1): " << std::fixed << std::setprecision(2) 
                  << val_accuracy * 100 << "%" << std::endl;
        std::cout << "  Test Accuracy (Tier 1): " << test_metrics.accuracy * 100 << "%" << std::endl;
        std::cout << "  Average Latency: " << test_metrics.avg_latency_ms << " ms" << std::endl;
        std::cout << "  Total Runtime: " << total_time_sec << " seconds" << std::endl;
        std::cout << "  Status: ? SUCCESS" << std::endl;
        
        std::cout << std::string(80, '=') << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        ErrorLogger::getInstance().log(ErrorLogger::LogLevel::ERROR,
            std::string("Training pipeline failed: ") + e.what());
        std::cerr << "\nERROR: " << e.what() << std::endl;
        return 1;
    }
}
