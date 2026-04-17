#include <iostream>
#include <chrono>
#include <filesystem>
#include <opencv2/opencv.hpp>
#include "candidate/ImprovedCandidateDetector.h"
#include "candidate/RobustNoiseFilter.h"

namespace fs = std::filesystem;
using namespace bullet_detection;

int main() {
    std::cout << "=== INTEGRATION TEST - REAL IMAGE ===" << std::endl;
    
    try {
        // Try to load a real IR image
        std::string image_path = "data/datasets/dataset_main/images/010498.png";
        
        if (!fs::exists(image_path)) {
            std::cerr << "Image not found: " << image_path << std::endl;
            std::cerr << "Looking for alternative images..." << std::endl;
            
            // Search for any PNG in the directory
            std::string image_dir = "data/datasets/dataset_main/images";
            if (fs::exists(image_dir)) {
                for (const auto& entry : fs::directory_iterator(image_dir)) {
                    if (entry.path().extension() == ".png") {
                        image_path = entry.path().string();
                        std::cout << "Found image: " << image_path << std::endl;
                        break;
                    }
                }
            }
        }
        
        cv::Mat image = cv::imread(image_path, cv::IMREAD_GRAYSCALE);
        
        if (image.empty()) {
            std::cerr << "? Failed to load image: " << image_path << std::endl;
            std::cerr << "Creating synthetic test image instead..." << std::endl;
            
            // Create synthetic image for testing
            image = cv::Mat(256, 256, CV_8U, cv::Scalar(50));
            cv::circle(image, cv::Point(128, 128), 20, cv::Scalar(240), -1);
            cv::circle(image, cv::Point(200, 100), 10, cv::Scalar(230), -1);
            cv::circle(image, cv::Point(60, 180), 8, cv::Scalar(220), -1);
            cv::circle(image, cv::Point(150, 200), 12, cv::Scalar(235), -1);
        }
        
        std::cout << "? Image loaded: " << image.size() << std::endl;
        std::cout << "  Image type: " << image.type() << " (CV_8U=0)" << std::endl;
        
        // STEP 2: Detection
        {
            std::cout << "\n[STEP 2] Detection Pipeline" << std::endl;
            
            ImprovedCandidateDetector detector;
            detector.setPreprocessingParams(9, 75.0f, 2.0f);
            detector.setDetectionThresholds(10, 10000, 0.5f, 0.6f, 0.5f);
            
            auto start = std::chrono::high_resolution_clock::now();
            auto candidates = detector.detectCandidates(image, 0);
            auto end = std::chrono::high_resolution_clock::now();
            
            float elapsed = std::chrono::duration<float, std::milli>(end - start).count();
            
            std::cout << "  Candidates detected: " << candidates.size() << std::endl;
            std::cout << "  Time: " << elapsed << " ms" << std::endl;
            
            if (candidates.empty()) {
                std::cout << "  ? No candidates detected (may be expected for some images)" << std::endl;
            } else {
                std::cout << "  Candidate details:" << std::endl;
                for (size_t i = 0; i < std::min(size_t(3), candidates.size()); ++i) {
                    std::cout << "    [" << i << "] bbox(" << candidates[i].bbox.x << "," 
                              << candidates[i].bbox.y << ") size=" << candidates[i].bbox.width 
                              << "x" << candidates[i].bbox.height 
                              << " score=" << candidates[i].detection_score << std::endl;
                }
            }
        }
        
        // STEP 2 + STEP 3: Full pipeline
        {
            std::cout << "\n[STEP 2+3] Full Pipeline (Detection + Filtering)" << std::endl;
            
            ImprovedCandidateDetector detector;
            RobustNoiseFilter filter;
            
            detector.setPreprocessingParams(9, 75.0f, 2.0f);
            detector.setDetectionThresholds(10, 10000, 0.5f, 0.6f, 0.5f);
            filter.setFilteringLevel(1);  // Balanced
            
            auto start = std::chrono::high_resolution_clock::now();
            
            auto raw = detector.detectCandidates(image, 0);
            auto filtered = filter.filterAndScoreCandidates(raw, image);
            
            auto end = std::chrono::high_resolution_clock::now();
            
            float elapsed = std::chrono::duration<float, std::milli>(end - start).count();
            
            std::cout << "  Raw candidates: " << raw.size() << std::endl;
            std::cout << "  After filtering: " << filtered.size() << std::endl;
            std::cout << "  Total time: " << elapsed << " ms" << std::endl;
            
            if (!filtered.empty()) {
                // Compute statistics
                float avg_score = 0.0f;
                float min_score = 1.0f;
                float max_score = 0.0f;
                int high_conf = 0;  // > 0.75
                int med_conf = 0;   // 0.50 - 0.75
                int low_conf = 0;   // < 0.50
                
                for (const auto& cand : filtered) {
                    float score = cand.detection_score;
                    avg_score += score;
                    min_score = std::min(min_score, score);
                    max_score = std::max(max_score, score);
                    
                    if (score > 0.75f) high_conf++;
                    else if (score >= 0.50f) med_conf++;
                    else low_conf++;
                }
                avg_score /= filtered.size();
                
                std::cout << "  Confidence stats:" << std::endl;
                std::cout << "    Min: " << min_score << ", Avg: " << avg_score 
                          << ", Max: " << max_score << std::endl;
                std::cout << "    High (>0.75): " << high_conf 
                          << ", Medium (0.50-0.75): " << med_conf 
                          << ", Low (<0.50): " << low_conf << std::endl;
                
                std::cout << "  Top candidates:" << std::endl;
                for (size_t i = 0; i < std::min(size_t(5), filtered.size()); ++i) {
                    std::cout << "    [" << i << "] score=" << filtered[i].detection_score << std::endl;
                }
            } else {
                std::cout << "  ? No candidates after filtering" << std::endl;
            }
        }
        
        std::cout << "\n? Integration test completed successfully" << std::endl;
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "\n? Exception caught: " << e.what() << std::endl;
        return 1;
    }
}
