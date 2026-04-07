// Standalone Training Executable
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
#include <filesystem>

namespace fs = std::filesystem;

// ============ Simple Data Structures ============

struct Sample {
    std::vector<double> features;
    int label;
};

struct Metrics {
    double accuracy = 0;
    double precision[3] = {0, 0, 0};
    double recall[3] = {0, 0, 0};
    double f1[3] = {0, 0, 0};
    int confusion_matrix[3][3] = {{0}};
    double avg_latency_ms = 0;
};

// ============ CSV Loading ============

std::vector<Sample> load_csv(const std::string& path) {
    std::vector<Sample> dataset;
    std::ifstream file(path);
    
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open: " + path);
    }
    
    std::string line;
    bool header = true;
    
    while (std::getline(file, line)) {
        if (header) {
            header = false;
            continue;
        }
        
        std::istringstream iss(line);
        std::string token;
        Sample s;
        int field = 0, label = -1;
        
        while (std::getline(iss, token, ',')) {
            try {
                if (field < 10) {
                    s.features.push_back(std::stod(token));
                } else if (field == 11) {
                    label = std::stoi(token);
                }
                field++;
            } catch (...) {}
        }
        
        if ((label == 0 || label == 1 || label == -1) && s.features.size() == 10) {
            s.label = (label == -1) ? 2 : label;
            dataset.push_back(s);
        }
    }
    
    file.close();
    return dataset;
}

// ============ Naive Bayes Classifier ============

class NaiveBayesClassifier {
private:
    std::vector<std::vector<double>> class_mean;
    std::vector<std::vector<double>> class_std;
    std::vector<double> class_priors;
    int n_features = 0;
    static const int N_CLASSES = 3;
    
public:
    void fit(const std::vector<Sample>& train_data) {
        if (train_data.empty()) return;
        
        n_features = train_data[0].features.size();
        class_mean.assign(N_CLASSES, std::vector<double>(n_features, 0));
        class_std.assign(N_CLASSES, std::vector<double>(n_features, 0));
        class_priors.assign(N_CLASSES, 0);
        
        std::vector<int> class_counts(N_CLASSES, 0);
        
        for (const auto& s : train_data) {
            class_counts[s.label]++;
        }
        
        for (int c = 0; c < N_CLASSES; ++c) {
            class_priors[c] = static_cast<double>(class_counts[c]) / train_data.size();
        }
        
        for (const auto& s : train_data) {
            for (int j = 0; j < n_features; ++j) {
                class_mean[s.label][j] += s.features[j];
            }
        }
        
        for (int c = 0; c < N_CLASSES; ++c) {
            if (class_counts[c] > 0) {
                for (int j = 0; j < n_features; ++j) {
                    class_mean[c][j] /= class_counts[c];
                }
            }
        }
        
        for (const auto& s : train_data) {
            for (int j = 0; j < n_features; ++j) {
                double diff = s.features[j] - class_mean[s.label][j];
                class_std[s.label][j] += diff * diff;
            }
        }
        
        for (int c = 0; c < N_CLASSES; ++c) {
            if (class_counts[c] > 0) {
                for (int j = 0; j < n_features; ++j) {
                    class_std[c][j] = std::sqrt(class_std[c][j] / class_counts[c] + 1e-8);
                }
            }
        }
    }
    
    int predict(const std::vector<double>& features) {
        auto proba = predict_proba(features);
        return std::max_element(proba.begin(), proba.end()) - proba.begin();
    }
    
    std::vector<double> predict_proba(const std::vector<double>& features) {
        std::vector<double> proba(N_CLASSES, 0);
        
        for (int c = 0; c < N_CLASSES; ++c) {
            double log_p = std::log(class_priors[c] + 1e-10);
            
            for (int j = 0; j < n_features; ++j) {
                double m = class_mean[c][j];
                double s = class_std[c][j];
                double d = features[j] - m;
                
                double pdf = std::exp(-(d * d) / (2 * s * s)) / (std::sqrt(2 * 3.14159 * s * s) + 1e-10);
                log_p += std::log(pdf + 1e-10);
            }
            
            proba[c] = log_p;
        }
        
        // Softmax
        double mx = *std::max_element(proba.begin(), proba.end());
        double sum = 0;
        for (auto& p : proba) {
            p = std::exp(p - mx);
            sum += p;
        }
        for (auto& p : proba) {
            p /= sum;
        }
        
        return proba;
    }
};

// ============ Main Training Pipeline ============

int main() {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    std::cout << "\n" << std::string(80, '=') << std::endl;
    std::cout << "MULTI-TIER BULLET HOLE DETECTION - TRAINING PIPELINE" << std::endl;
    std::cout << std::string(80, '=') << std::endl;
    
    try {
        // Create results directory
        if (!fs::exists("results")) {
            fs::create_directory("results");
        }
        
        // STEP 1: Load Dataset
        std::cout << "\n[STEP 1] Loading dataset..." << std::endl;
        auto dataset = load_csv("dataset_ir_realistic/annotations.csv");
        std::cout << "[DATA] Loaded " << dataset.size() << " samples" << std::endl;
        
        // STEP 2: Shuffle
        std::cout << "\n[STEP 2] Shuffling data..." << std::endl;
        std::mt19937 rng(42);
        std::shuffle(dataset.begin(), dataset.end(), rng);
        
        size_t n = dataset.size();
        size_t train_sz = static_cast<size_t>(n * 0.70);
        size_t val_sz = static_cast<size_t>(n * 0.15);
        size_t test_sz = n - train_sz - val_sz;
        
        std::cout << "[SPLIT] Train: " << train_sz << ", Val: " << val_sz << ", Test: " << test_sz << std::endl;
        
        // STEP 3: Normalize
        std::cout << "\n[STEP 3] Normalizing features..." << std::endl;
        
        std::vector<double> mean(10, 0), std_dev(10, 0);
        
        for (int j = 0; j < 10; ++j) {
            for (size_t i = 0; i < train_sz; ++i) {
                mean[j] += dataset[i].features[j];
            }
            mean[j] /= train_sz;
        }
        
        for (int j = 0; j < 10; ++j) {
            for (size_t i = 0; i < train_sz; ++i) {
                double diff = dataset[i].features[j] - mean[j];
                std_dev[j] += diff * diff;
            }
            std_dev[j] = std::sqrt(std_dev[j] / train_sz + 1e-8);
        }
        
        for (auto& s : dataset) {
            for (int j = 0; j < 10; ++j) {
                s.features[j] = (s.features[j] - mean[j]) / (std_dev[j] + 1e-8);
            }
        }
        
        std::cout << "[NORM] Applied normalization" << std::endl;
        
        // STEP 4: Train
        std::cout << "\n[STEP 4] Training Naive Bayes..." << std::endl;
        
        NaiveBayesClassifier clf;
        std::vector<Sample> train_data(dataset.begin(), dataset.begin() + train_sz);
        clf.fit(train_data);
        
        std::cout << "[TIER1] Naive Bayes trained" << std::endl;
        
        // STEP 5: Validate
        std::cout << "\n[STEP 5] Validating..." << std::endl;
        
        int val_correct = 0;
        for (size_t i = train_sz; i < train_sz + val_sz; ++i) {
            if (clf.predict(dataset[i].features) == dataset[i].label) {
                val_correct++;
            }
        }
        
        double val_acc = static_cast<double>(val_correct) / val_sz;
        std::cout << "[VAL] Accuracy: " << std::fixed << std::setprecision(4) << val_acc * 100 << "%" << std::endl;
        
        // STEP 6: Test
        std::cout << "\n[STEP 6] Testing..." << std::endl;
        
        Metrics metrics;
        int test_correct = 0;
        double latency_total = 0;
        
        for (size_t i = train_sz + val_sz; i < n; ++i) {
            auto t1 = std::chrono::high_resolution_clock::now();
            int pred = clf.predict(dataset[i].features);
            auto t2 = std::chrono::high_resolution_clock::now();
            
            latency_total += std::chrono::duration<double, std::milli>(t2 - t1).count();
            
            metrics.confusion_matrix[dataset[i].label][pred]++;
            if (pred == dataset[i].label) {
                test_correct++;
            }
        }
        
        metrics.accuracy = static_cast<double>(test_correct) / test_sz;
        metrics.avg_latency_ms = latency_total / test_sz;
        
        // Compute per-class metrics
        for (int c = 0; c < 3; ++c) {
            int tp = metrics.confusion_matrix[c][c];
            int fp = 0, fn = 0;
            
            for (int i = 0; i < 3; ++i) {
                if (i != c) {
                    fp += metrics.confusion_matrix[i][c];
                    fn += metrics.confusion_matrix[c][i];
                }
            }
            
            metrics.precision[c] = (tp + fp > 0) ? static_cast<double>(tp) / (tp + fp) : 0;
            metrics.recall[c] = (tp + fn > 0) ? static_cast<double>(tp) / (tp + fn) : 0;
            
            double p = metrics.precision[c];
            double r = metrics.recall[c];
            metrics.f1[c] = (p + r > 0) ? 2 * p * r / (p + r) : 0;
        }
        
        // STEP 7: Print Results
        std::cout << "\n" << std::string(80, '=') << std::endl;
        std::cout << "EVALUATION RESULTS" << std::endl;
        std::cout << std::string(80, '=') << std::endl;
        
        std::cout << "\nTest Metrics:" << std::endl;
        std::cout << "  Accuracy: " << std::fixed << std::setprecision(4) << metrics.accuracy * 100 << "%" << std::endl;
        std::cout << "  Avg Latency: " << metrics.avg_latency_ms << " ms" << std::endl;
        
        std::cout << "\nPer-Class Metrics:" << std::endl;
        std::cout << "Label        Precision   Recall      F1-Score" << std::endl;
        std::cout << "---------------------------------------------------" << std::endl;
        
        const std::string labels[] = {"Bullet", "Non-Bullet", "Ambiguous"};
        for (int i = 0; i < 3; ++i) {
            std::cout << std::left << std::setw(12) << labels[i]
                      << std::setw(12) << metrics.precision[i]
                      << std::setw(12) << metrics.recall[i]
                      << metrics.f1[i] << std::endl;
        }
        
        std::cout << "\nConfusion Matrix:" << std::endl;
        std::cout << "              Pred 0      Pred 1      Pred 2" << std::endl;
        std::cout << "---------------------------------------------------" << std::endl;
        for (int i = 0; i < 3; ++i) {
            std::cout << std::left << std::setw(14) << ("True " + std::to_string(i));
            for (int j = 0; j < 3; ++j) {
                std::cout << std::setw(12) << metrics.confusion_matrix[i][j];
            }
            std::cout << std::endl;
        }
        
        // STEP 8: Debug Samples
        std::cout << "\n" << std::string(80, '=') << std::endl;
        std::cout << "DEBUG - 5 RANDOM TEST SAMPLES" << std::endl;
        std::cout << std::string(80, '=') << std::endl;
        
        std::uniform_int_distribution<size_t> dist(train_sz + val_sz, n - 1);
        for (int i = 0; i < 5; ++i) {
            size_t idx = dist(rng);
            int pred = clf.predict(dataset[idx].features);
            auto proba = clf.predict_proba(dataset[idx].features);
            
            std::cout << "\nSample " << i + 1 << ":" << std::endl;
            std::cout << "  True Label: " << dataset[idx].label << std::endl;
            std::cout << "  Predicted: " << pred << std::endl;
            std::cout << "  Confidence: " << *std::max_element(proba.begin(), proba.end()) * 100 << "%" << std::endl;
            std::cout << "  Features (first 5): ";
            for (int j = 0; j < 5; ++j) {
                std::cout << std::fixed << std::setprecision(2) << dataset[idx].features[j] << " ";
            }
            std::cout << std::endl;
        }
        
        // STEP 9: Save Report
        std::cout << "\n[STEP 9] Saving report..." << std::endl;
        
        std::ofstream report("results/training_report.txt");
        report << "=== MULTI-TIER BULLET HOLE DETECTION - TRAINING REPORT ===" << std::endl;
        report << "\nDataset Statistics:" << std::endl;
        report << "  Total: " << n << ", Train: " << train_sz << ", Val: " << val_sz << ", Test: " << test_sz << std::endl;
        report << "\nTier 1 (Naive Bayes):" << std::endl;
        report << "  Val Accuracy: " << val_acc * 100 << "%" << std::endl;
        report << "  Test Accuracy: " << metrics.accuracy * 100 << "%" << std::endl;
        report << "  Avg Latency: " << metrics.avg_latency_ms << " ms" << std::endl;
        report << "\nPer-Class (Test):" << std::endl;
        for (int i = 0; i < 3; ++i) {
            report << "  Class " << i << ": P=" << metrics.precision[i] 
                   << ", R=" << metrics.recall[i] << ", F1=" << metrics.f1[i] << std::endl;
        }
        report.close();
        
        // FINAL SUMMARY
        auto end_time = std::chrono::high_resolution_clock::now();
        double total_sec = std::chrono::duration<double>(end_time - start_time).count();
        
        std::cout << "\n" << std::string(80, '=') << std::endl;
        std::cout << "TRAINING PIPELINE COMPLETED SUCCESSFULLY" << std::endl;
        std::cout << std::string(80, '=') << std::endl;
        
        std::cout << "\nFinal Summary:" << std::endl;
        std::cout << "  Total Samples: " << n << std::endl;
        std::cout << "  Val Accuracy (Tier 1): " << std::fixed << std::setprecision(2) << val_acc * 100 << "%" << std::endl;
        std::cout << "  Test Accuracy (Tier 1): " << metrics.accuracy * 100 << "%" << std::endl;
        std::cout << "  Average Latency: " << metrics.avg_latency_ms << " ms" << std::endl;
        std::cout << "  Total Runtime: " << total_sec << " seconds" << std::endl;
        std::cout << "  Status: ? SUCCESS" << std::endl;
        std::cout << std::string(80, '=') << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "\nERROR: " << e.what() << std::endl;
        return 1;
    }
}
