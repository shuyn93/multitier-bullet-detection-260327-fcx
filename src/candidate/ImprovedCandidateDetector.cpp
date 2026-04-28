#include "candidate/ImprovedCandidateDetector.h"
#include "candidate/ImprovedCandidateDetector.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgproc/types_c.h>
#include <opencv2/highgui.hpp>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace bullet_detection {

ImprovedCandidateDetector::ImprovedCandidateDetector() {}

std::vector<CandidateRegion> ImprovedCandidateDetector::detectCandidates(
    const cv::Mat& frame_ir,
    uint64_t frame_id
) {
    std::vector<CandidateRegion> candidates;

    if (frame_ir.empty()) {
        return candidates;
    }

    // ===== STEP 1: PREPROCESSING =====
    cv::Mat preprocessed = preprocessImage(frame_ir);

    // ===== STEP 2: MULTI-THRESHOLD BLOB DETECTION =====
    auto contours = detectBlobsMultiThreshold(preprocessed);

    // ===== STEP 3: VALIDATE & CLEANUP =====
    validateAndCleanupContours(contours);

    // ===== STEP 4: PROCESS EACH CANDIDATE =====
    for (const auto& contour : contours) {
        if (contour.size() < 4) continue;

        cv::Rect bbox = cv::boundingRect(contour);

        // Early geometric checks
        if (bbox.width < 2 || bbox.height < 2) continue;
        if (bbox.area() < min_area_ || bbox.area() > max_area_) continue;

        // Extract ROI from ORIGINAL frame (not preprocessed)
        cv::Mat roi = extractROI(frame_ir, bbox);

        double area = cv::contourArea(contour);
        double perimeter = cv::arcLength(contour, true);

        if (perimeter < 1.0) continue;  // Avoid division by zero

        // Compute circularity
        float circularity = static_cast<float>(4 * M_PI * area / (perimeter * perimeter));
        circularity = std::max(0.0f, std::min(1.0f, circularity));

        // Compute solidity
        std::vector<cv::Point> hull;
        cv::convexHull(contour, hull);
        double hull_area = cv::contourArea(hull);
        float solidity = hull_area > 0 ? static_cast<float>(area / hull_area) : 0.0f;

        // ===== GEOMETRIC FILTER =====
        if (!passesGeometricFilter(bbox, area, perimeter, hull)) {
            continue;
        }

        // ===== RADIAL FILTER (Early, before full feature extraction) =====
        if (radial_filtering_enabled_) {
            if (!passesRadialFilter(roi, contour)) {
                continue;
            }
        }

        // ===== INTENSITY FILTER =====
        if (!passesIntensityFilter(roi)) {
            continue;
        }

        // ===== PASSED ALL FILTERS =====
        float detection_score = circularity * solidity;  // Combined geometric score

        CandidateRegion candidate;
        candidate.bbox = bbox;
        candidate.contour = contour;
        candidate.roi_image = roi.clone();
        candidate.frame_id = frame_id;
        candidate.detection_score = detection_score;

        candidates.push_back(candidate);
    }

    return candidates;
}

// ===== PREPROCESSING =====

cv::Mat ImprovedCandidateDetector::preprocessImage(const cv::Mat& frame) {
    cv::Mat result = frame.clone();

    // Step 1: Denoise
    if (denoise_enabled_) {
        result = denoiseImage(result);
    }

    // Step 2: Contrast Enhancement
    if (clahe_enabled_) {
        result = enhanceContrast(result);
    }

    return result;
}

cv::Mat ImprovedCandidateDetector::denoiseImage(const cv::Mat& image) {
    cv::Mat denoised;

    // Bilateral filter: denoise while preserving edges
    cv::bilateralFilter(
        image,
        denoised,
        bilateral_diameter_,
        bilateral_sigma_,
        bilateral_sigma_
    );

    return denoised;
}

cv::Mat ImprovedCandidateDetector::enhanceContrast(const cv::Mat& image) {
    cv::Mat result;

    // Use CLAHE on the image directly (works for both grayscale and color)
    // For color images, apply to intensity/luminance
    
    if (image.channels() == 1) {
        // Grayscale: use CLAHE directly
        cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(
            clahe_clip_limit_,
            cv::Size(clahe_tile_size_, clahe_tile_size_)
        );
        clahe->apply(image, result);
    } else {
        // Color: convert to YCrCb, enhance Y channel, convert back
        // (More portable than LAB which may not be available)
        cv::Mat ycrcb;
        try {
            cv::cvtColor(image, ycrcb, cv::COLOR_BGR2YCrCb);
        } catch (...) {
            // Fallback: just use the input image
            return image.clone();
        }

        std::vector<cv::Mat> channels;
        cv::split(ycrcb, channels);

        cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(
            clahe_clip_limit_,
            cv::Size(clahe_tile_size_, clahe_tile_size_)
        );
        clahe->apply(channels[0], channels[0]);

        cv::merge(channels, ycrcb);
        cv::cvtColor(ycrcb, result, cv::COLOR_YCrCb2BGR);
    }

    return result;
}

// ===== MULTI-THRESHOLD BLOB DETECTION =====

std::vector<std::vector<cv::Point>> ImprovedCandidateDetector::detectBlobsMultiThreshold(
    const cv::Mat& frame
) {
    // Get Otsu blobs
    auto blobs_otsu = detectBlobsOtsu(frame);

    // Get adaptive blobs
    auto blobs_adaptive = adaptive_threshold_enabled_
        ? detectBlobsAdaptive(frame)
        : std::vector<std::vector<cv::Point>>();

    // Fuse results
    auto fused_blobs = fuseDetectionResults(blobs_otsu, blobs_adaptive);

    // Handle small objects
    processSmallBlobs(fused_blobs, frame);

    return fused_blobs;
}

std::vector<std::vector<cv::Point>> ImprovedCandidateDetector::detectBlobsOtsu(
    const cv::Mat& frame
) {
    std::vector<std::vector<cv::Point>> contours;

    if (frame.empty()) return contours;

    // Compute Otsu threshold
    double otsu_thresh = cv::threshold(
        frame,
        frame,
        0,
        255,
        cv::THRESH_OTSU | cv::THRESH_BINARY
    );

    // Adjust threshold (lower by offset to catch more blobs)
    int adjusted_thresh = static_cast<int>(std::max(0.0, otsu_thresh - otsu_offset_));

    cv::Mat thresh;
    cv::threshold(frame, thresh, adjusted_thresh, 255, cv::THRESH_BINARY);

    // Morphology: close small holes, open noise
    cv::Mat kernel = cv::getStructuringElement(
        cv::MORPH_ELLIPSE,
        cv::Size(MORPH_KERNEL_LARGE, MORPH_KERNEL_LARGE)
    );

    cv::morphologyEx(thresh, thresh, cv::MORPH_CLOSE, kernel, cv::Point(-1, -1), 1);
    cv::morphologyEx(thresh, thresh, cv::MORPH_OPEN, kernel, cv::Point(-1, -1), 1);

    cv::findContours(
        thresh.clone(),
        contours,
        cv::RETR_TREE,  // Get hierarchy (internal + external)
        cv::CHAIN_APPROX_SIMPLE
    );

    return contours;
}

std::vector<std::vector<cv::Point>> ImprovedCandidateDetector::detectBlobsAdaptive(
    const cv::Mat& frame
) {
    std::vector<std::vector<cv::Point>> contours;

    if (frame.empty()) return contours;

    cv::Mat thresh;
    cv::adaptiveThreshold(
        frame,
        thresh,
        255,
        cv::ADAPTIVE_THRESH_GAUSSIAN_C,
        cv::THRESH_BINARY,
        adaptive_block_size_,
        adaptive_constant_
    );

    // Morphology
    cv::Mat kernel = cv::getStructuringElement(
        cv::MORPH_ELLIPSE,
        cv::Size(MORPH_KERNEL_SMALL, MORPH_KERNEL_SMALL)
    );

    cv::morphologyEx(thresh, thresh, cv::MORPH_CLOSE, kernel, cv::Point(-1, -1), 1);

    cv::findContours(
        thresh.clone(),
        contours,
        cv::RETR_TREE,
        cv::CHAIN_APPROX_SIMPLE
    );

    return contours;
}

std::vector<std::vector<cv::Point>> ImprovedCandidateDetector::fuseDetectionResults(
    const std::vector<std::vector<cv::Point>>& blobs_otsu,
    const std::vector<std::vector<cv::Point>>& blobs_adaptive
) {
    std::vector<std::vector<cv::Point>> fused;

    // Add all Otsu blobs
    for (const auto& blob : blobs_otsu) {
        fused.push_back(blob);
    }

    // Add adaptive blobs (avoid duplicates via IoU check)
    for (const auto& adaptive_blob : blobs_adaptive) {
        cv::Rect adaptive_rect = cv::boundingRect(adaptive_blob);

        bool is_duplicate = false;
        for (const auto& otsu_blob : blobs_otsu) {
            cv::Rect otsu_rect = cv::boundingRect(otsu_blob);

            // Simple IoU check
            int intersection = (adaptive_rect & otsu_rect).area();
            int unionn = (adaptive_rect | otsu_rect).area();
            float iou = unionn > 0 ? static_cast<float>(intersection) / unionn : 0.0f;

            if (iou > 0.3f) {  // 30% overlap = duplicate
                is_duplicate = true;
                break;
            }
        }

        if (!is_duplicate) {
            fused.push_back(adaptive_blob);
        }
    }

    return fused;
}

void ImprovedCandidateDetector::processSmallBlobs(
    std::vector<std::vector<cv::Point>>& contours,
    const cv::Mat& original_image
) {
    // Small blobs (area < SMALL_BLOB_THRESHOLD) need special handling
    // because normal morphology erodes them

    std::vector<std::vector<cv::Point>> additional_small_blobs;

    for (auto it = contours.begin(); it != contours.end(); ++it) {
        double area = cv::contourArea(*it);

        if (area > 0 && area < SMALL_BLOB_THRESHOLD) {
            // This is a small blob - verify it has sufficient intensity
            cv::Rect bbox = cv::boundingRect(*it);
            
            // Clamp bbox to image bounds
            bbox.x = std::max(0, bbox.x);
            bbox.y = std::max(0, bbox.y);
            bbox.width = std::min(bbox.width, original_image.cols - bbox.x);
            bbox.height = std::min(bbox.height, original_image.rows - bbox.y);
            
            if (bbox.area() <= 0) continue;
            
            cv::Mat roi = original_image(bbox);

            // If small blob has reasonable intensity (likely bullet hole)
            float mean_intensity = cv::mean(roi)[0];
            
            // For small holes, use lower threshold (dim holes are still valid)
            if (mean_intensity > 80) {
                // Small blob confirmed as potentially valid
                // It remains in the contours list and passes through
            }
        }
    }

    // Note: Removed the erase logic - we keep all small blobs for Stage 2 filtering
    // Stage 2 (RobustNoiseFilter) will make the final precision decision
}


// ===== RADIAL PROPERTIES (EARLY) =====

float ImprovedCandidateDetector::computeQuickRadialSymmetry(
    const cv::Mat& roi,
    const std::vector<cv::Point>& contour
) {
    if (contour.empty() || roi.empty()) {
        return 0.5f;
    }

    // Get centroid
    cv::Moments moments = cv::moments(contour);
    if (moments.m00 == 0) return 0.5f;

    cv::Point2f center(
        static_cast<float>(moments.m10 / moments.m00),
        static_cast<float>(moments.m01 / moments.m00)
    );

    // Sample radial profile from center outward
    float max_radius = std::min(
        static_cast<float>(std::min(roi.cols, roi.rows)) / 2.0f,
        50.0f
    );

    auto radial_profile = sampleRadialProfile(roi, center, max_radius, 16);

    if (radial_profile.empty()) {
        return 0.5f;
    }

    // Radial symmetry = inverse of coefficient of variation
    // High symmetry ? low variance ? high score
    float mean_val = std::accumulate(radial_profile.begin(), radial_profile.end(), 0.0f)
        / radial_profile.size();

    float variance = 0.0f;
    for (float val : radial_profile) {
        variance += (val - mean_val) * (val - mean_val);
    }
    variance /= radial_profile.size();

    float std_dev = std::sqrt(variance);
    float cv = mean_val > 1.0f ? std_dev / mean_val : 1.0f;  // Coefficient of variation

    // Convert CV to symmetry score [0,1]
    // Low CV (symmetric) ? high score
    // High CV (asymmetric) ? low score
    float symmetry = 1.0f / (1.0f + cv);

    return std::max(0.0f, std::min(1.0f, symmetry));
}

float ImprovedCandidateDetector::computeQuickRadialGradient(
    const cv::Mat& roi,
    const std::vector<cv::Point>& contour
) {
    if (contour.empty() || roi.empty()) {
        return 0.5f;
    }

    // Compute gradients
    cv::Mat gradx, grady;
    cv::Sobel(roi, gradx, CV_32F, 1, 0, 3);
    cv::Sobel(roi, grady, CV_32F, 0, 1, 3);

    cv::Mat gradient_magnitude;
    cv::magnitude(gradx, grady, gradient_magnitude);

    // Get centroid
    cv::Moments moments = cv::moments(contour);
    if (moments.m00 == 0) return 0.5f;

    cv::Point2f center(
        static_cast<float>(moments.m10 / moments.m00),
        static_cast<float>(moments.m01 / moments.m00)
    );

    // Sample gradient magnitudes along radii
    float max_radius = std::min(
        static_cast<float>(std::min(roi.cols, roi.rows)) / 2.0f,
        50.0f
    );

    // For each direction, check if gradient is consistent (radial)
    int num_angles = 16;
    float consistency_sum = 0.0f;

    for (int i = 0; i < num_angles; ++i) {
        float angle = 2.0f * M_PI * i / num_angles;
        float cos_a = std::cos(angle);
        float sin_a = std::sin(angle);

        // Sample along this radius
        float radial_gradient = 0.0f;
        int count = 0;

        for (float r = 5.0f; r < max_radius; r += 5.0f) {
            int x = static_cast<int>(center.x + r * cos_a);
            int y = static_cast<int>(center.y + r * sin_a);

            if (x >= 0 && x < gradient_magnitude.cols && y >= 0 && y < gradient_magnitude.rows) {
                radial_gradient += gradient_magnitude.at<float>(y, x);
                count++;
            }
        }

        if (count > 0) {
            radial_gradient /= count;
            consistency_sum += radial_gradient;
        }
    }

    // Normalize
    float consistency = consistency_sum / num_angles / 100.0f;  // Normalize by typical gradient
    consistency = std::max(0.0f, std::min(1.0f, consistency));

    return consistency;
}

// ===== FILTERING =====

bool ImprovedCandidateDetector::passesGeometricFilter(
    const cv::Rect& bbox,
    double area,
    double perimeter,
    const std::vector<cv::Point>& hull
) {
    // Area check
    if (area < min_area_ || area > max_area_) {
        return false;
    }

    // Circularity check
    if (perimeter > 0) {
        float circularity = static_cast<float>(4 * M_PI * area / (perimeter * perimeter));
        if (circularity < min_circularity_) {
            return false;
        }
    } else {
        return false;
    }

    // Solidity check
    double hull_area = cv::contourArea(hull);
    if (hull_area > 0) {
        float solidity = static_cast<float>(area / hull_area);
        if (solidity < min_solidity_) {
            return false;
        }
    } else {
        return false;
    }

    return true;
}

bool ImprovedCandidateDetector::passesRadialFilter(
    const cv::Mat& roi,
    const std::vector<cv::Point>& contour
) {
    // Compute quick radial symmetry
    float radial_sym = computeQuickRadialSymmetry(roi, contour);
    if (radial_sym < min_radial_symmetry_) {
        return false;
    }

    // Compute quick radial gradient consistency
    float radial_grad = computeQuickRadialGradient(roi, contour);
    if (radial_grad < min_radial_gradient_) {
        return false;
    }

    return true;
}

bool ImprovedCandidateDetector::passesIntensityFilter(const cv::Mat& roi) {
    if (roi.empty()) {
        return false;
    }

    // Check contrast: max intensity - min intensity > threshold
    double min_val, max_val;
    cv::minMaxLoc(roi, &min_val, &max_val);

    float contrast = static_cast<float>(max_val - min_val);
    if (contrast < min_contrast_) {
        return false;
    }

    return true;
}

void ImprovedCandidateDetector::validateAndCleanupContours(
    std::vector<std::vector<cv::Point>>& contours
) {
    // Remove very small contours
    contours.erase(
        std::remove_if(
            contours.begin(),
            contours.end(),
            [](const std::vector<cv::Point>& c) { return c.size() < 4; }
        ),
        contours.end()
    );

    // Remove contours with zero area
    contours.erase(
        std::remove_if(
            contours.begin(),
            contours.end(),
            [](const std::vector<cv::Point>& c) {
                return cv::contourArea(c) < 1.0;
            }
        ),
        contours.end()
    );
}

// ===== HELPER FUNCTIONS =====

cv::Mat ImprovedCandidateDetector::extractROI(const cv::Mat& frame, const cv::Rect& bbox) {
    cv::Rect safe_bbox = bbox & cv::Rect(0, 0, frame.cols, frame.rows);
    if (safe_bbox.area() == 0) {
        return cv::Mat();
    }
    return frame(safe_bbox).clone();
}

cv::Point2f ImprovedCandidateDetector::computeBlobCentroid(
    const std::vector<cv::Point>& contour
) {
    cv::Moments moments = cv::moments(contour);
    if (moments.m00 == 0) {
        return cv::Point2f(0, 0);
    }
    return cv::Point2f(
        static_cast<float>(moments.m10 / moments.m00),
        static_cast<float>(moments.m01 / moments.m00)
    );
}

std::vector<float> ImprovedCandidateDetector::sampleRadialProfile(
    const cv::Mat& roi,
    const cv::Point2f& center,
    float max_radius,
    int num_angles
) {
    std::vector<float> profile;

    for (int i = 0; i < num_angles; ++i) {
        float angle = 2.0f * M_PI * i / num_angles;
        float cos_a = std::cos(angle);
        float sin_a = std::sin(angle);

        // Find where ray exits the ROI
        int sample_index = 0;
        for (int r = 1; r < static_cast<int>(max_radius); ++r) {
            int x = static_cast<int>(center.x + r * cos_a);
            int y = static_cast<int>(center.y + r * sin_a);

            if (x < 0 || x >= roi.cols || y < 0 || y >= roi.rows) {
                break;
            }
            sample_index = r;
        }

        if (sample_index > 0) {
            profile.push_back(static_cast<float>(sample_index));
        }
    }

    return profile;
}

// ===== CONFIGURATION =====

void ImprovedCandidateDetector::setPreprocessingParams(
    int bilateral_diameter,
    float bilateral_sigma,
    float clahe_clip_limit
) {
    bilateral_diameter_ = bilateral_diameter;
    bilateral_sigma_ = bilateral_sigma;
    clahe_clip_limit_ = clahe_clip_limit;
}

void ImprovedCandidateDetector::setDetectionThresholds(
    int min_area,
    int max_area,
    float min_circularity,
    float min_solidity,
    float min_radial_symmetry
) {
    min_area_ = min_area;
    max_area_ = max_area;
    min_circularity_ = min_circularity;
    min_solidity_ = min_solidity;
    min_radial_symmetry_ = min_radial_symmetry;
}

} // namespace bullet_detection
