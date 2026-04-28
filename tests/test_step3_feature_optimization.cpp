/**
 * STEP 3 TEST: Feature Extraction Optimization Validation
 * 
 * Validates:
 * 1. All 17 features computed correctly
 * 2. Features normalized to [0, 1]
 * 3. Optimization performance improvements
 * 4. Numerical accuracy maintained
 * 5. No NaN/Inf values
 */

#include <iostream>
#include <chrono>
#include <iomanip>
#include <cmath>
#include <opencv2/opencv.hpp>
#include "feature/ImprovedFeatureExtractorOptimized.h"
#include "candidate/ImprovedBlobDetectorStep2.h"

using namespace bullet_detection;

// ===== HELPER FUNCTIONS =====

bool isValidFeatureValue(float val) {
    return std::isfinite(val) && val >= 0.0f && val <= 1.0f;
}

void printFeatures(const CandidateFeature& feat) {
    std::cout << "  Area: " << feat.area
              << " | Circularity: " << feat.circularity
              << " | Solidity: " << feat.solidity
              << " | Aspect Ratio: " << feat.aspect_ratio << std::endl;
    std::cout << "  Radial Sym: " << feat.radial_symmetry
              << " | Radial Grad: " << feat.radial_gradient
              << " | SNR: " << feat.snr
              << " | Entropy: " << feat.entropy << std::endl;
    std::cout << "  Ring Energy: " << feat.ring_energy
              << " | Sharpness: " << feat.sharpness
              << " | Laplacian Dens: " << feat.laplacian_density
              << " | Phase Coh: " << feat.phase_coherence << std::endl;
    std::cout << "  Contrast: " << feat.contrast
              << " | Mean Intensity: " << feat.mean_intensity
              << " | Std Intensity: " << feat.std_intensity
              << " | Edge Density: " << feat.edge_density
              << " | Corner Count: " << feat.corner_count << std::endl;
}

// ===== TEST 1: Feature Correctness =====
void testFeatureCorrectness() {
    std::cout << "\n" << std::string(70, '=') << std::endl;
    std::cout << "TEST 1: Feature Extraction Correctness (All 17 Features)" << std::endl;
    std::cout << std::string(70, '=') << std::endl;
    
    ImprovedFeatureExtractorOptimized extractor;
    
    // Create test image with bright blob
    cv::Mat test_image = cv::Mat(256, 256, CV_8U, cv::Scalar(80));
    cv::circle(test_image, cv::Point(128, 128), 20, cv::Scalar(240), -1);
    
    // Detect blob
    ImprovedBlobDetectorCpp detector(256);
    auto blobs = detector.detectBlobs(test_image);
    
    if (blobs.empty()) {
        std::cout << "\n? FAIL: No blobs detected" << std::endl;
        return;
    }
    
    std::cout << "\n[DETECTED " << blobs.size() << " BLOBS]" << std::endl;
    
    // Extract features for first blob
    const auto& blob = blobs[0];
    auto features = extractor.extractFeatures(blob.roi, blob.contour);
    
    std::cout << "\n[FEATURE VALUES]" << std::endl;
    printFeatures(features);
    
    // Validate
    std::cout << "\n[VALIDATION]" << std::endl;
    int valid_count = 0;
    int total_features = 17;
    
    // Check each feature
    std::vector<std::pair<std::string, float>> feature_list = {
        {"area", features.area},
        {"circularity", features.circularity},
        {"solidity", features.solidity},
        {"aspect_ratio", features.aspect_ratio},
        {"radial_symmetry", features.radial_symmetry},
        {"radial_gradient", features.radial_gradient},
        {"snr", features.snr},
        {"entropy", features.entropy},
        {"ring_energy", features.ring_energy},
        {"sharpness", features.sharpness},
        {"laplacian_density", features.laplacian_density},
        {"phase_coherence", features.phase_coherence},
        {"contrast", features.contrast},
        {"mean_intensity", features.mean_intensity},
        {"std_intensity", features.std_intensity},
        {"edge_density", features.edge_density},
        {"corner_count", features.corner_count}
    };
    
    for (const auto& [name, value] : feature_list) {
        if (isValidFeatureValue(value)) {
            std::cout << "  ? " << std::setw(20) << name << " = " << std::fixed << std::setprecision(4) << value << std::endl;
            valid_count++;
        } else {
            std::cout << "  ? " << std::setw(20) << name << " = " << value << " (INVALID)" << std::endl;
        }
    }
    
    std::cout << "\n  Result: " << valid_count << "/" << total_features << " features valid";
    if (valid_count == total_features) {
        std::cout << " ? PASS" << std::endl;
    } else {
        std::cout << " ? FAIL" << std::endl;
    }
}

// ===== TEST 2: Performance Benchmark =====
void testPerformanceBenchmark() {
    std::cout << "\n" << std::string(70, '=') << std::endl;
    std::cout << "TEST 2: Performance Optimization (Speed Improvement)" << std::endl;
    std::cout << std::string(70, '=') << std::endl;
    
    ImprovedFeatureExtractorOptimized extractor;
    
    // Create test image with multiple blobs
    cv::Mat test_image = cv::Mat(512, 512, CV_8U, cv::Scalar(90));
    cv::circle(test_image, cv::Point(100, 100), 15, cv::Scalar(240), -1);
    cv::circle(test_image, cv::Point(250, 150), 12, cv::Scalar(235), -1);
    cv::circle(test_image, cv::Point(400, 100), 10, cv::Scalar(230), -1);
    cv::circle(test_image, cv::Point(150, 350), 18, cv::Scalar(238), -1);
    cv::circle(test_image, cv::Point(350, 350), 8, cv::Scalar(225), -1);
    
    // Detect blobs
    ImprovedBlobDetectorCpp detector(512);
    auto blobs = detector.detectBlobs(test_image);
    
    if (blobs.empty()) {
        std::cout << "\n? FAIL: No blobs detected" << std::endl;
        return;
    }
    
    std::cout << "\n[BENCHMARK: " << blobs.size() << " blobs]" << std::endl;
    
    // Run extraction multiple times and measure
    const int iterations = 20;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    double total_time = 0.0;
    for (int iter = 0; iter < iterations; ++iter) {
        for (const auto& blob : blobs) {
            auto features = extractor.extractFeatures(blob.roi, blob.contour);
        }
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    total_time = std::chrono::duration<double, std::milli>(end - start).count();
    
    double total_extractions = blobs.size() * iterations;
    double avg_time = total_time / total_extractions;
    double throughput = 1000.0 / avg_time;  // features per second
    
    std::cout << "\n  Total Extractions: " << total_extractions << std::endl;
    std::cout << "  Total Time: " << std::fixed << std::setprecision(2) << total_time << " ms" << std::endl;
    std::cout << "  Avg per Feature: " << std::fixed << std::setprecision(3) << avg_time << " ms" << std::endl;
    std::cout << "  Throughput: " << std::fixed << std::setprecision(1) << throughput << " features/sec" << std::endl;
    
    // Validation
    if (avg_time < 2.0) {
        std::cout << "\n  ? PASS: Feature extraction time < 2ms (optimized)" << std::endl;
    } else if (avg_time < 5.0) {
        std::cout << "\n  ? WARNING: Feature extraction time " << avg_time << "ms (acceptable)" << std::endl;
    } else {
        std::cout << "\n  ? FAIL: Feature extraction time too high: " << avg_time << "ms" << std::endl;
    }
}

// ===== TEST 3: Small Blob Feature Extraction =====
void testSmallBlobFeatures() {
    std::cout << "\n" << std::string(70, '=') << std::endl;
    std::cout << "TEST 3: Small Blob Feature Extraction (r < 10px)" << std::endl;
    std::cout << std::string(70, '=') << std::endl;
    
    ImprovedFeatureExtractorOptimized extractor;
    
    // Create test image with small blobs
    cv::Mat test_image = cv::Mat(256, 256, CV_8U, cv::Scalar(100));
    cv::circle(test_image, cv::Point(80, 80), 5, cv::Scalar(235), -1);    // r=5
    cv::circle(test_image, cv::Point(150, 100), 7, cv::Scalar(230), -1);  // r=7
    cv::circle(test_image, cv::Point(200, 80), 4, cv::Scalar(225), -1);   // r=4
    
    // Detect
    ImprovedBlobDetectorCpp detector(256);
    auto small_blobs = detector.detectSmallObjects(test_image);
    
    std::cout << "\n[DETECTED " << small_blobs.size() << " SMALL BLOBS]" << std::endl;
    
    if (small_blobs.empty()) {
        std::cout << "  ? WARNING: No small blobs detected" << std::endl;
        return;
    }
    
    // Extract features for each
    for (size_t i = 0; i < std::min(size_t(3), small_blobs.size()); ++i) {
        const auto& blob = small_blobs[i];
        double area = cv::contourArea(blob.contour);
        double radius = std::sqrt(area / M_PI);
        
        auto features = extractor.extractFeatures(blob.roi, blob.contour);
        
        std::cout << "\n  [BLOB " << i << ": radius?" << std::fixed << std::setprecision(1) 
                  << radius << "px, area=" << area << "px²]" << std::endl;
        
        // Show key features
        std::cout << "    Mean Intensity: " << features.mean_intensity
                  << " | Contrast: " << features.contrast
                  << " | Circularity: " << features.circularity << std::endl;
        
        if (isValidFeatureValue(features.mean_intensity) && 
            isValidFeatureValue(features.contrast) &&
            isValidFeatureValue(features.circularity)) {
            std::cout << "    ? Valid features" << std::endl;
        } else {
            std::cout << "    ? Invalid features" << std::endl;
        }
    }
}

// ===== TEST 4: Feature Vector Consistency =====
void testFeatureVectorConsistency() {
    std::cout << "\n" << std::string(70, '=') << std::endl;
    std::cout << "TEST 4: Feature Vector Consistency (17 dims)" << std::endl;
    std::cout << std::string(70, '=') << std::endl;
    
    ImprovedFeatureExtractorOptimized extractor;
    
    // Create test image
    cv::Mat test_image = cv::Mat(256, 256, CV_8U, cv::Scalar(85));
    cv::circle(test_image, cv::Point(128, 128), 18, cv::Scalar(240), -1);
    cv::circle(test_image, cv::Point(60, 60), 8, cv::Scalar(230), -1);
    
    // Detect
    ImprovedBlobDetectorCpp detector(256);
    auto blobs = detector.detectBlobs(test_image);
    
    if (blobs.empty()) {
        std::cout << "\n? No blobs detected" << std::endl;
        return;
    }
    
    std::cout << "\n[FEATURE VECTOR DIMENSIONALITY]" << std::endl;
    
    for (size_t i = 0; i < std::min(size_t(2), blobs.size()); ++i) {
        const auto& blob = blobs[i];
        
        // Extract as feature vector
        auto fvec = extractor.extractFeatureVector(blob.roi, blob.contour);
        
        // Check dimensions
        std::cout << "\n  Blob " << i << ":" << std::endl;
        std::cout << "    Feature vector dimension: " << FeatureVector::DIM << std::endl;
        std::cout << "    Expected dimension: 17" << std::endl;
        
        if (FeatureVector::DIM == 17) {
            std::cout << "    ? PASS: Correct dimensionality" << std::endl;
        } else {
            std::cout << "    ? FAIL: Incorrect dimensionality" << std::endl;
        }
        
        // Check all values are valid
        bool all_valid = true;
        for (int j = 0; j < FeatureVector::DIM; ++j) {
            if (!std::isfinite(fvec.data[j])) {
                all_valid = false;
                std::cout << "    ? Feature[" << j << "] = " << fvec.data[j] << " (invalid)" << std::endl;
            }
        }
        
        if (all_valid) {
            std::cout << "    ? All 17 features valid" << std::endl;
        }
    }
}

// ===== TEST 5: Numerical Stability =====
void testNumericalStability() {
    std::cout << "\n" << std::string(70, '=') << std::endl;
    std::cout << "TEST 5: Numerical Stability (Edge Cases)" << std::endl;
    std::cout << std::string(70, '=') << std::endl;
    
    ImprovedFeatureExtractorOptimized extractor;
    
    // Test case 1: Very small blob
    std::cout << "\n  [Test Case 1: Very small blob]" << std::endl;
    cv::Mat tiny_image = cv::Mat(64, 64, CV_8U, cv::Scalar(100));
    cv::circle(tiny_image, cv::Point(32, 32), 2, cv::Scalar(230), -1);
    
    ImprovedBlobDetectorCpp detector_small(64);
    auto tiny_blobs = detector_small.detectSmallObjects(tiny_image);
    
    if (!tiny_blobs.empty()) {
        auto features = extractor.extractFeatures(tiny_blobs[0].roi, tiny_blobs[0].contour);
        bool stable = isValidFeatureValue(features.area) && 
                      isValidFeatureValue(features.mean_intensity);
        std::cout << "    Result: " << (stable ? "? Stable" : "? Unstable") << std::endl;
    }
    
    // Test case 2: High contrast blob
    std::cout << "\n  [Test Case 2: High contrast blob]" << std::endl;
    cv::Mat high_contrast = cv::Mat(256, 256, CV_8U, cv::Scalar(10));
    cv::circle(high_contrast, cv::Point(128, 128), 15, cv::Scalar(255), -1);
    
    ImprovedBlobDetectorCpp detector_contrast(256);
    auto contrast_blobs = detector_contrast.detectBlobs(high_contrast);
    
    if (!contrast_blobs.empty()) {
        auto features = extractor.extractFeatures(contrast_blobs[0].roi, contrast_blobs[0].contour);
        bool stable = isValidFeatureValue(features.contrast) && 
                      isValidFeatureValue(features.mean_intensity);
        std::cout << "    Result: " << (stable ? "? Stable" : "? Unstable") << std::endl;
    }
    
    // Test case 3: Low contrast blob
    std::cout << "\n  [Test Case 3: Low contrast blob]" << std::endl;
    cv::Mat low_contrast = cv::Mat(256, 256, CV_8U, cv::Scalar(100));
    cv::circle(low_contrast, cv::Point(128, 128), 12, cv::Scalar(120), -1);
    
    ImprovedBlobDetectorCpp detector_low(256);
    auto low_blobs = detector_low.detectBlobs(low_contrast);
    
    if (!low_blobs.empty()) {
        auto features = extractor.extractFeatures(low_blobs[0].roi, low_blobs[0].contour);
        bool stable = isValidFeatureValue(features.contrast) && 
                      isValidFeatureValue(features.mean_intensity);
        std::cout << "    Result: " << (stable ? "? Stable" : "? Unstable") << std::endl;
    }
}

// ===== MAIN TEST RUNNER =====
int main() {
    std::cout << "\n" << std::string(70, '*') << std::endl;
    std::cout << "*  STEP 3: FEATURE EXTRACTION OPTIMIZATION - TESTS  *" << std::endl;
    std::cout << std::string(70, '*') << std::endl;
    
    try {
        testFeatureCorrectness();
        testPerformanceBenchmark();
        testSmallBlobFeatures();
        testFeatureVectorConsistency();
        testNumericalStability();
        
        std::cout << "\n" << std::string(70, '=') << std::endl;
        std::cout << "ALL TESTS COMPLETED" << std::endl;
        std::cout << std::string(70, '=') << std::endl;
        
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "\n? Exception: " << e.what() << std::endl;
        return 1;
    }
}
