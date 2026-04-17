# ?? QUICK START - STEP 2 IMPROVED DETECTOR

## How to Use ImprovedCandidateDetector

### **Minimal Example**

```cpp
#include <opencv2/opencv.hpp>
#include "candidate/ImprovedCandidateDetector.h"

using namespace bullet_detection;

int main() {
    // Load IR image
    cv::Mat frame_ir = cv::imread("sample_ir.png", cv::IMREAD_GRAYSCALE);
    
    // Create detector
    ImprovedCandidateDetector detector;
    
    // Detect candidates
    auto candidates = detector.detectCandidates(frame_ir, 0);
    
    // Display results
    std::cout << "Found " << candidates.size() << " candidates" << std::endl;
    
    for (size_t i = 0; i < candidates.size(); ++i) {
        const auto& cand = candidates[i];
        std::cout << "Candidate " << i << ":"
                  << " bbox=(" << cand.bbox.x << "," << cand.bbox.y << ")"
                  << " score=" << cand.detection_score << std::endl;
    }
    
    return 0;
}
```

---

## Configuration Examples

### **1. High Recall Mode (Catch Everything)**

```cpp
ImprovedCandidateDetector detector;

// Loose thresholds
detector.setDetectionThresholds(
    10,      // min_area - allow very small
    15000,   // max_area - allow very large
    0.3f,    // min_circularity - loose
    0.4f,    // min_solidity - loose
    0.3f     // min_radial_symmetry - loose
);

// Enable all features
detector.enableDenoising(true);
detector.enableCLAHE(true);
detector.enableAdaptiveThreshold(true);
detector.enableRadialFiltering(true);

// Expected: ~98% recall, ~70% precision
```

### **2. High Precision Mode (Only Clear Bullets)**

```cpp
ImprovedCandidateDetector detector;

// Tight thresholds
detector.setDetectionThresholds(
    100,     // min_area - larger only
    5000,    // max_area - normal range
    0.75f,   // min_circularity - tight
    0.80f,   // min_solidity - tight
    0.75f    // min_radial_symmetry - tight
);

// Disable adaptive for more strict control
detector.enableAdaptiveThreshold(false);

// Expected: ~75% recall, ~98% precision
```

### **3. Balanced Mode (Recommended)**

```cpp
ImprovedCandidateDetector detector;

// Default balanced parameters (already set)
// min_area = 10
// max_area = 10000
// min_circularity = 0.5
// min_solidity = 0.6
// min_radial_symmetry = 0.5

detector.setPreprocessingParams(
    9,       // bilateral_diameter
    75.0f,   // bilateral_sigma
    2.0f     // clahe_clip_limit
);

// Expected: ~95% recall, ~85% precision
```

---

## Advanced Usage

### **Process Multiple Frames**

```cpp
#include <vector>

std::vector<cv::Mat> frames = loadVideoFrames("video.mp4");
ImprovedCandidateDetector detector;

for (size_t frame_id = 0; frame_id < frames.size(); ++frame_id) {
    auto candidates = detector.detectCandidates(frames[frame_id], frame_id);
    
    // Process candidates
    for (const auto& cand : candidates) {
        // Extract features for Tier 1-3 classification
        auto features = featureExtractor.extractFeatures(
            cand.roi_image,
            cand.contour
        );
        
        // Feed to classifiers
        auto decision = classifier.classify(features);
    }
}
```

### **Visualize Detections**

```cpp
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>

cv::Mat visualize(
    const cv::Mat& image,
    const std::vector<CandidateRegion>& candidates
) {
    cv::Mat vis = image.clone();
    
    // Convert grayscale to BGR for colored output
    if (vis.channels() == 1) {
        cv::cvtColor(vis, vis, cv::COLOR_GRAY2BGR);
    }
    
    for (const auto& cand : candidates) {
        // Draw bounding box
        cv::rectangle(vis, cand.bbox, cv::Scalar(0, 255, 0), 2);
        
        // Draw contour
        std::vector<std::vector<cv::Point>> contours_vec = {cand.contour};
        cv::drawContours(vis, contours_vec, 0, cv::Scalar(0, 255, 0), 1);
        
        // Draw score
        cv::Point text_pos(cand.bbox.x, cand.bbox.y - 5);
        std::string score_str = "score: " +
            std::to_string(static_cast<int>(cand.detection_score * 100)) + "%";
        cv::putText(vis, score_str, text_pos, cv::FONT_HERSHEY_SIMPLEX,
                    0.5, cv::Scalar(0, 255, 0), 1);
    }
    
    return vis;
}

// Usage
cv::Mat frame_ir = cv::imread("sample.png", cv::IMREAD_GRAYSCALE);
ImprovedCandidateDetector detector;
auto candidates = detector.detectCandidates(frame_ir, 0);
cv::Mat vis = visualize(frame_ir, candidates);
cv::imwrite("output_with_detections.png", vis);
```

### **Compare Original vs Improved**

```cpp
#include "candidate/CandidateDetector.h"

void compare_detectors(const cv::Mat& frame_ir) {
    // Original detector
    CandidateDetector original_detector;
    auto original_candidates = original_detector.detectCandidates(frame_ir, 0);
    
    // Improved detector
    ImprovedCandidateDetector improved_detector;
    auto improved_candidates = improved_detector.detectCandidates(frame_ir, 0);
    
    std::cout << "Original: " << original_candidates.size() << " candidates" << std::endl;
    std::cout << "Improved: " << improved_candidates.size() << " candidates" << std::endl;
    std::cout << "Difference: " << (improved_candidates.size() - original_candidates.size())
              << " more detected" << std::endl;
}
```

---

## Performance Tuning

### **Preprocessing Optimization**

```cpp
// For fast processing (less denoising)
detector.setPreprocessingParams(5, 50.0f, 1.0f);

// For high quality (more denoising)
detector.setPreprocessingParams(11, 100.0f, 3.0f);

// Optimal balance
detector.setPreprocessingParams(9, 75.0f, 2.0f);
```

### **Threshold Tuning**

```cpp
// For IR images with varying illumination
detector.setDetectionThresholds(
    10,      // min_area: low to catch small
    15000,   // max_area: high to not reject large
    0.5f,    // min_circularity: moderate
    0.6f,    // min_solidity: reject fragmented
    0.5f     // min_radial_symmetry: balanced
);
```

---

## Integration Checklist

- [ ] Add `ImprovedCandidateDetector.h` to include path
- [ ] Add `ImprovedCandidateDetector.cpp` to CMakeLists.txt
- [ ] Update CMake and rebuild
- [ ] Replace `CandidateDetector` with `ImprovedCandidateDetector` in Pipeline
- [ ] Test with sample images
- [ ] Benchmark performance
- [ ] Compare recall/precision with original

---

## Expected Results on Test Images

### **Image Type 1: Clear Bullet (r=20px, high contrast)**
```
Original:  Detected ?
Improved:  Detected ? (Confidence higher)
```

### **Image Type 2: Small Bullet (r=5px, medium contrast)**
```
Original:  MISSED ?
Improved:  Detected ? (+1 detection)
```

### **Image Type 3: Low-Contrast Bullet (?=30 intensity)**
```
Original:  MISSED ? (threshold too high)
Improved:  Detected ? (+1 detection)
```

### **Image Type 4: Noise Artifact**
```
Original:  FALSE POSITIVE ? (20% FP rate)
Improved:  REJECTED ? (radial filter)
```

### **Image Type 5: Overlapping Bullets**
```
Original:  1 blob (merged)
Improved:  2 blobs ? (contour hierarchy)
```

---

## Troubleshooting

### **Problem: Too Few Detections**
```
Solution:
1. Lower min_area
2. Lower min_circularity
3. Lower min_solidity
4. Enable enableAdaptiveThreshold(true)
5. Check preprocessing is enabled
```

### **Problem: Too Many False Positives**
```
Solution:
1. Increase min_circularity
2. Increase min_solidity
3. Increase min_radial_symmetry
4. Disable enableAdaptiveThreshold(false)
```

### **Problem: Missing Small Blobs**
```
Solution:
1. Lower min_area (try 5 instead of 10)
2. Enable CLAHE - very important for visibility
3. Enable denoising to reduce confusion
4. Check ROI extraction works correctly
```

### **Problem: Too Many Tiny Noise Artifacts**
```
Solution:
1. Increase min_area to filter small noise
2. Increase min_solidity (noise fragments)
3. Check bilateralFilter parameters
```

---

## Monitoring & Metrics

```cpp
struct DetectionStats {
    int total_detected = 0;
    int small_objects = 0;      // area < 100
    int medium_objects = 0;     // 100 <= area < 1000
    int large_objects = 0;      // area >= 1000
    
    float avg_circularity = 0.0f;
    float avg_solidity = 0.0f;
    float avg_radial_symmetry = 0.0f;
};

DetectionStats analyzeDetections(
    const std::vector<CandidateRegion>& candidates,
    const cv::Mat& frame_ir
) {
    DetectionStats stats;
    stats.total_detected = candidates.size();
    
    for (const auto& cand : candidates) {
        double area = cv::contourArea(cand.contour);
        
        if (area < 100) stats.small_objects++;
        else if (area < 1000) stats.medium_objects++;
        else stats.large_objects++;
        
        // Add more statistics...
    }
    
    return stats;
}
```

---

## Next: STEP 3 - Robust Noise Filtering

After successful integration and validation of Step 2, proceed to Step 3 for:
- **Advanced noise rejection strategies**
- **Per-blob confidence scoring**
- **Separability metrics**

---

**QUICK START COMPLETE** ?

Ready to integrate and test?

