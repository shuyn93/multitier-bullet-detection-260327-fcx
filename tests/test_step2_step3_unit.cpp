#include <iostream>
#include <chrono>
#include <opencv2/opencv.hpp>
#include "candidate/ImprovedCandidateDetector.h"
#include "candidate/RobustNoiseFilter.h"

using namespace bullet_detection;

int main() {
    std::cout << "=== STEP 2 & 3 UNIT TEST ===" << std::endl;
    
    try {
        // Test STEP 2: ImprovedCandidateDetector
        {
            std::cout << "\n[TEST] STEP 2: ImprovedCandidateDetector" << std::endl;
            
            ImprovedCandidateDetector detector;
            
            // Create dummy test image (256x256 grayscale)
            cv::Mat test_image = cv::Mat(256, 256, CV_8U, cv::Scalar(100));
            
            // Add synthetic bullet hole (bright blob)
            cv::circle(test_image, cv::Point(128, 128), 15, cv::Scalar(240), -1);
            cv::circle(test_image, cv::Point(200, 100), 8, cv::Scalar(230), -1);
            
            // Test detection
            auto start = std::chrono::high_resolution_clock::now();
            auto candidates = detector.detectCandidates(test_image, 0);
            auto end = std::chrono::high_resolution_clock::now();
            
            float elapsed = std::chrono::duration<float, std::milli>(end - start).count();
            
            std::cout << "  Candidates detected: " << candidates.size() << std::endl;
            std::cout << "  Time: " << elapsed << " ms" << std::endl;
            
            if (candidates.size() > 0) {
                std::cout << "  ? STEP 2 works - candidates found" << std::endl;
                
                // Display some details
                for (size_t i = 0; i < std::min(size_t(3), candidates.size()); ++i) {
                    std::cout << "    Candidate " << i << ": "
                              << "bbox=(" << candidates[i].bbox.x << "," << candidates[i].bbox.y << ") "
                              << "size=" << candidates[i].bbox.width << "x" << candidates[i].bbox.height
                              << " score=" << candidates[i].detection_score << std::endl;
                }
            } else {
                std::cerr << "  ? WARNING: STEP 2 detected no candidates" << std::endl;
            }
        }
        
        // Test STEP 3: RobustNoiseFilter
        {
            std::cout << "\n[TEST] STEP 3: RobustNoiseFilter" << std::endl;
            
            RobustNoiseFilter filter;
            filter.setFilteringLevel(1);  // Balanced
            
            ImprovedCandidateDetector detector;
            
            // Create test image with multiple blobs
            cv::Mat test_image = cv::Mat(256, 256, CV_8U, cv::Scalar(100));
            cv::circle(test_image, cv::Point(128, 128), 15, cv::Scalar(240), -1);
            cv::circle(test_image, cv::Point(200, 100), 8, cv::Scalar(230), -1);
            // Add noise blob
            cv::circle(test_image, cv::Point(50, 50), 3, cv::Scalar(180), -1);
            
            auto start = std::chrono::high_resolution_clock::now();
            auto raw = detector.detectCandidates(test_image, 0);
            auto filtered = filter.filterAndScoreCandidates(raw, test_image);
            auto end = std::chrono::high_resolution_clock::now();
            
            float elapsed = std::chrono::duration<float, std::milli>(end - start).count();
            
            std::cout << "  Raw candidates: " << raw.size() << std::endl;
            std::cout << "  Filtered candidates: " << filtered.size() << std::endl;
            std::cout << "  Total time: " << elapsed << " ms" << std::endl;
            
            if (filtered.size() > 0) {
                // Check confidence scores
                float avg_score = 0.0f;
                float min_score = 1.0f, max_score = 0.0f;
                
                for (const auto& cand : filtered) {
                    avg_score += cand.detection_score;
                    min_score = std::min(min_score, cand.detection_score);
                    max_score = std::max(max_score, cand.detection_score);
                }
                avg_score /= filtered.size();
                
                std::cout << "  Score stats - Min: " << min_score 
                          << ", Avg: " << avg_score 
                          << ", Max: " << max_score << std::endl;
                
                if (avg_score >= 0.5f && avg_score <= 1.0f) {
                    std::cout << "  ? STEP 3 works - confidence scores in valid range" << std::endl;
                } else {
                    std::cerr << "  ? WARNING: Confidence scores out of expected range" << std::endl;
                }
                
                std::cout << "  Sample scores:" << std::endl;
                for (size_t i = 0; i < std::min(size_t(5), filtered.size()); ++i) {
                    std::cout << "    Candidate " << i << ": " << filtered[i].detection_score << std::endl;
                }
            } else {
                std::cerr << "  ? WARNING: No candidates after filtering" << std::endl;
            }
        }
        
        std::cout << "\n=== ALL TESTS COMPLETED ===" << std::endl;
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "\n? Exception caught: " << e.what() << std::endl;
        return 1;
    }
}
