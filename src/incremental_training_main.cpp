// incremental_training_main.cpp - Continuous Learning Pipeline
#include <iostream>
#include <iomanip>
#include <fstream>
#include <chrono>
#include <ctime>

#include "training/IncrementalTrainer.h"
#include "core/ErrorHandler.h"

using namespace bullet_detection;

void printMetrics(const IncrementalTrainingMetrics& metrics) {
    std::cout << "\n" << std::string(80, '=') << std::endl;
    std::cout << "INCREMENTAL TRAINING FINAL REPORT" << std::endl;
    std::cout << std::string(80, '=') << std::endl;
    
    std::cout << "\n[DATASET STATISTICS]" << std::endl;
    std::cout << "  New samples processed: " << metrics.new_samples_count << std::endl;
    std::cout << "  Old samples (replay buffer): " << metrics.old_samples_count_used << std::endl;
    std::cout << "  Total training samples: " << metrics.total_training_samples << std::endl;
    
    std::cout << "\n[PERFORMANCE METRICS]" << std::endl;
    std::cout << "  Initial accuracy (before): " << std::fixed << std::setprecision(4) 
              << metrics.initial_accuracy * 100.0f << "%" << std::endl;
    std::cout << "  Final accuracy (after):   " << std::fixed << std::setprecision(4) 
              << metrics.final_accuracy * 100.0f << "%" << std::endl;
    std::cout << "  Improvement: " << std::fixed << std::setprecision(4) 
              << metrics.accuracy_change * 100.0f << "%" << std::endl;
    
    std::cout << "\n[PRECISION & RECALL]" << std::endl;
    std::cout << "  Precision (before): " << std::fixed << std::setprecision(4) 
              << metrics.precision_before * 100.0f << "%" << std::endl;
    std::cout << "  Precision (after):  " << std::fixed << std::setprecision(4) 
              << metrics.precision_after * 100.0f << "%" << std::endl;
    std::cout << "  Recall (before): " << std::fixed << std::setprecision(4) 
              << metrics.recall_before * 100.0f << "%" << std::endl;
    std::cout << "  Recall (after):  " << std::fixed << std::setprecision(4) 
              << metrics.recall_after * 100.0f << "%" << std::endl;
    
    std::cout << "\n[TRAINING METRICS]" << std::endl;
    std::cout << "  Initial loss: " << std::fixed << std::setprecision(6) 
              << metrics.initial_loss << std::endl;
    std::cout << "  Final loss:   " << std::fixed << std::setprecision(6) 
              << metrics.final_loss << std::endl;
    std::cout << "  Epochs trained: " << metrics.epochs_trained << std::endl;
    std::cout << "  Training time: " << std::fixed << std::setprecision(2) 
              << metrics.training_time_seconds << " seconds" << std::endl;
    
    std::cout << "\n[STABILITY CHECK]" << std::endl;
    std::cout << "  Status: " << (metrics.is_stable ? "? STABLE" : "? UNSTABLE") << std::endl;
    std::cout << "  Max performance drop: " << std::fixed << std::setprecision(4) 
              << metrics.max_performance_drop * 100.0f << "%" << std::endl;
    std::cout << "  Details: " << metrics.stability_status << std::endl;
    
    std::cout << "\n" << std::string(80, '=') << std::endl;
}

void saveReportToFile(const IncrementalTrainingMetrics& metrics, const std::string& filename) {
    std::ofstream report(filename);
    if (!report.is_open()) {
        std::cerr << "WARNING: Could not save report to " << filename << std::endl;
        return;
    }
    
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    
    report << "================================================================================\n";
    report << "INCREMENTAL TRAINING REPORT - CONTINUOUS LEARNING\n";
    report << "================================================================================\n";
    report << "Execution Date: " << std::ctime(&time);
    report << "Project: Bullet Hole Detection System (Multi-Tier)\n";
    report << "Task: Continuous Learning via Incremental Training\n";
    report << "\n";
    
    report << "================================================================================\n";
    report << "EXECUTION STATUS: ? COMPLETED SUCCESSFULLY\n";
    report << "================================================================================\n";
    report << "\n";
    
    report << "DATASET STATISTICS:\n";
    report << "  New samples processed: " << metrics.new_samples_count << "\n";
    report << "  Old samples (replay buffer): " << metrics.old_samples_count_used << "\n";
    report << "  Total training samples: " << metrics.total_training_samples << "\n";
    report << "\n";
    
    report << "PERFORMANCE METRICS:\n";
    report << "  Initial accuracy (before): " << std::fixed << std::setprecision(4) 
           << metrics.initial_accuracy * 100.0f << "%\n";
    report << "  Final accuracy (after):    " << std::fixed << std::setprecision(4) 
           << metrics.final_accuracy * 100.0f << "%\n";
    report << "  Improvement:               " << std::fixed << std::setprecision(4) 
           << metrics.accuracy_change * 100.0f << "%\n";
    report << "\n";
    
    report << "PRECISION & RECALL:\n";
    report << "  Precision (before): " << std::fixed << std::setprecision(4) 
           << metrics.precision_before * 100.0f << "%\n";
    report << "  Precision (after):  " << std::fixed << std::setprecision(4) 
           << metrics.precision_after * 100.0f << "%\n";
    report << "  Recall (before): " << std::fixed << std::setprecision(4) 
           << metrics.recall_before * 100.0f << "%\n";
    report << "  Recall (after):  " << std::fixed << std::setprecision(4) 
           << metrics.recall_after * 100.0f << "%\n";
    report << "\n";
    
    report << "TRAINING METRICS:\n";
    report << "  Initial loss: " << std::fixed << std::setprecision(6) 
           << metrics.initial_loss << "\n";
    report << "  Final loss:   " << std::fixed << std::setprecision(6) 
           << metrics.final_loss << "\n";
    report << "  Epochs trained: " << metrics.epochs_trained << "\n";
    report << "  Training time: " << std::fixed << std::setprecision(2) 
           << metrics.training_time_seconds << " seconds\n";
    report << "\n";
    
    report << "STABILITY CHECK:\n";
    report << "  Status: " << (metrics.is_stable ? "? STABLE" : "? UNSTABLE") << "\n";
    report << "  Max performance drop: " << std::fixed << std::setprecision(4) 
           << metrics.max_performance_drop * 100.0f << "%\n";
    report << "  Details: " << metrics.stability_status << "\n";
    report << "\n";
    
    report << "================================================================================\n";
    report << "CONTINUOUS LEARNING COMPLETED SUCCESSFULLY\n";
    report << "================================================================================\n";
    report << "\n";
    report << "IMPLEMENTATION DETAILS:\n";
    report << "  ? Loaded existing trained models (Tier 1 & Tier 2)\n";
    report << "  ? Loaded new dataset with " << metrics.new_samples_count << " samples\n";
    report << "  ? Applied curriculum learning (prioritizing hard examples)\n";
    report << "  ? Created replay buffer with " << metrics.old_samples_count_used 
           << " historical samples\n";
    report << "  ? Fine-tuned models incrementally (" << metrics.epochs_trained 
           << " epochs)\n";
    report << "  ? Validated stability (catastrophic forgetting check)\n";
    report << "  ? Saved updated models to versioned directory\n";
    report << "\n";
    
    report << "KEY FINDINGS:\n";
    if (metrics.accuracy_change > 0.0f) {
        report << "  ? Performance IMPROVED by " << std::fixed << std::setprecision(2) 
               << metrics.accuracy_change * 100.0f << "%\n";
    } else if (metrics.accuracy_change < 0.0f) {
        report << "  ? Performance slightly decreased by " << std::fixed << std::setprecision(2) 
               << -metrics.accuracy_change * 100.0f << "%\n";
    } else {
        report << "  = Performance remained stable\n";
    }
    
    if (metrics.is_stable) {
        report << "  ? No catastrophic forgetting detected\n";
        report << "  ? Safe to deploy updated models in production\n";
    } else {
        report << "  ? WARNING: Potential performance drop detected\n";
        report << "  ? Recommend: Reduce learning rate and retrain\n";
    }
    
    report << "\n";
    report << "NEXT STEPS:\n";
    report << "  1. Integrate updated models into production system\n";
    report << "  2. Monitor performance on real-world data\n";
    report << "  3. Collect feedback for next incremental training cycle\n";
    report << "  4. Schedule periodic incremental training (e.g., weekly/monthly)\n";
    report << "\n";
    
    report << "================================================================================\n";
    report.close();
    
    std::cout << "Report saved to: " << filename << std::endl;
}

int main(int argc, char* argv[]) {
    std::cout << "\n" << std::string(80, '=') << std::endl;
    std::cout << "CONTINUOUS LEARNING PIPELINE FOR BULLET HOLE DETECTION SYSTEM" << std::endl;
    std::cout << std::string(80, '=') << std::endl;
    
    try {
        // ===== CONFIGURATION =====
        
        // Paths
        std::string new_data_path = "dataset_ir_realistic/annotations.csv";
        std::string scaler_path = "models/scaler.bin";
        std::string model_output_dir = "models";
        
        // Allow command-line override
        if (argc > 1) {
            new_data_path = argv[1];
        }
        if (argc > 2) {
            model_output_dir = argv[2];
        }
        
        // Create model directory if it doesn't exist
        system("if not exist models mkdir models");
        system("if not exist results mkdir results");
        
        // Training configuration
        IncrementalTrainingConfig config;
        config.learning_rate = 0.001f;
        config.max_epochs = 100;  // Increased for better convergence
        config.min_learning_rate = 0.0001f;
        config.use_curriculum = true;
        config.prioritize_hard_examples = true;
        config.replay_buffer_ratio = 0.20f;  // 20% old data, 80% new data
        config.new_data_ratio = 0.80f;
        config.max_replay_buffer_size = 2000;
        config.performance_drop_threshold = 0.05f;  // 5% threshold
        config.validate_on_old_data = true;
        
        std::cout << "\n[CONFIGURATION]" << std::endl;
        std::cout << "  New data path: " << new_data_path << std::endl;
        std::cout << "  Model directory: " << model_output_dir << std::endl;
        std::cout << "  Scaler path: " << scaler_path << std::endl;
        std::cout << "  Learning rate: " << std::fixed << std::setprecision(5) 
                  << config.learning_rate << std::endl;
        std::cout << "  Max epochs: " << config.max_epochs << std::endl;
        std::cout << "  Curriculum learning: " << (config.use_curriculum ? "ENABLED" : "DISABLED") << std::endl;
        std::cout << "  Prioritize hard examples: " 
                  << (config.prioritize_hard_examples ? "YES" : "NO") << std::endl;
        std::cout << "  Replay buffer ratio: " << std::fixed << std::setprecision(2) 
                  << (config.replay_buffer_ratio * 100.0f) << "% old data" << std::endl;
        
        // ===== PERFORM INCREMENTAL TRAINING =====
        
        IncrementalTrainer trainer;
        IncrementalTrainingMetrics metrics;
        
        bool success = trainer.performIncrementalTraining(
            new_data_path,
            scaler_path,
            model_output_dir,
            config,
            metrics
        );
        
        if (!success) {
            std::cerr << "ERROR: Incremental training failed" << std::endl;
            return 1;
        }
        
        // ===== PRINT RESULTS =====
        
        printMetrics(metrics);
        
        // ===== SAVE REPORT =====
        
        std::string report_filename = "results/INCREMENTAL_TRAINING_REPORT_" 
                                      + std::to_string(metrics.new_samples_count) 
                                      + "_SAMPLES.txt";
        saveReportToFile(metrics, report_filename);
        
        // ===== FINAL OUTPUT =====
        
        std::cout << "\n" << std::string(80, '=') << std::endl;
        std::cout << "? INCREMENTAL TRAINING COMPLETED" << std::endl;
        std::cout << std::string(80, '=') << std::endl;
        
        std::cout << "\n[SUMMARY]" << std::endl;
        std::cout << "  Samples used: " << metrics.total_training_samples << std::endl;
        std::cout << "  - New samples: " << metrics.new_samples_count << std::endl;
        std::cout << "  - Replay buffer: " << metrics.old_samples_count_used << std::endl;
        
        std::cout << "\n[PERFORMANCE]" << std::endl;
        std::cout << "  Before: " << std::fixed << std::setprecision(2) << metrics.initial_accuracy * 100.0f << "%" << std::endl;
        std::cout << "  After:  " << std::fixed << std::setprecision(2) << metrics.final_accuracy * 100.0f << "%" << std::endl;
        std::cout << "  Change: " << std::fixed << std::setprecision(2) << metrics.accuracy_change * 100.0f << "%" << std::endl;
        
        std::cout << "\n[STABILITY]" << std::endl;
        std::cout << "  Status: " << (metrics.is_stable ? "? PASSED" : "? FAILED") << std::endl;
        std::cout << "  Max drop: " << std::fixed << std::setprecision(2) << metrics.max_performance_drop * 100.0f << "%" << std::endl;
        std::cout << "  Details: " << metrics.stability_status << std::endl;
        
        std::cout << "\n[TIMING]" << std::endl;
        std::cout << "  Training time: " << std::fixed << std::setprecision(2) << metrics.training_time_seconds << "s" << std::endl;
        std::cout << "  Epochs: " << metrics.epochs_trained << std::endl;
        
        std::cout << "\n" << std::string(80, '=') << std::endl;
        std::cout << "Report saved to: " << report_filename << std::endl;
        std::cout << "Models saved to: " << model_output_dir << std::endl;
        std::cout << std::string(80, '=') << std::endl;
        std::cout << "\n";
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "FATAL ERROR: " << e.what() << std::endl;
        return 1;
    }
}
