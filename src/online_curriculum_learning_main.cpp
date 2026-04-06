// online_curriculum_learning_main.cpp - Online Curriculum Learning Loop
#include <iostream>
#include <iomanip>
#include <fstream>
#include <chrono>

#include "training/OnlineCurriculumLearner.h"
#include "core/ErrorHandler.h"

using namespace bullet_detection;

void printLoopHeader() {
    std::cout << "\n" << std::string(80, '=') << std::endl;
    std::cout << "ONLINE CURRICULUM LEARNING LOOP FOR BULLET HOLE DETECTION" << std::endl;
    std::cout << std::string(80, '=') << std::endl;
}

void printIterationProgress(const OnlineTrainingHistory& history) {
    if (history.iterations.empty()) return;
    
    std::cout << "\n" << std::string(80, '-') << std::endl;
    std::cout << "PROGRESS SUMMARY" << std::endl;
    std::cout << std::string(80, '-') << std::endl;
    
    for (size_t i = 0; i < history.iterations.size(); ++i) {
        const auto& iter = history.iterations[i];
        std::cout << "Iteration " << (i + 1) << ": ";
        std::cout << "Accuracy " << std::fixed << std::setprecision(2) 
                  << iter.accuracy_before * 100.0f << "% ? " 
                  << iter.accuracy_after * 100.0f << "% ";
        std::cout << "(+" << iter.accuracy_improvement * 100.0f << "%) | ";
        std::cout << "Dataset: " << iter.total_dataset_size << " | ";
        std::cout << "Time: " << iter.training_time_seconds << "s" << std::endl;
    }
}

void saveDetailedReport(const OnlineTrainingHistory& history, const std::string& filename) {
    std::ofstream report(filename);
    if (!report.is_open()) {
        std::cerr << "WARNING: Could not save detailed report" << std::endl;
        return;
    }
    
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    
    report << "================================================================================\n";
    report << "ONLINE CURRICULUM LEARNING LOOP - FINAL REPORT\n";
    report << "================================================================================\n";
    report << "Execution Date: " << std::ctime(&time);
    report << "Project: Bullet Hole Detection System (Multi-Tier)\n";
    report << "Task: Online Curriculum Learning with Continuous Data Generation\n";
    report << "\n";
    
    report << "================================================================================\n";
    report << "EXECUTION SUMMARY\n";
    report << "================================================================================\n";
    report << "Total Iterations: " << history.iterations.size() << "\n";
    report << "Total Samples Generated: " << history.total_samples_generated << "\n";
    report << "Best Iteration: " << history.best_iteration << "\n";
    report << "Best Accuracy: " << std::fixed << std::setprecision(4) 
           << history.best_accuracy * 100.0f << "%\n";
    
    if (!history.iterations.empty()) {
        float initial_acc = history.iterations.front().accuracy_before;
        float final_acc = history.iterations.back().accuracy_after;
        float total_improvement = final_acc - initial_acc;
        
        report << "Total Accuracy Improvement: " << std::fixed << std::setprecision(4) 
               << total_improvement * 100.0f << "%\n";
        report << "  From: " << initial_acc * 100.0f << "%\n";
        report << "  To: " << final_acc * 100.0f << "%\n";
        report << "Final Dataset Size: " << history.iterations.back().total_dataset_size << "\n";
    }
    
    report << "\n";
    report << "================================================================================\n";
    report << "ITERATION-BY-ITERATION BREAKDOWN\n";
    report << "================================================================================\n";
    
    for (size_t i = 0; i < history.iterations.size(); ++i) {
        const auto& iter = history.iterations[i];
        
        report << "\n--- ITERATION " << (i + 1) << " ---\n";
        report << "Samples Added: " << iter.samples_added << "\n";
        report << "Total Dataset Size: " << iter.total_dataset_size << "\n";
        report << "Hard Samples Identified: " << iter.hard_samples_identified << "\n";
        report << "Generated Samples:\n";
        report << "  Easy: " << iter.easy_samples_generated << "\n";
        report << "  Medium: " << iter.medium_samples_generated << "\n";
        report << "  Hard: " << iter.hard_samples_generated << "\n";
        
        report << "Performance Metrics:\n";
        report << "  Accuracy: " << std::fixed << std::setprecision(4) 
               << iter.accuracy_before * 100.0f << "% ? " 
               << iter.accuracy_after * 100.0f << "% ";
        report << "(+" << iter.accuracy_improvement * 100.0f << "%)\n";
        report << "  Precision: " << iter.precision_before * 100.0f << "% ? " 
               << iter.precision_after * 100.0f << "%\n";
        report << "  Recall: " << iter.recall_before * 100.0f << "% ? " 
               << iter.recall_after * 100.0f << "%\n";
        report << "  F1-Score: " << iter.f1_before * 100.0f << "% ? " 
               << iter.f1_after * 100.0f << "%\n";
        
        report << "Timings:\n";
        report << "  Training: " << iter.training_time_seconds << "s\n";
        report << "  Data Generation: " << iter.data_generation_time_seconds << "s\n";
        
        if (!iter.notes.empty()) {
            report << "Notes: " << iter.notes << "\n";
        }
    }
    
    report << "\n";
    report << "================================================================================\n";
    report << "TRENDS & ANALYSIS\n";
    report << "================================================================================\n";
    
    report << "Accuracy Trend:\n";
    for (size_t i = 0; i < history.accuracy_trend.size(); ++i) {
        report << "  Iter " << (i + 1) << ": " << std::fixed << std::setprecision(4) 
               << history.accuracy_trend[i] * 100.0f << "%\n";
    }
    
    report << "\nRecall Trend:\n";
    for (size_t i = 0; i < history.recall_trend.size(); ++i) {
        report << "  Iter " << (i + 1) << ": " << std::fixed << std::setprecision(4) 
               << history.recall_trend[i] * 100.0f << "%\n";
    }
    
    report << "\nDataset Size Growth:\n";
    for (size_t i = 0; i < history.dataset_size_trend.size(); ++i) {
        report << "  Iter " << (i + 1) << ": " << history.dataset_size_trend[i] << " samples\n";
    }
    
    report << "\n";
    report << "================================================================================\n";
    report << "COMPLETION STATUS: ? ONLINE CURRICULUM LEARNING LOOP COMPLETED\n";
    report << "================================================================================\n";
    report << "\nThe online curriculum learning system has successfully completed the training loop.\n";
    report << "The model has been continuously improved through iterative data generation and\n";
    report << "incremental training with curriculum learning strategy.\n";
    report << "\nNext Steps:\n";
    report << "  1. Review the accuracy trend and validate improvements\n";
    report << "  2. Check the best-performing model (iteration " << history.best_iteration << ")\n";
    report << "  3. Deploy the model to production if performance is satisfactory\n";
    report << "  4. Monitor performance in production environment\n";
    report << "  5. Plan next cycle of online curriculum learning\n";
    
    report.close();
    std::cout << "Detailed report saved to: " << filename << std::endl;
}

int main(int argc, char* argv[]) {
    printLoopHeader();
    
    try {
        // ===== CONFIGURATION =====
        
        std::string initial_dataset = "dataset_ir_realistic/annotations.csv";
        std::string model_dir = "models";
        std::string scaler_path = "models/scaler.bin";
        std::string generator_script = "scripts/generate_dataset_realistic.py";
        
        // Allow command-line override
        if (argc > 1) {
            initial_dataset = argv[1];
        }
        if (argc > 2) {
            model_dir = argv[2];
        }
        if (argc > 3) {
            generator_script = argv[3];
        }
        
        // Create output directories
        system("if not exist models mkdir models");
        system("if not exist results mkdir results");
        
        // Online curriculum configuration
        OnlineCurriculumConfig config;
        config.max_iterations = 10;
        config.samples_per_iteration = 1000;
        config.learning_rate = 0.001f;
        config.epochs_per_iteration = 50;
        config.use_curriculum = true;
        config.focus_on_hard_samples = true;
        config.new_data_ratio = 0.70f;
        config.old_data_ratio = 0.30f;
        config.improvement_threshold = 0.001f;
        config.convergence_patience = 3;
        config.validation_split = 0.20f;
        config.max_dataset_size = 50000;
        
        std::cout << "\n[CONFIGURATION]" << std::endl;
        std::cout << "  Initial dataset: " << initial_dataset << std::endl;
        std::cout << "  Model directory: " << model_dir << std::endl;
        std::cout << "  Generator script: " << generator_script << std::endl;
        std::cout << "  Max iterations: " << config.max_iterations << std::endl;
        std::cout << "  Samples per iteration: " << config.samples_per_iteration << std::endl;
        std::cout << "  Learning rate: " << std::fixed << std::setprecision(5) 
                  << config.learning_rate << std::endl;
        std::cout << "  Curriculum learning: " << (config.use_curriculum ? "ENABLED" : "DISABLED") << std::endl;
        std::cout << "  Focus on hard samples: " << (config.focus_on_hard_samples ? "YES" : "NO") << std::endl;
        std::cout << "  New data ratio: " << std::fixed << std::setprecision(2) 
                  << (config.new_data_ratio * 100.0f) << "%" << std::endl;
        
        // ===== RUN ONLINE CURRICULUM LEARNING LOOP =====
        
        auto total_start = std::chrono::high_resolution_clock::now();
        
        OnlineCurriculumLearner learner;
        OnlineTrainingHistory history;
        
        bool success = learner.runOnlineCurriculumLoop(
            initial_dataset,
            model_dir,
            scaler_path,
            generator_script,
            config,
            history
        );
        
        auto total_end = std::chrono::high_resolution_clock::now();
        auto total_time = std::chrono::duration_cast<std::chrono::minutes>(total_end - total_start);
        
        if (!success) {
            std::cerr << "ERROR: Online curriculum learning failed" << std::endl;
            return 1;
        }
        
        // ===== PRINT RESULTS =====
        
        printIterationProgress(history);
        
        // ===== SAVE DETAILED REPORT =====
        
        std::string report_filename = "results/ONLINE_CURRICULUM_LEARNING_REPORT.txt";
        saveDetailedReport(history, report_filename);
        
        // ===== FINAL OUTPUT =====
        
        std::cout << "\n" << std::string(80, '=') << std::endl;
        std::cout << "? ONLINE CURRICULUM LEARNING COMPLETED" << std::endl;
        std::cout << std::string(80, '=') << std::endl;
        
        std::cout << "\n[FINAL STATISTICS]" << std::endl;
        std::cout << "  Total iterations: " << history.iterations.size() << std::endl;
        std::cout << "  Total samples generated: " << history.total_samples_generated << std::endl;
        std::cout << "  Final dataset size: ";
        if (!history.iterations.empty()) {
            std::cout << history.iterations.back().total_dataset_size << std::endl;
        } else {
            std::cout << "N/A" << std::endl;
        }
        
        std::cout << "  Best accuracy: " << std::fixed << std::setprecision(2) 
                  << history.best_accuracy * 100.0f << "% (iteration " << history.best_iteration << ")" << std::endl;
        
        if (!history.iterations.empty()) {
            float initial_acc = history.iterations.front().accuracy_before;
            float final_acc = history.iterations.back().accuracy_after;
            std::cout << "  Total improvement: " << std::fixed << std::setprecision(2) 
                      << (final_acc - initial_acc) * 100.0f << "%" << std::endl;
        }
        
        std::cout << "  Total execution time: " << total_time.count() << " minutes" << std::endl;
        
        std::cout << "\n? All iterations completed successfully!" << std::endl;
        std::cout << "? Models saved with version numbering" << std::endl;
        std::cout << "? Results logged to " << report_filename << std::endl;
        
        std::cout << "\n" << std::string(80, '=') << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "FATAL ERROR: " << e.what() << std::endl;
        return 1;
    }
}
