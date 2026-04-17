# ?? Datasets Inventory Report

**Generated**: 2024
**Project**: Bullet Hole Detection System
**Location**: `C:\Users\Admin\source\repos\bullet_hole_detection_system`

---

## ?? Directory Structure Overview

```
data/
??? datasets/                          # Active datasets
?   ??? dataset_main/                  # Main training dataset
?   ??? dataset_realistic/             # Realistic synthetic dataset
?   ??? dataset_ir_realistic/          # IR (Infrared) realistic dataset
?
??? datasets_archive/                  # Archived/historical datasets
?   ??? dataset_previous/              # Previous version
?   ??? dataset_ir_test/               # IR test dataset (archived)
?   ??? dataset_realistic_test/        # Realistic test dataset (archived)
?
??? models/                            # Trained models (not dataset)
    ??? trained_models/                # Current trained models
    ??? backup/                        # Model backups
```

---

## ?? Active Datasets Summary

### 1. **dataset_main** (Primary Training Dataset)
- **Location**: `data/datasets/dataset_main/`
- **Subdirectories**:
  - `images/` - Main training images
  - `test_images/` - Validation/test images
- **File Statistics**:
  - Total Images: **10,903** PNG files
  - Annotations: **1** CSV file (`annotations.csv`)
  - Total Files: 10,904
- **Purpose**: Primary dataset for training Tier 1 & Tier 2 models
- **Format**: PNG images with feature annotations

### 2. **dataset_realistic** (Synthetic Realistic Dataset)
- **Location**: `data/datasets/dataset_realistic/`
- **Subdirectories**:
  - `images/` - Realistic synthetic images
  - `test_images/` - Test subset
- **File Statistics**:
  - Total Images: **10,005** PNG files
  - Annotations: **1** CSV file (`annotations.csv`)
  - Total Files: 10,006
- **Purpose**: Realistic synthetic data for improved model generalization
- **Format**: Synthetically generated bullet hole images

### 3. **dataset_ir_realistic** (IR Realistic Dataset)
- **Location**: `data/datasets/dataset_ir_realistic/`
- **Subdirectories**:
  - `images/` - IR (Infrared) images
  - `samples_visualization/` - Visualization samples
- **File Statistics**:
  - Total Images: **20,012** PNG files
  - Annotations: **1** CSV file (`annotations.csv`)
  - Total Files: 20,013
- **Purpose**: IR sensor data for infrared-specific model training
- **Format**: Infrared/thermal imaging data

---

## ?? Archived Datasets

### 1. **dataset_previous** (Historical Version)
- **Location**: `data/datasets_archive/dataset_previous/`
- **Contents**:
  - **Images**: 10,005 PNG files
  - **Annotations**: 1 CSV file
  - **Other**: 3 Additional PNG files (likely validation samples)
- **Status**: ?? **ARCHIVED** - No longer in active use
- **Purpose**: Historical reference/backup

### 2. **dataset_ir_test** (IR Test Archive)
- **Location**: `data/datasets_archive/dataset_ir_test/`
- **Status**: ?? **EMPTY/MINIMAL** - Likely placeholder or deprecated
- **Purpose**: Previous IR testing attempt

### 3. **dataset_realistic_test** (Realistic Test Archive)
- **Location**: `data/datasets_archive/dataset_realistic_test/`
- **Contents**:
  - **Images**: 1 PNG file
  - **Annotations**: 1 CSV file
- **Status**: ?? **MINIMAL** - Likely test snapshot
- **Purpose**: Quick validation/testing

---

## ?? Dataset Statistics Summary

| Dataset Name | Location | Images | Annotations | Status |
|---|---|---:|---:|---|
| **dataset_main** | `datasets/` | 10,903 | 1 | ? Active |
| **dataset_realistic** | `datasets/` | 10,005 | 1 | ? Active |
| **dataset_ir_realistic** | `datasets/` | 20,012 | 1 | ? Active |
| **dataset_previous** | `datasets_archive/` | 10,005 | 1 | ?? Archived |
| **dataset_ir_test** | `datasets_archive/` | — | — | ?? Archived |
| **dataset_realistic_test** | `datasets_archive/` | 1 | 1 | ?? Archived |
| **TOTAL ACTIVE** | — | **40,920** | **3** | — |
| **TOTAL ARCHIVED** | — | **10,006** | **2** | — |

---

## ?? Annotation Files

All datasets include CSV annotation files containing:
- **File**: `annotations.csv`
- **Content**: Image metadata, labels, and feature vectors
- **Format**: Comma-separated values (CSV)
- **Location**: Root directory of each dataset

### CSV Files Found:
1. `data/datasets/dataset_main/annotations.csv`
2. `data/datasets/dataset_realistic/annotations.csv`
3. `data/datasets/dataset_ir_realistic/annotations.csv`
4. `data/datasets_archive/dataset_previous/annotations.csv`
5. `data/datasets_archive/dataset_realistic_test/annotations.csv`

---

## ??? Directory Tree (Detailed View)

```
data/
?
??? datasets/
?   ?
?   ??? dataset_main/
?   ?   ??? images/                    (10,903 PNG files)
?   ?   ??? test_images/               (training validation)
?   ?   ??? annotations.csv
?   ?
?   ??? dataset_realistic/
?   ?   ??? images/                    (10,005 PNG files)
?   ?   ??? test_images/               (test subset)
?   ?   ??? annotations.csv
?   ?
?   ??? dataset_ir_realistic/
?       ??? images/                    (20,012 PNG files)
?       ??? samples_visualization/     (preview images)
?       ??? annotations.csv
?
??? datasets_archive/
?   ?
?   ??? dataset_previous/
?   ?   ??? images/                    (10,005 PNG files)
?   ?   ??? test_images/
?   ?   ??? annotations.csv
?   ?
?   ??? dataset_ir_test/
?   ?   ??? images/                    (empty/minimal)
?   ?
?   ??? dataset_realistic_test/
?       ??? images/                    (1 PNG file)
?       ??? annotations.csv
?
??? models/                            [NOT DATASET - SEE BELOW]
    ??? trained_models/
    ??? backup/
    ?   ??? models_backup_20260406_204304/
    ?   ??? models_backup_20260406_211843/
    ??? ...
```

---

## ?? Data Characteristics

### Dataset Types:
1. **Synthetic Data** (`dataset_main`, `dataset_realistic`)
   - Algorithmically generated bullet hole images
   - Controlled parameters for consistency
   - Multiple variations of bullet holes

2. **IR/Thermal Data** (`dataset_ir_realistic`)
   - Infrared imaging format
   - Higher resolution (20K+ images)
   - Specialized for thermal camera training

3. **Archived/Legacy** (datasets_archive/)
   - Previous versions retained for reference
   - Not actively used in training
   - Can be purged to save space

---

## ?? Recommendations

### Cleanup Opportunities:
1. **Archive datasets_archive/** folder if not needed:
   - `dataset_ir_test/` appears empty
   - `dataset_realistic_test/` has only 1 sample
   - Could save significant storage space

2. **Consolidate datasets** if storage is limited:
   - Consider merging similar datasets
   - Archive older versions separately

3. **Document dataset versions**:
   - Keep clear naming convention
   - Version numbers or dates in folder names
   - Add README in each dataset folder

### Before Deletion:
- ? Backup current trained models (`data/models/`)
- ? Verify all annotations are properly saved
- ? Ensure no models depend on specific dataset paths
- ? Document dataset parameters used for training

---

## ?? Related Files

### Dataset Generation Scripts:
Located in `scripts/`:
- `generate_dataset.py` - Primary dataset generation
- `generate_dataset_realistic.py` - Realistic data generation
- `generate_ir_dataset_v4.py` - IR dataset generation
- `validate_dataset.py` - Dataset validation
- `verify_dataset_for_cpp.py` - C++ compatibility check

### Dataset Loading Code:
Located in `include/training/`:
- `DataLoader.h` - C++ dataset loading interface

---

## ?? Dataset Usage in Training

### Training Pipelines:
1. **Incremental Training** (`src/incremental_training_main.cpp`)
   - Loads dataset incrementally
   - Updates models with new samples

2. **Online Curriculum Learning** (`src/online_curriculum_learning_main.cpp`)
   - Uses curriculum-based training
   - Selects hard examples automatically

3. **Standard Training** (`scripts/train_multi_tier_complete.py`)
   - Batch training on full dataset
   - Trains both Tier 1 & Tier 2 models

---

## ?? Data Integrity

**Last Verified**: Current session
- ? All CSV annotation files present
- ? Image counts consistent with metadata
- ? Archive structure intact
- ?? Dataset versions not explicitly labeled (consider adding version info)

---

**End of Report**
