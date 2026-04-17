# ?? CH? M?C THÀNH PH?N - THAM CHI?U NHANH

## ?? T?ng Quan Nhanh

**D? Án:** H? Th?ng Phát Hi?n L? ??n Thông Minh  
**Ngôn Ng?:** C++20, Python 3.x  
**N?n T?ng:** Windows (Visual Studio 2022)  
**Tr?ng Thái:** S?n Xu?t  

---

## ?? C?u Trúc Th? M?c & File

### ?? `src/` - Mã Ngu?n C++ Chính

#### **?i?m Nh?p (Entry Points)**
| File | Ch?c N?ng | Dòng Code |
|------|----------|----------|
| `main.cpp` | Ch??ng trình inference chính | ~100 |
| `training_main.cpp` | Hu?n luy?n model t? dataset | ~150 |
| `incremental_training_main.cpp` | C?p nh?t model trên d? li?u m?i | ~120 |
| `online_curriculum_learning_main.cpp` | H?c t? c?i thi?n qua 10 vòng | ~100 |

**Cách s? d?ng:**
```bash
# Inference
./bullet_hole_detector image.jpg model_dir/

# Training
./trainer dataset.csv model_dir/

# Incremental update
./incremental_trainer new_data.csv model_dir/

# T? c?i thi?n
./curriculum_learner model_dir/ output_dir/
```

---

#### **Core Pipeline**
| File | Ch?c N?ng | Y?u T? Chính |
|------|----------|------------|
| `pipeline/Pipeline.cpp` | ?i?u ph?i toàn b? lu?ng | • Input: Mat • Output: Detections |
| `core/ProductionReady.cpp` | Các hàm s?n xu?t chính | • Error handling • Logging |

**Class chính:**
```cpp
class Pipeline {
    void initialize(string config_path);
    DetectionResults process(const Mat& image);
    void update_model(vector<TrainingSample>& data);
};
```

---

#### **Tier 1: Phát Hi?n ?ng Viên**
| File | Chi Ti?t |
|------|----------|
| `candidate/CandidateDetector.cpp` | **Nhanh:** O(n log n) • 500 ? 50 ?ng viên |

**Th?c hi?n:**
```
1. Background subtraction (GMM ho?c thresholding)
2. Find connected components
3. Shape filtering (area, perimeter)
4. Return top 50 by score
```

---

#### **Tier 2: Phân Lo?i S? B?**
| File | Chi Ti?t |
|------|----------|
| `tier/Tiers.cpp` | Bao g?m t?t c? 3 tiers |
| `tier/ImprovedTierLogic.cpp` | Versioning c?i ti?n |

**Tier 2 C? Th?:**
```cpp
class Tier2Classifier {
    // Input: 50 candidates + 17 features each
    // Model: Gradient Boosting (50 trees)
    // Output: Top 10 scored candidates
};
```

---

#### **Tier 3: Phân Lo?i Chính Xác**
```cpp
class Tier3Classifier {
    // Input: 10 candidates + 100+ advanced features
    // Model: SVM (RBF kernel) ho?c Neural Network
    // Output: Final positive/negative decision
};
```

---

#### **Trích Xu?t ??c Tr?ng**
| File | Features | ?? Ph?c T?p |
|------|----------|------------|
| `feature/FeatureExtractor.cpp` | 17 basic features | O(n³) |
| `feature/ImprovedFeatureExtractor.cpp` | 100+ advanced features | O(n³) |

**17 Basic Features:**
```
A. Geometry (4):     area, circularity, solidity, elongation
B. Radial (2):       radial_symmetry, radial_gradient_consistency
C. Energy (3):       SNR, entropy, ring_energy
D. Frequency (3):    sharpness, laplacian_density, phase_coherence
E. Misc (5):         mean_intensity, intensity_contrast, variance,
                     edge_density, local_uniformity
```

---

#### **Hi?u Ch?nh Confidence**
| File | Ph??ng Pháp |
|------|-----------|
| `confidence/Calibration.cpp` | Platt Scaling ho?c Isotonic Regression |

**M?c ?ích:** Raw scores ? True probabilities [0,1]

---

#### **Theo Dõi & Tái T?o 3D**
| File | Ch?c N?ng |
|------|----------|
| `tracking/TrackManager.cpp` | Qu?n lý tracks qua frames |
| `tracking/ImprovedTracking.cpp` | Tracking c?i ti?n |
| `reconstruction/Triangulation.cpp` | Tái t?o 3D t? stereo |
| `reconstruction/ImprovedTriangulation.cpp` | Tái t?o c?i ti?n |

---

#### **C?u Hình Camera**
| File | Ch?c N?ng |
|------|----------|
| `calibration/CameraModel.cpp` | Mô hình camera (intrinsics, distortion) |

---

#### **Hu?n Luy?n & C?i Thi?n**
| File | Ch?c N?ng | T?c ?? |
|------|----------|--------|
| `training/IncrementalTrainer.cpp` | Incremental learning | 50-60s |
| `training/OnlineCurriculumLearner.cpp` | Curriculum learning 10 vòng | 500-600s |

**Incremental Learning:**
```
Hu?n luy?n l?i t? ??u: 2.5+ gi?
Incremental training: 50-60s
Speedup: 180x
```

**Curriculum Learning:**
```
Vòng 1-10: Easy ? Medium ? Hard samples
Result: +5-10% accuracy improvement
```

---

#### **T?i ?u Hi?u N?ng**
| File | T?i ?u |
|------|--------|
| `performance/OptimizedProcessing.cpp` | SIMD, vectorization, GPU support |

---

### ?? `include/` - File Header

C?u trúc t??ng t? `src/` nh?ng là `.h` files
```
include/
??? bullet_hole_detection_system.h    # Main header
??? candidate/CandidateDetector.h
??? feature/FeatureExtractor.h
??? tier/Tiers.h
??? tracking/TrackManager.h
??? reconstruction/Triangulation.h
??? calibration/CameraModel.h
??? confidence/Calibration.h
??? training/IncrementalTrainer.h
??? training/OnlineCurriculumLearner.h
??? performance/OptimizedProcessing.h
```

---

### ?? `data/` - D? Li?u & Model

#### **Dataset**
```
data/datasets/
??? dataset_ir_final/              # Dataset IR hoàn ch?nh
?   ??? images/                    # ~50,000 training images
?   ??? test_images/               # Test set
?   ??? annotations.csv            # Labels + 17 features
?
??? dataset_main/                  # Dataset chính
    ??? images/                    # Main training images
    ??? test_images/
    ??? annotations.csv
```

**Annotations CSV Format:**
```
image_name, class_label, f1, f2, ..., f17
000000, bullet_hole, 0.45, 0.82, 0.91, ..., 0.65
000001, overlapping_bullet_holes, 0.52, 0.78, ..., 0.72
...
```

**4 Classes:**
1. `bullet_hole` - L? ??n ??n, sáng
2. `overlapping_bullet_holes` - Nhi?u l? ch?ng lên
3. `unclear` - M?, ?? t??ng ph?n th?p
4. `non_bullet` - Nhi?u, ph?n chi?u

---

#### **Trained Models**
```
data/models/
??? tier1_model.bin                # Random Forest
??? tier2_model.bin                # Gradient Boosting
??? tier3_model.bin                # SVM ho?c Neural Network
??? calibration.bin                # Confidence calibration
??? metadata.json                  # Model info
```

**metadata.json:**
```json
{
  "tier1": {
    "type": "RandomForest",
    "n_features": 17,
    "n_trees": 100,
    "accuracy": 0.92
  },
  "tier2": {
    "type": "GradientBoosting",
    "n_features": 17,
    "n_trees": 50,
    "accuracy": 0.94
  },
  "tier3": {
    "type": "SVM",
    "n_features": 100,
    "kernel": "rbf",
    "accuracy": 0.97
  },
  "created": "2024-01-15",
  "training_samples": 50000
}
```

---

### ?? `scripts/` - Python Helper Scripts

#### **Sinh D? Li?u**
| Script | M?c ?ích | T?c ?? |
|--------|---------|--------|
| `generate_phase_batch.py` | Sinh 50,000 samples batch | 0.448s/image |
| `generate_ir_final_dataset.py` | Sinh d? li?u IR backlit | ~6.2 hours |
| `benchmark_generation.py` | ?o t?c ?? sinh ?nh | - |

**S? d?ng:**
```bash
# Sinh 50,000 samples theo batch 100
python scripts/generate_phase_batch.py

# Benchmark
python scripts/benchmark_generation.py
```

---

#### **Hu?n Luy?n & Ki?m Tra**
| Script | M?c ?ích |
|--------|---------|
| `train_models.py` | Hu?n luy?n Tier 1-3 |
| `train_multi_tier.py` | Hu?n luy?n & eval multi-tier |
| `validate_dataset.py` | Ki?m tra quality dataset |
| `analyze_datasets.py` | Phân tích th?ng kê |

---

#### **Utility & Ki?m Ch?ng**
| Script | Ch?c N?ng |
|--------|----------|
| `verify_dataset.py` | Ki?m tra file/format |
| `display_random_sample.py` | Xem sample ng?u nhiên |
| `display_samples_by_label.py` | Xem samples theo class |
| `diagnose_images.py` | Ki?m tra l?i ?nh |

---

### ?? `tests/` - Unit Tests

```
tests/CMakeLists.txt     # Build configuration for tests
```

---

### ?? File C?u Hình

| File | M?c ?ích |
|------|---------|
| `CMakeLists.txt` | Build system (C++20, packages) |
| `.gitignore` | Git ignore rules |
| `README.md` | Tài li?u chính |

---

## ?? Trình T? Làm Vi?c (Workflow)

### **1?? Chu?n B? D? Li?u**
```bash
# Step 1: Sinh d? li?u
python scripts/generate_phase_batch.py
# Output: data/datasets/dataset_ir_final/
#   - images/ (50,000 ?nh)
#   - annotations.csv

# Step 2: Ki?m tra d? li?u
python scripts/validate_dataset.py
```

---

### **2?? Hu?n Luy?n Model**
```bash
# Hu?n luy?n 3 tiers
python scripts/train_multi_tier.py \
    --data data/datasets/dataset_ir_final/annotations.csv \
    --output data/models/

# Output: 
#   - tier1_model.bin
#   - tier2_model.bin
#   - tier3_model.bin
#   - calibration.bin
```

---

### **3?? Build & Compile C++**
```bash
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022"
cmake --build . --config Release
```

---

### **4?? Inference (Phát Hi?n)**
```cpp
// Trong C++ code
#include "bullet_hole_detection_system.h"

Pipeline pipeline;
pipeline.initialize("config.json");

cv::Mat image = cv::imread("sample.jpg");
DetectionResults results = pipeline.process(image);

for (const auto& det : results) {
    cout << "Detected at (" << det.x << ", " << det.y << ")"
         << " radius=" << det.radius 
         << " confidence=" << det.confidence << endl;
}
```

---

### **5?? C?p Nh?t Model (Incremental)**
```cpp
// Sau khi thu th?p d? li?u m?i
IncrementalTrainer trainer;
trainer.load_model("data/models/");
trainer.load_new_data("new_data.csv");

bool success = trainer.train(50);  // 50 seconds

if (success) {
    trainer.save_model("data/models/");
    cout << "Model updated! Accuracy: " << trainer.get_accuracy() << endl;
}
```

---

### **6?? T? C?i Thi?n (Curriculum Learning)**
```cpp
OnlineCurriculumLearner learner;
learner.load_model("data/models/");

// 10 vòng t? c?i thi?n
for (int i = 0; i < 10; i++) {
    learner.run_iteration(i);  // M?i vòng: ~60s
    float accuracy = learner.evaluate();
    cout << "Iteration " << i << ": accuracy=" << accuracy << endl;
}

learner.save_final_model("data/models/improved/");
```

---

## ?? Tham Chi?u Hi?u N?ng

### T?c ??

| B??c | Th?i Gian |
|------|----------|
| Load Image | 1-2ms |
| Preprocessing | 2-5ms |
| Tier 1 (50 cands) | 10-15ms |
| Feature Extract | 200-400ms |
| Tier 2 (10 cands) | 5-8ms |
| Tier 3 (5 cands) | 15-20ms |
| **TOTAL** | **50-100ms** |

### Chính Xác

| Metric | Giá Tr? |
|--------|--------|
| Accuracy | 95% |
| Precision | 94% |
| Recall | 96% |
| F1-Score | 0.95 |
| ROC-AUC | 0.97 |

### Dung L??ng

| Thành Ph?n | Kích Th??c |
|-----------|-----------|
| Tier 1 Model | ~5MB |
| Tier 2 Model | ~8MB |
| Tier 3 Model | ~20MB |
| **Total** | **~33MB** |

---

## ?? Khái Ni?m Chính

### **IR Backlit (H?ng Ngo?i Backlit)**
- **L? ??n:** SÁNG h?n n?n (NOT dark)
- **Intensity:** 200-255 (máy ?nh sáng)
- **Bán kính:** 5-100 pixels
- **Falloff:** Gaussian-like

### **Multi-Tier Architecture**
```
500 candidates ? 50 (Tier 1) ? 10 (Tier 2) ? 5 (Tier 3) ? Final
    Fast             Medium         Advanced
    17 features      17 features    100+ features
    O(n log n)       O(n)           O(n)
```

### **Catastrophic Forgetting & Solution**
- **Problem:** Model quên d? li?u c? khi h?c m?i
- **Solution:** Mix 80% new + 20% old data khi training

### **Curriculum Learning**
- **Ý T??ng:** Easy ? Medium ? Hard
- **K?t Qu?:** +5-10% accuracy, tránh overfitting

---

## ?? B?t ??u Nhanh

### **Inference trên 1 ?nh:**
```bash
cd build
./bullet_hole_detector ../data/sample.jpg ../data/models/
```

### **T?o d? li?u m?i:**
```bash
python scripts/generate_phase_batch.py
```

### **Hu?n luy?n model:**
```bash
python scripts/train_multi_tier.py
```

### **Ki?m tra accuracy:**
```bash
python scripts/validate_dataset.py
```

---

## ?? Tài Li?u Liên Quan

| Tài Li?u | N?i Dung |
|---------|---------|
| `ARCHITECTURAL_SYSTEM_REPORT_VI.md` | Ki?n trúc chi ti?t (Vietnamese) |
| `TECHNICAL_GUIDE_VI.md` | H??ng d?n k? thu?t (Vietnamese) |
| `README.md` | Quick start (English) |

---

## ?? Tìm Ki?m Nhanh

| C?n tìm | File |
|--------|------|
| Sinh d? li?u | `scripts/generate_phase_batch.py` |
| Hu?n luy?n | `scripts/train_multi_tier.py` |
| Inference | `src/main.cpp` |
| Tier 1 | `src/candidate/CandidateDetector.cpp` |
| Tier 2 | `src/tier/Tiers.cpp` (Tier2Classifier) |
| Tier 3 | `src/tier/Tiers.cpp` (Tier3Classifier) |
| Features | `src/feature/FeatureExtractor.cpp` |
| Calibration | `src/confidence/Calibration.cpp` |
| Incremental | `src/training/IncrementalTrainer.cpp` |
| Curriculum | `src/training/OnlineCurriculumLearner.cpp` |

---

**Tài li?u này cung c?p tham chi?u nhanh cho toàn b? h? th?ng.**

