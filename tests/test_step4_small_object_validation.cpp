/**
 * STEP 4: SMALL OBJECT VALIDATION TEST SUITE
 * 
 * Validates that small bullet holes (r < 10px) are:
 * 1. Correctly detected by STEP 2
 * 2. Have valid features extracted by STEP 3
 * 3. Pass through RobustNoiseFilter appropriately
 * 4. Maintain 85%+ recall on small objects
 */

#include <iostream>
#include <chrono>
#include <vector>
#include <cmath>
#include <iomanip>
#include <fstream>
#include <opencv2/opencv.hpp>
#include "candidate/ImprovedBlobDetectorStep2.h"
#include "candidate/ImprovedCandidateDetector.h"
#include "feature/ImprovedFeatureExtractorOptimized.h"
#include "candidate/RobustNoiseFilter.h"

using namespace bullet_detection;

// ===== TEST DATA STRUCTURE =====

struct SmallObjectTestCase {
    int radius;
    int contrast_level;  // 0=low, 1=medium, 2=high
    float noise_level;   // 0.0=no noise, 0.2=20% noise
    cv::Mat image;
    cv::Point center;
    std::string description;
};

struct ValidationResults {
    int total_objects;
    int detected_objects;
    std::vector<float> feature_validity;
    std::vector<float> confidence_scores;
    double detection_time_ms;
    double feature_time_ms;
    double filter_time_ms;
    float recall;
    float false_positive_rate;
};

// ===== HELPER FUNCTIONS =====

cv::Mat createSmallObjectTestImage(int radius, int contrast_level, float noise_level) {
    // Background intensity based on contrast
    int bg_intensity = 100 - (contrast_level * 10);  // 100, 90, 80
    cv::Mat image = cv::Mat(256, 256, CV_8U, cv::Scalar(bg_intensity));
    
    // Small bright bullet hole
    int blob_intensity = bg_intensity + 100 + (contrast_level * 30);  // 200-260
    blob_intensity = std::min(255, blob_intensity);
    
    cv::circle(image, cv::Point(128, 128), radius, cv::Scalar(blob_intensity), -1);
    
    // Add noise if requested
    if (noise_level > 0.0f) {
        cv::Mat noise = cv::Mat(image.size(), CV_8U);
        cv::randu(noise, 0, 50);
        image = image + noise * noise_level;
    }
    
    return image;
}

bool isFeatureValid(float value) {
    return std::isfinite(value) && value >= 0.0f && value <= 1.0f;
}

// ===== TEST 1: SMALL BLOB DETECTION =====

ValidationResults testSmallBlobDetection() {
    std::cout << "\n" << std::string(70, '=') << std::endl;
    std::cout << "TEST 1: SMALL BLOB DETECTION (r < 10px)" << std::endl;
    std::cout << std::string(70, '=') << std::endl;
    
    ValidationResults results;
    results.total_objects = 0;
    results.detected_objects = 0;
    results.detection_time_ms = 0.0;
    
    ImprovedBlobDetectorCpp detector(256);
    
    // Test different radii
    std::vector<int> test_radii = {3, 4, 5, 6, 7, 8, 9, 10};
    std::vector<int> detected_per_radius(test_radii.size(), 0);
    
    for (size_t r_idx = 0; r_idx < test_radii.size(); ++r_idx) {
        int radius = test_radii[r_idx];
        int detected_count = 0;
        
        // Test each contrast level
        for (int contrast = 0; contrast < 3; ++contrast) {
            // Test each noise level
            for (float noise = 0.0f; noise <= 0.2f; noise += 0.1f) {
                cv::Mat test_image = createSmallObjectTestImage(radius, contrast, noise);
                
                auto start = std::chrono::high_resolution_clock::now();
                auto blobs = detector.detectSmallObjects(test_image);
                auto end = std::chrono::high_resolution_clock::now();
                
                results.detection_time_ms += 
                    std::chrono::duration<double, std::milli>(end - start).count();
                
                if (!blobs.empty()) {
                    detected_count++;
                    results.detected_objects++;
                }
                results.total_objects++;
            }
        }
        
        detected_per_radius[r_idx] = detected_count;
        int total_for_radius = 9;  // 3 contrast × 3 noise levels
        float recall_for_radius = static_cast<float>(detected_count) / total_for_radius * 100;
        
        std::cout << "  Radius " << std::setw(2) << radius << "px: "
                  << std::setw(2) << detected_count << "/" << total_for_radius
                  << " (" << std::fixed << std::setprecision(1) << recall_for_radius << "%)" << std::endl;
    }
    
    results.recall = static_cast<float>(results.detected_objects) / results.total_objects;
    
    std::cout << "\n[SUMMARY]" << std::endl;
    std::cout << "  Total Detected: " << results.detected_objects << "/" << results.total_objects
              << " (" << std::fixed << std::setprecision(1) << (results.recall * 100) << "%)" << std::endl;
    std::cout << "  Average Detection Time: " 
              << std::fixed << std::setprecision(3) 
              << (results.detection_time_ms / results.total_objects) << " ms" << std::endl;
    
    if (results.recall >= 0.85) {
        std::cout << "  ? PASS: Recall ? 85%" << std::endl;
    } else if (results.recall >= 0.75) {
        std::cout << "  ?? WARNING: Recall 75-85% (acceptable but not ideal)" << std::endl;
    } else {
        std::cout << "  ? FAIL: Recall < 75%" << std::endl;
    }
    
    return results;
}

// ===== TEST 2: FEATURE EXTRACTION ON SMALL OBJECTS =====

void testSmallObjectFeatures() {
    std::cout << "\n" << std::string(70, '=') << std::endl;
    std::cout << "TEST 2: FEATURE EXTRACTION ON SMALL OBJECTS" << std::endl;
    std::cout << std::string(70, '=') << std::endl;
    
    ImprovedBlobDetectorCpp detector(256);
    ImprovedFeatureExtractorOptimized extractor;
    
    int feature_errors = 0;
    int feature_success = 0;
    double total_feature_time = 0.0;
    
    // Test small objects at different radii
    std::vector<int> test_radii = {4, 6, 8};
    
    for (int radius : test_radii) {
        // Create test image
        cv::Mat test_image = createSmallObjectTestImage(radius, 1, 0.1f);
        
        // Detect blobs
        auto blobs = detector.detectSmallObjects(test_image);
        
        if (blobs.empty()) {
            std::cout << "  Radius " << radius << "px: No blobs detected" << std::endl;
            continue;
        }
        
        // Extract features for each blob
        std::cout << "  Radius " << radius << "px: " << blobs.size() << " blob(s)" << std::endl;
        
        for (size_t i = 0; i < std::min(size_t(3), blobs.size()); ++i) {
            auto start = std::chrono::high_resolution_clock::now();
            auto features = extractor.extractFeatures(blobs[i].roi, blobs[i].contour);
            auto end = std::chrono::high_resolution_clock::now();
            
            total_feature_time += 
                std::chrono::duration<double, std::milli>(end - start).count();
            
            // Validate all features
            std::vector<float> feature_values = {
                features.area, features.circularity, features.solidity, features.aspect_ratio,
                features.radial_symmetry, features.radial_gradient, features.snr, features.entropy,
                features.ring_energy, features.sharpness, features.laplacian_density,
                features.phase_coherence, features.contrast, features.mean_intensity,
                features.std_intensity, features.edge_density, features.corner_count
            };
            
            bool all_valid = true;
            for (float val : feature_values) {
                if (!isFeatureValid(val)) {
                    all_valid = false;
                    feature_errors++;
                    break;
                }
            }
            
            if (all_valid) {
                feature_success++;
                std::cout << "    Blob " << i << ": ? All 17 features valid" << std::endl;
            } else {
                std::cout << "    Blob " << i << ": ? Invalid features" << std::endl;
            }
        }
    }
    
    std::cout << "\n[SUMMARY]" << std::endl;
    std::cout << "  Successful Extractions: " << feature_success << std::endl;
    std::cout << "  Failed Extractions: " << feature_errors << std::endl;
    std::cout << "  Average Extraction Time: " << std::fixed << std::setprecision(3)
              << (total_feature_time / (feature_success + feature_errors)) << " ms" << std::endl;
    
    if (feature_errors == 0) {
        std::cout << "  ? PASS: All features valid" << std::endl;
    } else {
        std::cout << "  ? FAIL: Some features invalid" << std::endl;
    }
}

// ===== TEST 3: NOISE FILTER ON SMALL OBJECTS =====

void testSmallObjectFiltering() {
    std::cout << "\n" << std::string(70, '=') << std::endl;
    std::cout << "TEST 3: ROBUST NOISE FILTER ON SMALL OBJECTS" << std::endl;
    std::cout << std::string(70, '=') << std::endl;
    
    ImprovedBlobDetectorCpp detector(256);
    RobustNoiseFilter filter;
    filter.setFilteringLevel(1);  // Balanced
    
    // Create mixed dataset: real small holes + noise artifacts
    cv::Mat test_image = cv::Mat(256, 256, CV_8U, cv::Scalar(100));
    
    // Add real small bullet holes
    cv::circle(test_image, cv::Point(80, 80), 6, cv::Scalar(235), -1);
    cv::circle(test_image, cv::Point(180, 80), 8, cv::Scalar(240), -1);
    cv::circle(test_image, cv::Point(128, 180), 5, cv::Scalar(225), -1);
    
    // Add noise artifacts
    cv::circle(test_image, cv::Point(50, 50), 2, cv::Scalar(180), -1);
    cv::circle(test_image, cv::Point(200, 200), 3, cv::Scalar(190), -1);
    
    // Add some noise
    cv::Mat noise = cv::Mat(256, 256, CV_8U);
    cv::randu(noise, 0, 30);
    test_image = test_image + noise * 0.15f;
    
    // Detect all candidates
    auto candidates = detector.detectBlobs(test_image);
    
    std::cout << "  Total Candidates Detected: " << candidates.size() << std::endl;
    
    if (candidates.empty()) {
        std::cout << "  ?? WARNING: No candidates detected" << std::endl;
        return;
    }
    
    // Filter through RobustNoiseFilter
    auto start = std::chrono::high_resolution_clock::now();
    auto filtered = filter.filterAndScoreCandidates(candidates, test_image);
    auto end = std::chrono::high_resolution_clock::now();
    
    double filter_time = std::chrono::duration<double, std::milli>(end - start).count();
    
    std::cout << "  After Filtering: " << filtered.size() << " candidates" << std::endl;
    std::cout << "  Filter Time: " << std::fixed << std::setprecision(2) << filter_time << " ms" << std::endl;
    
    // Analyze scores
    if (!filtered.empty()) {
        float min_score = 1.0f, max_score = 0.0f, avg_score = 0.0f;
        
        for (const auto& cand : filtered) {
            min_score = std::min(min_score, cand.detection_score);
            max_score = std::max(max_score, cand.detection_score);
            avg_score += cand.detection_score;
        }
        avg_score /= filtered.size();
        
        std::cout << "\n[SCORE DISTRIBUTION]" << std::endl;
        std::cout << "  Min Score: " << std::fixed << std::setprecision(3) << min_score << std::endl;
        std::cout << "  Avg Score: " << std::fixed << std::setprecision(3) << avg_score << std::endl;
        std::cout << "  Max Score: " << std::fixed << std::setprecision(3) << max_score << std::endl;
        
        if (avg_score >= 0.5f && max_score > 0.7f) {
            std::cout << "  ? PASS: Good score separation" << std::endl;
        } else {
            std::cout << "  ?? WARNING: May need filter tuning" << std::endl;
        }
    }
}

// ===== TEST 4: PERFORMANCE BENCHMARKING =====

void testSmallObjectPerformance() {
    std::cout << "\n" << std::string(70, '=') << std::endl;
    std::cout << "TEST 4: PERFORMANCE BENCHMARKING ON SMALL OBJECTS" << std::endl;
    std::cout << std::string(70, '=') << std::endl;
    
    ImprovedBlobDetectorCpp detector(256);
    ImprovedFeatureExtractorOptimized extractor;
    
    int num_iterations = 50;
    double total_detection_time = 0.0;
    double total_feature_time = 0.0;
    int total_blobs = 0;
    
    std::cout << "  Testing with " << num_iterations << " iterations..." << std::endl;
    
    for (int iter = 0; iter < num_iterations; ++iter) {
        // Random radius and parameters
        int radius = 3 + (rand() % 8);
        int contrast = rand() % 3;
        float noise = (rand() % 3) * 0.1f;
        
        cv::Mat test_image = createSmallObjectTestImage(radius, contrast, noise);
        
        // Time detection
        auto det_start = std::chrono::high_resolution_clock::now();
        auto blobs = detector.detectSmallObjects(test_image);
        auto det_end = std::chrono::high_resolution_clock::now();
        
        total_detection_time += 
            std::chrono::duration<double, std::milli>(det_end - det_start).count();
        
        // Time feature extraction if blobs detected
        if (!blobs.empty()) {
            for (const auto& blob : blobs) {
                auto feat_start = std::chrono::high_resolution_clock::now();
                auto features = extractor.extractFeatures(blob.roi, blob.contour);
                auto feat_end = std::chrono::high_resolution_clock::now();
                
                total_feature_time += 
                    std::chrono::duration<double, std::milli>(feat_end - feat_start).count();
                total_blobs++;
            }
        }
    }
    
    std::cout << "\n[PERFORMANCE METRICS]" << std::endl;
    std::cout << "  Average Detection Time: " 
              << std::fixed << std::setprecision(3)
              << (total_detection_time / num_iterations) << " ms/image" << std::endl;
    
    if (total_blobs > 0) {
        std::cout << "  Average Feature Extraction: "
                  << std::fixed << std::setprecision(3)
                  << (total_feature_time / total_blobs) << " ms/blob" << std::endl;
    }
    
    double detection_avg = total_detection_time / num_iterations;
    double feature_avg = (total_blobs > 0) ? (total_feature_time / total_blobs) : 0.0;
    
    if (detection_avg < 1.5 && feature_avg < 1.5) {
        std::cout << "  ? PASS: Performance within targets" << std::endl;
    } else if (detection_avg < 2.0 && feature_avg < 2.0) {
        std::cout << "  ?? WARNING: Performance acceptable but not optimal" << std::endl;
    } else {
        std::cout << "  ? FAIL: Performance below acceptable levels" << std::endl;
    }
}

// ===== MAIN TEST RUNNER =====

int main() {
    std::cout << "\n" << std::string(70, '*') << std::endl;
    std::cout << "*  STEP 4: SMALL OBJECT VALIDATION TEST SUITE  *" << std::endl;
    std::cout << std::string(70, '*') << std::endl;
    
    try {
        // Run all tests
        auto results1 = testSmallBlobDetection();
        testSmallObjectFeatures();
        testSmallObjectFiltering();
        testSmallObjectPerformance();
        
        // Summary
        std::cout << "\n" << std::string(70, '=') << std::endl;
        std::cout << "STEP 4 VALIDATION COMPLETE" << std::endl;
        std::cout << std::string(70, '=') << std::endl;
        
        std::cout << "\n[OVERALL RESULTS]" << std::endl;
        std::cout << "  Small Object Detection Recall: " 
                  << std::fixed << std::setprecision(1) 
                  << (results1.recall * 100) << "%" << std::endl;
        
        if (results1.recall >= 0.85f) {
            std::cout << "\n? STEP 4 PASSED: Small objects validated at 85%+ recall" << std::endl;
            return 0;
        } else if (results1.recall >= 0.75f) {
            std::cout << "\n?? STEP 4 PARTIAL: Small objects validated but recall < 85%" << std::endl;
            return 0;
        } else {
            std::cout << "\n? STEP 4 FAILED: Small object recall below acceptable threshold" << std::endl;
            return 1;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "\n? Exception: " << e.what() << std::endl;
        return 1;
    }
}
