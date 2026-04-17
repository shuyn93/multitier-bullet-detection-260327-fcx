# ?? SYSTEM TEST PLAN & EXECUTION GUIDE

**Status:** Ready to Test  
**Date:** 2024-01-15  
**Focus:** Build, test, identify errors, and upgrade system  

---

## ?? TEST PLAN OVERVIEW

```
Phase 1: Build Verification       (5 min)
  ?? CMake configuration
  ?? C++20 compilation
  ?? All targets compile

Phase 2: Unit Testing             (10 min)
  ?? STEP 2: ImprovedCandidateDetector
  ?? STEP 3: RobustNoiseFilter
  ?? Integration functionality

Phase 3: Integration Testing      (15 min)
  ?? Load sample IR image
  ?? Run detection pipeline
  ?? Verify output
  ?? Check performance metrics

Phase 4: Performance Testing      (10 min)
  ?? Processing time measurement
  ?? Memory usage analysis
  ?? Batch processing test
  ?? Scaling verification

Phase 5: Error Handling & Fix     (20 min)
  ?? Identify compilation errors
  ?? Fix issues systematically
  ?? Verify fixes work
  ?? Document solutions

Total Time: ~60 minutes
```

---

## ? PHASE 1: BUILD VERIFICATION

### **Step 1.1: Check CMake Version**

```bash
cmake --version
# Expected: 3.8+
```

### **Step 1.2: Create Build Directory**

```bash
# If not exists
mkdir build
cd build

# Clean build
rm -rf *  # On Windows: del /Q *
```

### **Step 1.3: Configure with CMake**

```bash
# Visual Studio 2022, x64, Release
cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release

# Expected output:
# -- Configuring done
# -- Generating done
# -- Build files have been written to: ...
```

### **Step 1.4: Verify Configuration**

Check for messages:
```
-- Found OpenCV
-- Found Eigen3
-- Configuring done
```

If missing dependencies:
```bash
# Install OpenCV (vcpkg)
vcpkg install opencv4:x64-windows

# Install Eigen3
vcpkg install eigen3:x64-windows

# Update CMake paths
cmake .. -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
```

### **Step 1.5: Build All Targets**

```bash
cmake --build . --config Release --parallel 8

# Expected:
# [100%] Built target BulletHoleDetection
# [100%] Built target autonomous_training
# [100%] Built target training_pipeline
# [100%] Built target incremental_training
# [100%] Built target online_curriculum_learning
# [100%] Built target test_worst_case_scenarios
```

---

## ? PHASE 2: UNIT TESTING

### **Step 2.1: Add STEP 2 & 3 to Build**

First, update CMakeLists.txt to include STEP 2 & 3:

```cmake
# Find in CMakeLists.txt around line 38-46
# Add these lines after CandidateDetector.cpp:

add_executable(BulletHoleDetection 
    src/main.cpp
    src/pipeline/Pipeline.cpp
    src/feature/FeatureExtractor.cpp
    src/feature/ImprovedFeatureExtractor.cpp
    src/tier/Tiers.cpp
    src/tier/ImprovedModels.cpp
    src/tier/ImprovedTierLogic.cpp
    src/candidate/CandidateDetector.cpp
    src/candidate/ImprovedCandidateDetector.cpp        # NEW - STEP 2
    src/candidate/RobustNoiseFilter.cpp                # NEW - STEP 3
    # ... rest of files ...
)
```

### **Step 2.2: Update Pipeline.cpp Integration**

Check if Pipeline.cpp has been updated with STEP 2 & 3:

```cpp
// In src/pipeline/Pipeline.cpp, look for:
#include "candidate/ImprovedCandidateDetector.h"  // STEP 2
#include "candidate/RobustNoiseFilter.h"          // STEP 3

// In detectCandidates method:
ImprovedCandidateDetector detector;      // STEP 2
RobustNoiseFilter filter;                 // STEP 3

auto raw_candidates = detector.detectCandidates(frame_ir, frame_id);
auto filtered = filter.filterAndScoreCandidates(raw_candidates, frame_ir);
```

### **Step 2.3: Create Unit Test for STEP 2 & 3**

```bash
# Create test file
cat > tests/test_step2_step3.cpp << 'EOF'
#include <iostream>
#include <opencv2/opencv.hpp>
#include "candidate/ImprovedCandidateDetector.h"
#include "candidate/RobustNoiseFilter.h"

using namespace bullet_detection;

int main() {
    std::cout << "=== STEP 2 & 3 UNIT TEST ===" << std::endl;
    
    // Test STEP 2: ImprovedCandidateDetector
    {
        std::cout << "\n[TEST] STEP 2: ImprovedCandidateDetector" << std::endl;
        
        ImprovedCandidateDetector detector;
        
        // Create dummy test image (256x256 grayscale)
        cv::Mat test_image = cv::Mat(256, 256, CV_8U, cv::Scalar(100));
        
        // Add synthetic bullet hole (bright blob)
        cv::circle(test_image, cv::Point(128, 128), 15, cv::Scalar(240), -1);
        
        // Test detection
        auto candidates = detector.detectCandidates(test_image, 0);
        
        std::cout << "  Candidates detected: " << candidates.size() << std::endl;
        
        if (candidates.size() > 0) {
            std::cout << "  ? STEP 2 works - candidates found" << std::endl;
        } else {
            std::cout << "  ? STEP 2 issue - no candidates found" << std::endl;
            return 1;
        }
    }
    
    // Test STEP 3: RobustNoiseFilter
    {
        std::cout << "\n[TEST] STEP 3: RobustNoiseFilter" << std::endl;
        
        RobustNoiseFilter filter;
        filter.setFilteringLevel(1);  // Balanced
        
        ImprovedCandidateDetector detector;
        cv::Mat test_image = cv::Mat(256, 256, CV_8U, cv::Scalar(100));
        cv::circle(test_image, cv::Point(128, 128), 15, cv::Scalar(240), -1);
        
        auto raw = detector.detectCandidates(test_image, 0);
        auto filtered = filter.filterAndScoreCandidates(raw, test_image);
        
        std::cout << "  Raw candidates: " << raw.size() << std::endl;
        std::cout << "  Filtered candidates: " << filtered.size() << std::endl;
        
        if (filtered.size() > 0) {
            // Check confidence scores
            float avg_score = 0.0f;
            for (const auto& cand : filtered) {
                avg_score += cand.detection_score;
                std::cout << "    Candidate score: " << cand.detection_score << std::endl;
            }
            avg_score /= filtered.size();
            std::cout << "  Avg score: " << avg_score << std::endl;
            std::cout << "  ? STEP 3 works - filtering active, scores computed" << std::endl;
        } else {
            std::cout << "  ? STEP 3 issue - no candidates after filtering" << std::endl;
            return 1;
        }
    }
    
    std::cout << "\n=== ALL TESTS PASSED ===" << std::endl;
    return 0;
}
EOF

# Add to CMakeLists.txt
cat >> CMakeLists.txt << 'EOF'

# ===== STEP 2 & 3 UNIT TEST =====
if(OpenCV_FOUND)
add_executable(test_step2_step3
    tests/test_step2_step3.cpp
    src/candidate/ImprovedCandidateDetector.cpp
    src/candidate/RobustNoiseFilter.cpp
)

target_include_directories(test_step2_step3 PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(test_step2_step3 PRIVATE ${OpenCV_LIBS})
endif()
EOF
```

### **Step 2.4: Compile and Run Unit Tests**

```bash
cd build
cmake --build . --config Release
./Release/test_step2_step3.exe
```

**Expected output:**
```
=== STEP 2 & 3 UNIT TEST ===

[TEST] STEP 2: ImprovedCandidateDetector
  Candidates detected: 5
  ? STEP 2 works - candidates found

[TEST] STEP 3: RobustNoiseFilter
  Raw candidates: 5
  Filtered candidates: 5
  Candidate score: 0.89
  Candidate score: 0.91
  ...
  Avg score: 0.90
  ? STEP 3 works - filtering active, scores computed

=== ALL TESTS PASSED ===
```

---

## ? PHASE 3: INTEGRATION TESTING

### **Step 3.1: Load Real Sample Image**

```bash
# Use existing sample image
# data/datasets/dataset_main/images/010498.png

cat > tests/test_integration_real_image.cpp << 'EOF'
#include <iostream>
#include <chrono>
#include <opencv2/opencv.hpp>
#include "candidate/ImprovedCandidateDetector.h"
#include "candidate/RobustNoiseFilter.h"

using namespace bullet_detection;

int main() {
    std::cout << "=== INTEGRATION TEST - REAL IMAGE ===" << std::endl;
    
    // Load real IR image
    std::string image_path = "data/datasets/dataset_main/images/010498.png";
    cv::Mat image = cv::imread(image_path, cv::IMREAD_GRAYSCALE);
    
    if (image.empty()) {
        std::cerr << "? Failed to load image: " << image_path << std::endl;
        return 1;
    }
    
    std::cout << "? Image loaded: " << image.size() << std::endl;
    
    // STEP 2: Detection
    {
        std::cout << "\n[STEP 2] Detection Pipeline" << std::endl;
        
        ImprovedCandidateDetector detector;
        
        auto start = std::chrono::high_resolution_clock::now();
        auto candidates = detector.detectCandidates(image, 0);
        auto end = std::chrono::high_resolution_clock::now();
        
        float elapsed = std::chrono::duration<float, std::milli>(end - start).count();
        
        std::cout << "  Candidates detected: " << candidates.size() << std::endl;
        std::cout << "  Time: " << elapsed << " ms" << std::endl;
        
        if (candidates.empty()) {
            std::cerr << "  ? Warning: No candidates detected" << std::endl;
        }
    }
    
    // STEP 2 + STEP 3: Full pipeline
    {
        std::cout << "\n[STEP 2+3] Full Pipeline (Detection + Filtering)" << std::endl;
        
        ImprovedCandidateDetector detector;
        RobustNoiseFilter filter;
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
            std::cout << "  Confidence scores:" << std::endl;
            for (size_t i = 0; i < std::min(size_t(5), filtered.size()); ++i) {
                std::cout << "    [" << i << "] score=" << filtered[i].detection_score << std::endl;
            }
        }
    }
    
    std::cout << "\n? Integration test completed successfully" << std::endl;
    return 0;
}
EOF
```

### **Step 3.2: Compile and Run**

```bash
cd build
cmake --build . --config Release

# Run integration test
./Release/test_integration_real_image.exe
```

**Expected output:**
```
=== INTEGRATION TEST - REAL IMAGE ===
? Image loaded: [256 x 256]

[STEP 2] Detection Pipeline
  Candidates detected: 45
  Time: 25.3 ms

[STEP 2+3] Full Pipeline (Detection + Filtering)
  Raw candidates: 45
  After filtering: 38
  Total time: 95.2 ms
  Confidence scores:
    [0] score=0.87
    [1] score=0.92
    [2] score=0.81
    [3] score=0.88
    [4] score=0.95

? Integration test completed successfully
```

---

## ? PHASE 4: PERFORMANCE TESTING

### **Step 4.1: Batch Processing Test**

```bash
cat > tests/test_performance_batch.cpp << 'EOF'
#include <iostream>
#include <chrono>
#include <vector>
#include <filesystem>
#include <opencv2/opencv.hpp>
#include "candidate/ImprovedCandidateDetector.h"
#include "candidate/RobustNoiseFilter.h"

namespace fs = std::filesystem;

int main() {
    std::cout << "=== PERFORMANCE TEST - BATCH PROCESSING ===" << std::endl;
    
    std::string image_dir = "data/datasets/dataset_main/images";
    
    // Find all images
    std::vector<std::string> image_files;
    for (const auto& entry : fs::directory_iterator(image_dir)) {
        if (entry.path().extension() == ".png") {
            image_files.push_back(entry.path().string());
        }
    }
    
    std::cout << "Found " << image_files.size() << " images to process" << std::endl;
    
    if (image_files.empty()) {
        std::cerr << "No images found!" << std::endl;
        return 1;
    }
    
    // Test on first 10 images
    int test_count = std::min(10, static_cast<int>(image_files.size()));
    
    ImprovedCandidateDetector detector;
    RobustNoiseFilter filter;
    filter.setFilteringLevel(1);
    
    float total_time = 0.0f;
    int total_candidates = 0;
    int total_filtered = 0;
    
    for (int i = 0; i < test_count; ++i) {
        cv::Mat image = cv::imread(image_files[i], cv::IMREAD_GRAYSCALE);
        
        if (image.empty()) continue;
        
        auto start = std::chrono::high_resolution_clock::now();
        auto raw = detector.detectCandidates(image, i);
        auto filtered = filter.filterAndScoreCandidates(raw, image);
        auto end = std::chrono::high_resolution_clock::now();
        
        float elapsed = std::chrono::duration<float, std::milli>(end - start).count();
        
        total_time += elapsed;
        total_candidates += raw.size();
        total_filtered += filtered.size();
        
        std::cout << "[" << i+1 << "/" << test_count << "] "
                  << "raw=" << raw.size() << " "
                  << "filtered=" << filtered.size() << " "
                  << "time=" << elapsed << "ms" << std::endl;
    }
    
    float avg_time = total_time / test_count;
    float avg_candidates = total_candidates / static_cast<float>(test_count);
    float avg_filtered = total_filtered / static_cast<float>(test_count);
    
    std::cout << "\n=== PERFORMANCE SUMMARY ===" << std::endl;
    std::cout << "Average time per image: " << avg_time << " ms" << std::endl;
    std::cout << "Average candidates (raw): " << avg_candidates << std::endl;
    std::cout << "Average candidates (filtered): " << avg_filtered << std::endl;
    std::cout << "Filter reduction: " 
              << (1.0f - avg_filtered/avg_candidates) * 100.0f << "%" << std::endl;
    
    if (avg_time < 150.0f) {
        std::cout << "? Performance OK (<150ms)" << std::endl;
    } else {
        std::cout << "? Performance warning: >" << avg_time << "ms" << std::endl;
    }
    
    return 0;
}
EOF
```

### **Step 4.2: Run Performance Test**

```bash
./Release/test_performance_batch.exe
```

---

## ?? PHASE 5: ERROR HANDLING & FIXES

### **Common Compilation Errors & Solutions**

#### **Error 1: Header file not found**
```
error: cannot open source file "candidate/ImprovedCandidateDetector.h"
```

**Solution:**
```bash
# Check file exists
ls include/candidate/ImprovedCandidateDetector.h

# If missing, create it from the provided code
# Verify CMakeLists.txt includes:
target_include_directories(BulletHoleDetection PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
```

#### **Error 2: OpenCV not found**
```
error: #include <opencv2/opencv.hpp> No such file or directory
```

**Solution:**
```bash
# Install OpenCV
vcpkg install opencv4:x64-windows

# Update CMake
cmake .. -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
```

#### **Error 3: C++20 features not recognized**
```
error: 'std::ranges' is not a member of namespace 'std'
```

**Solution:**
```bash
# In CMakeLists.txt, ensure:
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Rebuild
cmake --build . --clean-first
cmake --build . --config Release
```

#### **Error 4: LNK1104 linking error**
```
fatal error LNK1104: cannot open file 'opencv_core480d.lib'
```

**Solution:**
```bash
# Make sure linking Release libraries for Release build
# In CMakeLists.txt after find_package:
if(OpenCV_FOUND)
    target_link_libraries(BulletHoleDetection PRIVATE ${OpenCV_LIBS})
endif()
```

### **Runtime Errors & Solutions**

#### **Error: No candidates detected**
- **Cause:** Preprocessing not working or threshold too high
- **Fix:** Adjust threshold or enable CLAHE:
```cpp
detector.setDetectionThresholds(10, 10000, 0.5f, 0.6f, 0.5f);
detector.enableCLAHE(true);
```

#### **Error: Confidence scores are 0.5 or unusual**
- **Cause:** Analysis methods not enabled or no radial pattern
- **Fix:** Verify filtering is enabled:
```cpp
filter.enableTextureAnalysis(true);
filter.enableFrequencyAnalysis(true);
filter.enableProfileAnalysis(true);
```

---

## ?? QUICK TEST CHECKLIST

```
BUILD:
  [ ] CMake configuration successful
  [ ] C++20 compiler detected
  [ ] All targets build without errors
  [ ] Executables generated

UNIT TESTS:
  [ ] STEP 2 test passes
  [ ] STEP 3 test passes
  [ ] Confidence scores in [0,1]
  [ ] Processing time < 150ms

INTEGRATION:
  [ ] Real image loads successfully
  [ ] Candidates detected (>0)
  [ ] Filtering reduces false positives
  [ ] Scores reasonable (0.7+)

PERFORMANCE:
  [ ] Batch processing works
  [ ] Average time acceptable (<150ms)
  [ ] Memory usage reasonable
  [ ] Scales to 10+ images

ERRORS:
  [ ] No compilation errors
  [ ] No runtime crashes
  [ ] All features working
  [ ] Fallback for missing features
```

---

## ?? RUN ALL TESTS AUTOMATICALLY

```bash
#!/bin/bash
# run_all_tests.sh

cd build

echo "=== Building all targets ==="
cmake --build . --config Release

echo -e "\n=== Running STEP 2 & 3 Unit Tests ==="
./Release/test_step2_step3.exe

echo -e "\n=== Running Integration Tests ==="
./Release/test_integration_real_image.exe

echo -e "\n=== Running Performance Tests ==="
./Release/test_performance_batch.exe

echo -e "\n=== ALL TESTS COMPLETE ==="
```

---

## ?? NEXT STEPS

1. ? Run all tests above
2. ?? Document any errors found
3. ?? Fix errors using provided solutions
4. ? Rerun tests to verify fixes
5. ?? Benchmark improvements
6. ?? Deploy to production

---

**STATUS: READY FOR TESTING** ?

