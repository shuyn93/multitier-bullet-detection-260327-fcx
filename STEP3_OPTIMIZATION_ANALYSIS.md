# STEP 3: OPTIMIZE FEATURE EXTRACTION - ANALYSIS & PLAN

## Current State Analysis

### Performance Bottlenecks Identified

#### 1. **Redundant Loops in Radial Features** ??
**File:** `src/feature/ImprovedFeatureExtractorOptimized.cpp` (lines 218-230)

**Issue:**
```cpp
for (const auto& pt : contour) {
    float gx = cache.roi_gradient_x.at<float>(pt.y, pt.x);
    float gy = cache.roi_gradient_y.at<float>(pt.y, pt.x);
    float grad = std::sqrt(gx * gx + gy * gy);
    gradient_sum += grad;
    count++;
}
```

- **O(n)** loop over contour points with pixel access
- `at<float>()` is slow for individual pixel access
- Can be vectorized using OpenCV's mask operations
- **Est. improvement:** 2-3x speedup

**Fix:** Use `cv::Mat::ptr()` for faster row access or vectorized operations

---

#### 2. **computeRadiiFast() is O(n²)** ??
**File:** `src/feature/ImprovedFeatureExtractorOptimized.cpp` (lines 373-407)

**Current Implementation:**
```cpp
for (int i = 0; i < ray_count; ++i) {  // ray_count = 16-32
    float max_r = 0.0f;
    for (const auto& pt : contour) {  // contour points 50-500
        // Expensive angle computation for each point
        double angle_diff = std::abs(pt_angle - angle);
        if (angle_diff > M_PI) angle_diff = 2 * M_PI - angle_diff;
        
        if (angle_diff < M_PI / ray_count) {  // Small angular tolerance
            float dist = std::sqrt(dx * dx + dy * dy);
            max_r = std::max(max_r, dist);
        }
    }
}
```

**Complexity:** O(ray_count × contour_size) = O(16-32 × 200) = ~3200-6400 ops

**Issues:**
- Redundant angle computation (`atan2`) for each ray
- Inner loop checks angle difference for every point
- No spatial indexing

**Fix:** Use polar coordinate conversion once, then index by angle

---

#### 3. **Edge Density Recomputes Canny** ??
**File:** `src/feature/ImprovedFeatureExtractorOptimized.cpp` (lines 451-469)

**Issue:**
```cpp
// In computeEdgeDensityFast()
cv::Mat edges;
cv::Canny(roi, edges, 50, 150);  // 50-150ms per call
```

- Canny is called even if Laplacian already computed
- Could reuse cached gradients from earlier computation
- Multiple edge detection calls per image

**Fix:** Cache Canny result or use cached gradients

---

#### 4. **Intensity Statistics Computed Multiple Times** ??
**File:** `src/feature/ImprovedFeatureExtractorOptimized.cpp` (lines 324-337)

**Current:**
- Computed in `computeIntensityStatsFast()` (line 438-445)
- Reused for `contrast` feature (line 332)
- Reused for `mean_intensity` feature (line 336)

**Issue:** Multiple passes over image data

**Fix:** Already in cache, just ensure no recomputation

---

#### 5. **Phase Coherence is Placeholder** ??
**File:** `src/feature/ImprovedFeatureExtractorOptimized.cpp` (line 322)

**Issue:**
```cpp
feat.phase_coherence = 0.5f;  // Placeholder!
```

- Not actually computed
- Should use FFT-based phase information
- But FFT is expensive - need to optimize

---

#### 6. **Inefficient Pixel Access in Loops** ??
**Multiple locations:** radial_gradient, laplacian_density, edge_density

**Issue:** Using `mat.at<T>(y, x)` in loops (slow)

**Fix:** Use `mat.ptr<T>(y)` for row-based access

---

## Optimization Strategy

### Priority 1: High Impact, Easy (est. 1.5-2x improvement)
1. Replace `at<T>()` with `ptr<T>()` for pixel loops
2. Vectorize gradient magnitude using OpenCV ops
3. Cache Canny edges

### Priority 2: High Impact, Medium (est. 2-3x improvement)  
1. Optimize `computeRadiiFast()` with polar coordinates
2. Pre-compute radii bounds for angular indexing

### Priority 3: Medium Impact, Complex (est. 1.5x improvement)
1. Implement proper phase coherence (FFT-based)
2. Use pre-computed edge cache

### Priority 4: Code Quality
1. Add detailed comments explaining each optimization
2. Profile actual execution time
3. Add normalization validation

---

## Expected Results

### Before Optimization
- Per-blob extraction: 5-10ms
- Batch (100 blobs): 500-1000ms
- Memory: ~10MB (including intermediate matrices)

### After Optimization  
- Per-blob extraction: 1-2ms (5-10x)
- Batch (100 blobs): 100-200ms
- Memory: ~5-8MB (vectorized ops reuse memory)

**Target:** 3-5x speedup on overall pipeline

---

## Validation Plan

1. **Functional Correctness**
   - All 17 features computed
   - Values in [0, 1] range
   - No NaN/Inf values

2. **Numerical Accuracy**
   - Optimized ? Original (error < 0.1%)

3. **Performance**
   - Time measurement before/after
   - Memory profiling
   - Throughput validation

---

## Files to Modify

1. `src/feature/ImprovedFeatureExtractorOptimized.cpp` - Main optimizations
2. `include/feature/ImprovedFeatureExtractorOptimized.h` - Helper declarations (if needed)

## Files to Create

1. `tests/test_step3_feature_optimization.cpp` - Performance and correctness tests

---

## Implementation Order

1. **Phase 1:** Quick wins (pixel access optimization)
2. **Phase 2:** Medium effort (radii optimization)
3. **Phase 3:** Phase coherence implementation
4. **Phase 4:** Testing and validation

---
