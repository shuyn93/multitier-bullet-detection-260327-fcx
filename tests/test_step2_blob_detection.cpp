/**
 * STEP 2 TEST: Blob Detection with Small Object Validation
 * 
 * Validates:
 * 1. Multi-threshold detection catches bright blobs
 * 2. Small blobs (r < 10px) are detected and preserved
 * 3. Adaptive filtering works correctly
 * 4. Two-stage pipeline (generate + filter) works as designed
 */

#include <iostream>
#include <chrono>
#include <opencv2/opencv.hpp>
#include "candidate/ImprovedBlobDetectorStep2.h"
#include "candidate/ImprovedCandidateDetector.h"

using namespace bullet_detection;

// ===== TEST 1: Small Blob Detection =====
void testSmallBlobDetection() {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "TEST 1: Small Blob Detection (r < 10px)" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    
    // Create test image with small bright holes
    cv::Mat test_image = cv::Mat(256, 256, CV_8U, cv::Scalar(100));
    
    // Add small bullet holes (various sizes)
    // r=4px ? area ? 50
    cv::circle(test_image, cv::Point(50, 50), 4, cv::Scalar(240), -1);
    cv::circle(test_image, cv::Point(80, 50), 5, cv::Scalar(235), -1);
    cv::circle(test_image, cv::Point(110, 50), 6, cv::Scalar(230), -1);
    cv::circle(test_image, cv::Point(140, 50), 7, cv::Scalar(225), -1);
    cv::circle(test_image, cv::Point(170, 50), 8, cv::Scalar(220), -1);
    cv::circle(test_image, cv::Point(200, 50), 9, cv::Scalar(215), -1);
    
    // Add normal bullet holes for comparison
    cv::circle(test_image, cv::Point(128, 128), 15, cv::Scalar(240), -1);
    cv::circle(test_image, cv::Point(200, 150), 12, cv::Scalar(230), -1);
    
    // Add noise (false positives to filter)
    cv::randu(test_image, 80, 120);
    
    ImprovedBlobDetectorCpp detector(256);
    detector.setAdaptiveBlockSize(21);
    detector.setAdaptiveConstant(5);
    
    // Test small object detection
    auto start = std::chrono::high_resolution_clock::now();
    auto small_blobs = detector.detectSmallObjects(test_image);
    auto end = std::chrono::high_resolution_clock::now();
    
    float small_elapsed = std::chrono::duration<float, std::milli>(end - start).count();
    
    std::cout << "\n[SMALL BLOBS]" << std::endl;
    std::cout << "  Detected: " << small_blobs.size() << " small blobs" << std::endl;
    std::cout << "  Time: " << small_elapsed << " ms" << std::endl;
    
    if (small_blobs.size() >= 5) {
        std::cout << "  ? PASS: Small object detection working" << std::endl;
    } else {
        std::cout << "  ? FAIL: Expected ?5 small blobs, got " << small_blobs.size() << std::endl;
    }
    
    // Show details
    std::cout << "\n[DETAILS - Small Blobs]" << std::endl;
    for (size_t i = 0; i < std::min(size_t(6), small_blobs.size()); ++i) {
        double area = cv::contourArea(small_blobs[i].contour);
        double radius = std::sqrt(area / M_PI);
        std::cout << "    Blob " << i << ": area=" << area << " px², "
                  << "radius?" << radius << " px, "
                  << "circularity=" << small_blobs[i].circularity << ", "
                  << "contrast=" << small_blobs[i].intensity_contrast << std::endl;
    }
}

// ===== TEST 2: Multi-Threshold Detection =====
void testMultiThresholdDetection() {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "TEST 2: Multi-Threshold Detection (Normal + Small)" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    
    // Create test image with mixed brightness blobs
    cv::Mat test_image = cv::Mat(256, 256, CV_8U, cv::Scalar(80));
    
    // Bright blobs
    cv::circle(test_image, cv::Point(60, 60), 12, cv::Scalar(240), -1);
    
    // Medium brightness blobs (should be caught by lower thresholds)
    cv::circle(test_image, cv::Point(120, 120), 10, cv::Scalar(180), -1);
    
    // Small but bright blobs
    cv::circle(test_image, cv::Point(180, 180), 6, cv::Scalar(230), -1);
    cv::circle(test_image, cv::Point(200, 60), 5, cv::Scalar(220), -1);
    
    ImprovedBlobDetectorCpp detector(256);
    
    auto start = std::chrono::high_resolution_clock::now();
    auto all_blobs = detector.detectBlobs(test_image);
    auto end = std::chrono::high_resolution_clock::now();
    
    float elapsed = std::chrono::duration<float, std::milli>(end - start).count();
    
    std::cout << "\n[ALL BLOBS - Two Stage Pipeline]" << std::endl;
    std::cout << "  Detected: " << all_blobs.size() << " total blobs" << std::endl;
    std::cout << "  Time: " << elapsed << " ms" << std::endl;
    
    if (all_blobs.size() >= 4) {
        std::cout << "  ? PASS: Multi-threshold detection working" << std::endl;
    } else {
        std::cout << "  ? FAIL: Expected ?4 blobs, got " << all_blobs.size() << std::endl;
    }
    
    // Analyze size distribution
    std::cout << "\n[SIZE DISTRIBUTION]" << std::endl;
    int small_count = 0, normal_count = 0;
    for (const auto& blob : all_blobs) {
        double area = cv::contourArea(blob.contour);
        if (area < 314) small_count++;
        else normal_count++;
    }
    std::cout << "  Small blobs (r<10): " << small_count << std::endl;
    std::cout << "  Normal blobs (r?10): " << normal_count << std::endl;
}

// ===== TEST 3: Contrast Sensitivity =====
void testContrastSensitivity() {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "TEST 3: Contrast Sensitivity (Low/Medium/High Contrast)" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    
    ImprovedBlobDetectorCpp detector(256);
    
    // Create three test images with different contrasts
    std::vector<std::pair<std::string, cv::Mat>> test_cases = {
        {"Low Contrast", cv::Mat(256, 256, CV_8U, cv::Scalar(100))},
        {"Medium Contrast", cv::Mat(256, 256, CV_8U, cv::Scalar(80))},
        {"High Contrast", cv::Mat(256, 256, CV_8U, cv::Scalar(50))}
    };
    
    for (auto& test : test_cases) {
        // Add bright blobs
        cv::circle(test.second, cv::Point(128, 128), 8, cv::Scalar(230), -1);
        cv::circle(test.second, cv::Point(100, 100), 6, cv::Scalar(220), -1);
        cv::circle(test.second, cv::Point(150, 150), 5, cv::Scalar(210), -1);
        
        auto blobs = detector.detectBlobs(test.second);
        
        std::cout << "\n  [" << test.first << "]" << std::endl;
        std::cout << "    Background: " << static_cast<int>(test.second.at<uchar>(0, 0)) << std::endl;
        std::cout << "    Detected: " << blobs.size() << " blobs" << std::endl;
        
        if (blobs.size() >= 2) {
            std::cout << "    ? Good detection" << std::endl;
        } else {
            std::cout << "    ? Poor detection" << std::endl;
        }
    }
}

// ===== TEST 4: High-Resolution Optimization =====
void testHighResolutionOptimization() {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "TEST 4: High-Resolution Optimization (2480x2400)" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    
    // Create high-res test image (simulated)
    cv::Mat hires_image = cv::Mat(600, 600, CV_8U, cv::Scalar(90));  // Scaled-down simulation
    
    // Add small and large blobs
    for (int i = 0; i < 5; ++i) {
        cv::circle(hires_image, cv::Point(100 + i*80, 100), 4, cv::Scalar(240), -1);  // Small
        cv::circle(hires_image, cv::Point(100 + i*80, 300), 10, cv::Scalar(230), -1);  // Normal
    }
    
    ImprovedBlobDetectorCpp detector(600);
    
    // Test with downscaling
    auto start = std::chrono::high_resolution_clock::now();
    auto blobs_scaled = detector.detectBlobsHighRes(hires_image, 0.5f);
    auto end = std::chrono::high_resolution_clock::now();
    
    float elapsed = std::chrono::duration<float, std::milli>(end - start).count();
    
    std::cout << "\n[HIGH-RES DETECTION (Downscaled)]" << std::endl;
    std::cout << "  Input size: " << hires_image.size() << " px" << std::endl;
    std::cout << "  Scale factor: 0.5x" << std::endl;
    std::cout << "  Detected: " << blobs_scaled.size() << " blobs" << std::endl;
    std::cout << "  Time: " << elapsed << " ms" << std::endl;
    
    if (elapsed < 100) {
        std::cout << "  ? PASS: Processing time acceptable" << std::endl;
    } else {
        std::cout << "  ? WARNING: Processing time high" << std::endl;
    }
}

// ===== TEST 5: ImprovedCandidateDetector Integration =====
void testImprovedCandidateDetector() {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "TEST 5: ImprovedCandidateDetector Integration" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    
    ImprovedCandidateDetector detector;
    
    // Create test image
    cv::Mat test_image = cv::Mat(256, 256, CV_8U, cv::Scalar(100));
    
    // Add test blobs
    cv::circle(test_image, cv::Point(128, 128), 12, cv::Scalar(240), -1);
    cv::circle(test_image, cv::Point(80, 80), 8, cv::Scalar(230), -1);
    cv::circle(test_image, cv::Point(180, 180), 5, cv::Scalar(220), -1);
    
    auto start = std::chrono::high_resolution_clock::now();
    auto candidates = detector.detectCandidates(test_image, 0);
    auto end = std::chrono::high_resolution_clock::now();
    
    float elapsed = std::chrono::duration<float, std::milli>(end - start).count();
    
    std::cout << "\n[ImprovedCandidateDetector]" << std::endl;
    std::cout << "  Detected: " << candidates.size() << " candidates" << std::endl;
    std::cout << "  Time: " << elapsed << " ms" << std::endl;
    
    if (candidates.size() >= 2) {
        std::cout << "  ? PASS: Detection working" << std::endl;
        
        std::cout << "\n[CANDIDATE DETAILS]" << std::endl;
        for (size_t i = 0; i < std::min(size_t(3), candidates.size()); ++i) {
            double area = cv::contourArea(candidates[i].contour);
            double radius = std::sqrt(area / M_PI);
            std::cout << "    Candidate " << i << ": "
                      << "area=" << area << " px², "
                      << "radius?" << radius << " px, "
                      << "score=" << candidates[i].detection_score << std::endl;
        }
    } else {
        std::cout << "  ? FAIL: Expected ?2 candidates, got " << candidates.size() << std::endl;
    }
}

// ===== MAIN TEST RUNNER =====
int main() {
    std::cout << "\n" << std::string(60, '*') << std::endl;
    std::cout << "*  STEP 2: BLOB DETECTION VALIDATION TESTS  *" << std::endl;
    std::cout << std::string(60, '*') << std::endl;
    
    try {
        testSmallBlobDetection();
        testMultiThresholdDetection();
        testContrastSensitivity();
        testHighResolutionOptimization();
        testImprovedCandidateDetector();
        
        std::cout << "\n" << std::string(60, '=') << std::endl;
        std::cout << "ALL TESTS COMPLETED" << std::endl;
        std::cout << std::string(60, '=') << std::endl;
        
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "\n? Exception: " << e.what() << std::endl;
        return 1;
    }
}
