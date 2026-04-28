/**
 * STEP 2: IMPROVED BLOB DETECTOR - C++ IMPLEMENTATION
 * 
 * Two-stage detection pipeline:
 * Stage 1: Candidate generation (maximize recall)
 * Stage 2: Filtering & scoring (maximize precision)
 */

#include "candidate/ImprovedBlobDetectorStep2.h"
#include <algorithm>
#include <cmath>
#include <iostream>

// Define M_PI if not available
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace bullet_detection {

ImprovedBlobDetectorCpp::ImprovedBlobDetectorCpp(int image_size)
    : image_size_(image_size),
      threshold_levels_{50, 100, 150, 200},
      adaptive_block_size_(21),
      adaptive_constant_(5),
      min_size_small_(1),
      max_size_small_(314),        // ~10 pixel radius
      min_size_large_(314),
      max_size_large_(100000),
      min_circularity_loose_(0.3f),
      min_solidity_(0.4f) {
}

// ==================== MAIN ENTRY POINTS ====================

std::vector<BlobCandidate> ImprovedBlobDetectorCpp::detectBlobs(const cv::Mat& image) {
    if (image.empty()) {
        return std::vector<BlobCandidate>();
    }
    
    // STAGE 1: Generate candidates (maximize recall)
    std::vector<std::vector<cv::Point>> candidates_stage1 = detectCandidatesStage1(image);
    
    if (candidates_stage1.empty()) {
        std::cerr << "STAGE 1: No candidates found" << std::endl;
        return std::vector<BlobCandidate>();
    }
    
    std::cout << "STAGE 1: Generated " << candidates_stage1.size() << " candidates" << std::endl;
    
    // STAGE 2: Filter & score (maximize precision)
    std::vector<BlobCandidate> candidates_stage2 = filterAndScoreCandidates(image, candidates_stage1);
    
    std::cout << "STAGE 2: Filtered to " << candidates_stage2.size() << " verified candidates" << std::endl;
    
    return candidates_stage2;
}

std::vector<BlobCandidate> ImprovedBlobDetectorCpp::detectSmallObjects(const cv::Mat& image) {
    if (image.empty()) {
        return std::vector<BlobCandidate>();
    }
    
    std::vector<std::vector<cv::Point>> small_candidates;
    
    // Strategy A: Multi-threshold with LOWER thresholds (catch dimmer small holes)
    for (int threshold : {30, 50, 80, 120}) {
        cv::Mat binary;
        cv::threshold(image, binary, threshold, 255, cv::THRESH_BINARY);
        
        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(binary.clone(), contours, cv::RETR_TREE, cv::CHAIN_APPROX_SIMPLE);
        
        for (const auto& contour : contours) {
            double area = cv::contourArea(contour);
            // Target: radius < 10 px ? area < ?*100 ? 314 px²
            // But be generous: accept up to 400 px² (r?11.3)
            if (area >= min_size_small_ && area <= max_size_small_) {
                small_candidates.push_back(contour);
            }
        }
    }
    
    // Strategy B: Adaptive threshold (good for varying local brightness)
    auto adaptive_cands = detectAdaptiveThreshold(image);
    for (const auto& contour : adaptive_cands) {
        double area = cv::contourArea(contour);
        if (area >= min_size_small_ && area <= max_size_small_) {
            small_candidates.push_back(contour);
        }
    }
    
    // Deduplicate
    auto unique_candidates = deduplicateCandidates(small_candidates);
    
    std::cout << "Small object detection: Found " << unique_candidates.size() 
              << " small blobs (r<10)" << std::endl;
    
    // Filter and score (with relaxed criteria for small objects)
    std::vector<BlobCandidate> results;
    for (const auto& contour : unique_candidates) {
        BlobCandidate blob = computeQualityScores(image, contour);
        
        // RELAXED criteria for small objects
        // Small holes may have lower circularity due to pixelization
        if (blob.circularity >= 0.2f &&                    // Very relaxed
            blob.intensity_contrast > 5.0f) {               // Lower threshold for small
            results.push_back(blob);
        }
    }
    
    return results;
}

std::vector<BlobCandidate> ImprovedBlobDetectorCpp::detectBlobsHighRes(
    const cv::Mat& image,
    float scale_factor) {
    
    if (image.empty()) {
        return std::vector<BlobCandidate>();
    }
    
    cv::Size original_size = image.size();
    std::vector<BlobCandidate> results;
    
    // STEP 1: Downscale
    cv::Mat small_image;
    if (scale_factor < 1.0f) {
        int new_width = static_cast<int>(image.cols * scale_factor);
        int new_height = static_cast<int>(image.rows * scale_factor);
        cv::resize(image, small_image, cv::Size(new_width, new_height));
        std::cout << "Downsampled " << original_size << " ? " << small_image.size() << std::endl;
    } else {
        small_image = image.clone();
    }
    
    // STEP 2: Detect on downsampled
    std::vector<BlobCandidate> candidates_small = detectBlobs(small_image);
    
    // STEP 3: Upscale coordinates back to original
    float inv_scale = 1.0f / scale_factor;
    for (const auto& cand : candidates_small) {
        cv::Rect bbox = cand.bbox;
        
        // Scale coordinates
        int x_orig = std::max(0, std::min(static_cast<int>(bbox.x * inv_scale), original_size.width - 1));
        int y_orig = std::max(0, std::min(static_cast<int>(bbox.y * inv_scale), original_size.height - 1));
        int w_orig = std::min(static_cast<int>(bbox.width * inv_scale), original_size.width - x_orig);
        int h_orig = std::min(static_cast<int>(bbox.height * inv_scale), original_size.height - y_orig);
        
        // Extract ROI from original image
        if (w_orig > 0 && h_orig > 0) {
            cv::Rect roi_rect(x_orig, y_orig, w_orig, h_orig);
            
            BlobCandidate upscaled_cand = cand;
            upscaled_cand.bbox = roi_rect;
            upscaled_cand.roi = image(roi_rect).clone();
            
            results.push_back(upscaled_cand);
        }
    }
    
    std::cout << "High-res detection: " << results.size() << " candidates (scale=" 
              << scale_factor << ")" << std::endl;
    
    return results;
}

// ==================== STAGE 1: CANDIDATE GENERATION ====================

std::vector<std::vector<cv::Point>> ImprovedBlobDetectorCpp::detectCandidatesStage1(
    const cv::Mat& image) {
    
    std::vector<std::vector<cv::Point>> all_candidates;
    
    // Strategy A: Multi-threshold detection
    auto candidates_multi = detectMultiThreshold(image);
    all_candidates.insert(all_candidates.end(), candidates_multi.begin(), candidates_multi.end());
    
    // Strategy B: Adaptive threshold detection
    auto candidates_adaptive = detectAdaptiveThreshold(image);
    all_candidates.insert(all_candidates.end(), candidates_adaptive.begin(), candidates_adaptive.end());
    
    // Strategy C: Morphological reconstruction
    auto candidates_morph = detectMorphological(image);
    all_candidates.insert(all_candidates.end(), candidates_morph.begin(), candidates_morph.end());
    
    // Deduplicate
    std::vector<std::vector<cv::Point>> unique_candidates = deduplicateCandidates(all_candidates);
    
    return unique_candidates;
}

std::vector<std::vector<cv::Point>> ImprovedBlobDetectorCpp::detectMultiThreshold(
    const cv::Mat& image) {
    
    std::vector<std::vector<cv::Point>> all_contours;
    
    for (int threshold : threshold_levels_) {
        cv::Mat binary;
        cv::threshold(image, binary, threshold, 255, cv::THRESH_BINARY);
        
        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(binary.clone(), contours, cv::RETR_TREE, cv::CHAIN_APPROX_SIMPLE);
        
        // Keep ALL contours (no filtering yet)
        for (const auto& contour : contours) {
            double area = cv::contourArea(contour);
            if (area >= 1 && area <= 100000) {
                all_contours.push_back(contour);
            }
        }
    }
    
    std::cout << "  Multi-threshold: " << all_contours.size() << " candidates" << std::endl;
    return all_contours;
}

std::vector<std::vector<cv::Point>> ImprovedBlobDetectorCpp::detectAdaptiveThreshold(
    const cv::Mat& image) {
    
    cv::Mat binary;
    cv::adaptiveThreshold(
        image,
        binary,
        255,
        cv::ADAPTIVE_THRESH_GAUSSIAN_C,
        cv::THRESH_BINARY,
        adaptive_block_size_,
        adaptive_constant_
    );
    
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(binary.clone(), contours, cv::RETR_TREE, cv::CHAIN_APPROX_SIMPLE);
    
    // Keep ALL contours
    std::vector<std::vector<cv::Point>> result;
    for (const auto& contour : contours) {
        double area = cv::contourArea(contour);
        if (area >= 1 && area <= 100000) {
            result.push_back(contour);
        }
    }
    
    std::cout << "  Adaptive threshold: " << result.size() << " candidates" << std::endl;
    return result;
}

std::vector<std::vector<cv::Point>> ImprovedBlobDetectorCpp::detectMorphological(
    const cv::Mat& image) {
    
    // Use Otsu's method for automatic threshold
    cv::Mat binary;
    cv::threshold(image, binary, 0, 255, cv::THRESH_BINARY + cv::THRESH_OTSU);
    
    // Morphological operations with SIZE-AWARE kernels
    // Small kernel (3x3) for preserving small blobs
    cv::Mat kernel_small = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(3, 3));
    
    // Apply gentle morphology to preserve small structures
    cv::morphologyEx(binary, binary, cv::MORPH_CLOSE, kernel_small, cv::Point(-1, -1), 1);
    
    // Optional: larger kernel for final cleanup (but preserve small blobs)
    // cv::morphologyEx(binary, binary, cv::MORPH_OPEN, kernel_small, cv::Point(-1, -1), 1);
    
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(binary.clone(), contours, cv::RETR_TREE, cv::CHAIN_APPROX_SIMPLE);
    
    std::vector<std::vector<cv::Point>> result;
    for (const auto& contour : contours) {
        double area = cv::contourArea(contour);
        if (area >= 1 && area <= 100000) {
            result.push_back(contour);
        }
    }
    
    std::cout << "  Morphological: " << result.size() << " candidates" << std::endl;
    return result;
}

std::vector<std::vector<cv::Point>> ImprovedBlobDetectorCpp::deduplicateCandidates(
    const std::vector<std::vector<cv::Point>>& candidates) {
    
    if (candidates.empty()) {
        return candidates;
    }
    
    std::vector<std::vector<cv::Point>> filtered;
    std::vector<cv::Rect> bboxes;
    
    for (const auto& contour : candidates) {
        cv::Rect bbox = cv::boundingRect(contour);
        
        // Check if similar bbox already exists
        bool is_duplicate = false;
        for (const auto& existing_bbox : bboxes) {
            if (bboxOverlap(bbox, existing_bbox) > 0.8f) {
                is_duplicate = true;
                break;
            }
        }
        
        if (!is_duplicate) {
            filtered.push_back(contour);
            bboxes.push_back(bbox);
        }
    }
    
    std::cout << "  Deduplication: " << candidates.size() << " ? " << filtered.size() << " candidates" << std::endl;
    return filtered;
}

float ImprovedBlobDetectorCpp::bboxOverlap(const cv::Rect& bbox1, const cv::Rect& bbox2) {
    // Calculate intersection over union (IoU)
    int xi1 = std::max(bbox1.x, bbox2.x);
    int yi1 = std::max(bbox1.y, bbox2.y);
    int xi2 = std::min(bbox1.x + bbox1.width, bbox2.x + bbox2.width);
    int yi2 = std::min(bbox1.y + bbox1.height, bbox2.y + bbox2.height);
    
    if (xi2 <= xi1 || yi2 <= yi1) {
        return 0.0f;
    }
    
    int intersection = (xi2 - xi1) * (yi2 - yi1);
    int union_area = bbox1.area() + bbox2.area() - intersection;
    
    return static_cast<float>(intersection) / (static_cast<float>(union_area) + 1e-6f);
}

// ==================== STAGE 2: FILTERING & SCORING ====================

std::vector<BlobCandidate> ImprovedBlobDetectorCpp::filterAndScoreCandidates(
    const cv::Mat& image,
    const std::vector<std::vector<cv::Point>>& contours) {
    
    std::vector<BlobCandidate> filtered;
    
    for (const auto& contour : contours) {
        BlobCandidate blob = computeQualityScores(image, contour);
        
        // Determine if small or normal blob
        double area = cv::contourArea(contour);
        bool is_small = (area < max_size_small_);  // r < 10 px
        
        // ADAPTIVE filtering based on size
        if (is_small) {
            // STAGE 2 for SMALL blobs: Very lenient
            // (precision filter happens in later tiers)
            if (blob.circularity >= 0.2f &&                    // Very low threshold
                blob.intensity_contrast > 3.0f) {               // Very low threshold
                filtered.push_back(blob);
            }
        } else {
            // STAGE 2 for NORMAL blobs: Stricter
            if (blob.circularity >= min_circularity_loose_ && 
                blob.intensity_contrast > 10.0f) {
                filtered.push_back(blob);
            }
        }
    }
    
    return filtered;
}

BlobCandidate ImprovedBlobDetectorCpp::computeQualityScores(
    const cv::Mat& image,
    const std::vector<cv::Point>& contour) {
    
    BlobCandidate blob;
    blob.contour = contour;
    
    // Basic geometry
    double area = cv::contourArea(contour);
    double perimeter = cv::arcLength(contour, true);
    
    // Circularity: 4?*A/P² (1.0 = perfect circle)
    blob.circularity = static_cast<float>(
        (perimeter > 0) ? (4.0 * M_PI * area) / (perimeter * perimeter) : 0.0
    );
    
    // Bounding box
    cv::Rect bbox = cv::boundingRect(contour);
    blob.bbox = bbox;
    
    // Solidity: area / bbox_area
    int bbox_area = bbox.width * bbox.height;
    blob.solidity = static_cast<float>(area / (bbox_area + 1e-6));
    
    // Intensity analysis
    cv::Mat roi = image(bbox);
    blob.roi = roi.clone();
    
    cv::Scalar mean = cv::mean(roi);
    blob.blob_intensity = static_cast<float>(mean[0]);
    
    // IMPROVED: Compute background intensity by sampling around blob
    // Create a mask for the contour
    cv::Mat mask = cv::Mat::zeros(image.size(), CV_8U);
    cv::drawContours(mask, std::vector<std::vector<cv::Point>>{contour}, 0, cv::Scalar(255), -1);
    
    // Sample pixels in ROI but OUTSIDE contour for background estimate
    // Dilate the mask to get a border region
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5, 5));
    cv::Mat dilated_mask;
    cv::dilate(mask, dilated_mask, kernel, cv::Point(-1, -1), 2);
    
    // Border region = dilated - original
    cv::Mat border_mask = dilated_mask - mask;
    
    // Sample background in border region within ROI
    if (cv::countNonZero(border_mask(bbox)) > 0) {
        cv::Mat border_samples = image(bbox).clone();
        border_samples.setTo(0, (border_mask(bbox) == 0));  // Keep only border pixels
        
        cv::Scalar border_mean = cv::mean(border_samples);
        blob.background_intensity = static_cast<float>(border_mean[0]);
    } else {
        // Fallback: use global mean scaled
        cv::Scalar img_mean = cv::mean(image);
        blob.background_intensity = static_cast<float>(img_mean[0] * 0.8f);
    }
    
    blob.intensity_contrast = blob.blob_intensity - blob.background_intensity;
    
    return blob;
}

} // namespace bullet_detection
