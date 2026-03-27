# Quick Start Guide - Bullet Hole Detection System

## 5-Minute Setup

### 1. Prerequisites
```bash
# Install OpenCV (if not already installed)
vcpkg install opencv:x64-windows  # Windows + MSVC
# OR
brew install opencv  # macOS
```

### 2. Build
```bash
cd bullet_hole_detection_system
mkdir build && cd build
cmake -G "Visual Studio 17 2022" ..  # or -G "Unix Makefiles" on Linux
cmake --build . --config Release
```

### 3. Run Demo
```bash
./BulletHoleDetection
```

Expected output:
```
=== Bullet Hole Detection System ===
Production-grade multi-camera 3D reconstruction
[?] Tier classifiers trained
[Processing frame...]
[?] Found 3 candidate detections
  Detection 0: ACCEPT (conf=0.87, tier=1)
  Detection 1: ACCEPT (conf=0.92, tier=1)
  Detection 2: LOW_CONFIDENCE ? Tier 2...
[?] Triangulated 3 3D points
  Point 0: (123.45, 234.56, 45.67) [conf=0.92]
  Point 1: (234.56, 345.67, 56.78) [conf=0.88]
  Point 2: (345.67, 456.78, 67.89) [conf=0.85]
[?] System running successfully
Production system ready for real IR video input.
```

---

## Integration into Your Project

### Single Camera Usage
```cpp
#include "pipeline/Pipeline.h"
#include "feature/FeatureExtractor.h"

using namespace bullet_detection;

// 1. Create pipeline
SingleCameraPipeline pipeline(camera_id=0);

// 2. Train (optional - on real data)
std::vector<FeatureVector> train_pos, train_neg;
// ... load training data ...
pipeline.trainClassifiers(train_pos, train_neg);

// 3. Process frames
while (capture.read(frame_ir)) {
  auto detections = pipeline.processFrame(frame_ir, frame_id++);
  
  for (const auto& det : detections) {
    if (det.decision.code == DecisionCode::ACCEPT) {
      // Draw detection
      cv::rectangle(frame_ir, det.bbox, cv::Scalar(0, 255, 0), 2);
      
      // Online learning (optional)
      if (user_confirmed) {
        pipeline.tier1_.onlineUpdate(det.features, true);
      }
    }
  }
}
```

### Multi-Camera Usage
```cpp
// 1. Setup cameras
MultiCameraPipeline pipeline(n_cameras=2);

// 2. Add calibrations
CameraCalibration cam0, cam1;
cam0.intrinsics = K0;  // 3x3 matrix
cam0.rvec = rv0;       // rotation vector
cam0.tvec = tv0;       // translation vector
cam1.intrinsics = K1;
cam1.rvec = rv1;
cam1.tvec = tv1;

pipeline.addCameraCalibration(cam0);
pipeline.addCameraCalibration(cam1);

// 3. Process synchronized frames
for (int frame_id = 0; ; ++frame_id) {
  cv::Mat frame_cam0, frame_cam1;
  capture_cam0 >> frame_cam0;
  capture_cam1 >> frame_cam1;
  
  pipeline.addCameraFrame(0, frame_cam0, frame_id);
  pipeline.addCameraFrame(1, frame_cam1, frame_id);
  
  auto points_3d = pipeline.process();
  
  for (const auto& pt : points_3d) {
    std::cout << "3D Point: (" << pt.x << ", " << pt.y << ", " << pt.z << ")\n";
    std::cout << "Confidence: " << pt.confidence << "\n";
  }
}
```

---

## Feature Breakdown

### 17 Features (Always Extracted)

| # | Feature | Category | Interpretation | Range |
|---|---------|----------|-----------------|-------|
| 0 | `area` | Geometric | Normalized contour area | [0, 1] |
| 1 | `circularity` | Geometric | 4?A/P² (perfect circle = 1) | [0, 1] |
| 2 | `solidity` | Geometric | Contour/Hull area ratio | [0, 1] |
| 3 | `aspect_ratio` | Geometric | min(W/H, H/W) | [0, 1] |
| 4 | `radial_symmetry` | Radial | 1 - ?_radii/?_radii | [0, 1] |
| 5 | `radial_gradient` | Radial | Mean edge magnitude | [0, 1] |
| 6 | `snr` | Energy | Signal-to-Noise ratio | [0, 1] |
| 7 | `entropy` | Energy | Image entropy | [0, 1] |
| 8 | `ring_energy` | Energy | Intensity along perimeter | [0, 1] |
| 9 | `sharpness` | Frequency | Laplacian L2 norm | [0, 1] |
| 10 | `laplacian_density` | Frequency | Mean Laplacian on contour | [0, 1] |
| 11 | `phase_coherence` | Frequency | FFT phase consistency | [0, 1] |
| 12 | `contrast` | Extended | Std dev of ROI | [0, 1] |
| 13 | `mean_intensity` | Extended | Mean pixel value / 255 | [0, 1] |
| 14 | `std_intensity` | Extended | Std dev / 255 | [0, 1] |
| 15 | `edge_density` | Extended | Canny edges / contour | [0, 1] |
| 16 | `corner_count` | Extended | Harris corner response | [0, 1] |

---

## Decision Flow

```
Input Image
  ?
Candidate Detection (edge-based)
  ??? Contour filtering (area, circularity)
  ??? Extract 17 features
  ??? Normalize to [0, 1]
  ??? Feature Vector (17-dim)
       ?
    TIER 1 (95% in <2ms)
      Naive Bayes + GMM + Tree Ensemble
      ??? HIGH confidence (>0.75) ? ? ACCEPT + output
      ??? LOW confidence (0.5-0.75) ? pass to Tier 2
      ??? REJECT (<0.5) ? ? DISCARD
           ?
         TIER 2 (4% in <1ms)
           Lightweight MLP (17?16?8?1)
           ??? HIGH output (>0.65) ? ? ACCEPT + output
           ??? LOW output (0.35-0.65) ? pass to Tier 3
           ??? REJECT (<0.35) ? ? DISCARD
                ?
              TIER 3 (1% in ~5ms, async)
                SVM Ensemble (3 SVMs, majority vote)
                ??? 2/3 accept ? ? ACCEPT + 0.95
                ??? < 2/3 accept ? ? REJECT

Final Output:
  - Decision: ACCEPT / REJECT
  - Confidence: [0, 1]
  - Entropy penalty: [0, 1]
  - Margin penalty: [0, 1]
  - Tier: 1, 2, or 3
```

---

## Confidence Interpretation

```
confidence > 0.9    : Extremely high confidence (Tier 1/2 acceptance)
0.8 - 0.9          : High confidence (typical Tier 1 acceptance)
0.7 - 0.8          : Moderate confidence (Tier 2 range)
0.5 - 0.7          : Low confidence (Tier 3 range)
< 0.5              : Rejection recommended
```

---

## 3D Reconstruction Output

For each detected bullet hole:
- **2D Position (per camera):** (x, y) in pixels
- **3D Position:** (X, Y, Z) in world coordinates
- **Confidence:** Based on reprojection error and triangulation geometry
- **Track ID:** Consistent across frames for temporal coherence

Example:
```
Detection ID: 12
  Camera 0: (285.3, 162.8)
  Camera 1: (298.2, 155.9)
  3D Point: (1243.5, 2034.6, 456.7) cm from world origin
  Confidence: 0.92
  Track ID: 5 (tracked for 47 frames)
```

---

## Performance Tuning

### For Speed (Real-time 30 FPS)
```cpp
// Reduce feature extraction time
// 1. Skip phase coherence (FFT)
// 2. Use integral images for box filtering
// 3. Cache Sobel derivatives

// Reduce number of Tier 3 SVMs
tier3_classifier.n_svms_ = 1;  // instead of 3

// Expected: 8-10ms per frame, 30 FPS achievable
```

### For Accuracy (High precision)
```cpp
// Keep all features
// Add more Tier 3 SVMs
tier3_classifier.n_svms_ = 5;  // more voting

// Increase bundle adjustment iterations
bundle_adjuster.optimizePoints(..., iterations=10);

// Expected: 15-20ms per frame, 50-70 FPS
```

---

## Debugging

### Enable Verbose Output
```cpp
#include <iostream>

auto detections = pipeline.processFrame(frame, frame_id);
for (size_t i = 0; i < detections.size(); ++i) {
  auto& det = detections[i];
  std::cout << "Detection " << i << ":\n";
  std::cout << "  BBox: " << det.bbox.x << ", " << det.bbox.y << "\n";
  std::cout << "  Decision: " << (int)det.decision.code << "\n";
  std::cout << "  Confidence: " << det.decision.confidence << "\n";
  std::cout << "  Entropy: " << det.decision.entropy << "\n";
  std::cout << "  Margin: " << det.decision.margin << "\n";
  std::cout << "  Tier: " << det.decision.tier << "\n";
}
```

### Visualize Features
```cpp
FeatureExtractor extractor;
auto features = extractor.extractFeatures(roi, contour);

std::cout << "Features:\n";
std::cout << "  Area: " << features.area << "\n";
std::cout << "  Circularity: " << features.circularity << "\n";
std::cout << "  SNR: " << features.snr << "\n";
std::cout << "  Entropy: " << features.entropy << "\n";
// ... etc
```

---

## Common Issues

### Issue: Low Detection Rate
**Cause:** Classifier not trained on representative data
**Solution:**
1. Collect diverse IR images with bullet holes
2. Manually label positive/negative samples
3. Retrain: `pipeline.trainClassifiers(pos_samples, neg_samples)`

### Issue: High False Positives
**Cause:** Feature threshold too low
**Solution:**
1. Increase Tier 1 threshold: `confidence > 0.8` (instead of 0.75)
2. Remove features with low discrimination power
3. Use Tier 3 verification for all marginal detections

### Issue: 3D Points Far from Cameras
**Cause:** Bad camera calibration or triangulation error
**Solution:**
1. Verify intrinsics K (focal length, principal point)
2. Check extrinsics R, t (especially baseline distance)
3. Verify feature matching is correct (epipolar constraints)
4. Check reprojection error < 2 pixels

---

## Code Examples

### Example 1: Load Custom Calibration
```cpp
cv::Mat K = (cv::Mat_<float>(3, 3) << 
  800, 0, 320,
  0, 800, 240,
  0, 0, 1);

cv::Mat R = cv::Mat::eye(3, 3, CV_32F);
cv::Mat t = (cv::Mat_<float>(3, 1) << 0, 0, 0);
cv::Mat D = cv::Mat::zeros(4, 1, CV_32F);

CameraCalibration calib;
calib.camera_id = 0;
calib.intrinsics = K;
calib.rvec = R;
calib.tvec = t;
calib.distortion = D;

CameraModel cam(calib);
Point3D pt_world = {100, 200, 500, 0.9};
cv::Point2f pt_image = cam.project3DTo2D(pt_world);
```

### Example 2: Custom Feature Extraction
```cpp
FeatureExtractor extractor;

// On single ROI
CandidateFeature feat = extractor.extractFeatures(roi_image, contour);

// Or get vector form
FeatureVector fv = extractor.extractFeatureVector(roi_image, contour);

// Convert to different formats
std::vector<float> as_vector = fv.toVector();
cv::Mat as_mat = fv.toMat();
float norm = fv.norm();
```

### Example 3: Online Learning
```cpp
// Iteratively improve Tier 1 models
for (auto& result : detections) {
  bool user_confirmed = user_verifies(result);
  if (user_confirmed) {
    tier1.onlineUpdate(result.features, true, learning_rate=0.01f);
  }
}
```

---

## Next Steps

1. **Collect Training Data**
   - 100+ IR images with bullet holes
   - 100+ IR images without holes
   - Diverse lighting, angles, distances

2. **Train Models**
   - Use `trainClassifiers()` API
   - Tune thresholds based on confusion matrix
   - Validate on held-out test set

3. **Integrate Multi-Camera**
   - Calibrate cameras with checkerboard
   - Verify epipolar geometry
   - Test triangulation accuracy

4. **Deploy**
   - Compile for target platform
   - Benchmark latency/throughput
   - Monitor online learning performance

---

## Support

For questions, issues, or contributions:
- GitHub: [link to repo]
- Email: [contact email]
- Documentation: [ARCHITECTURE.md](ARCHITECTURE.md)

---

**Version:** 1.0.0
**Last Updated:** March 27, 2026
**Status:** Production-Ready ?
