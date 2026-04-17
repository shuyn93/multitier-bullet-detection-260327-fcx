# ??? DATASET DELETION PLAN - WAITING FOR CONFIRMATION

**Status**: ? **AWAITING USER CONFIRMATION**

---

## ?? CRITICAL: THIS WILL DELETE ALL EXISTING DATASETS

### ?? DATASETS TO BE DELETED

#### 1. **dataset_ir_realistic**
- **Location**: `data/datasets/dataset_ir_realistic/`
- **Size**: **0.6 GB**
- **Files**: 20,013
  - `images/`: 20,000 PNG files
  - `samples_visualization/`: 12 files
- **Annotations**: `annotations.csv`

#### 2. **dataset_main**
- **Location**: `data/datasets/dataset_main/`
- **Size**: **0.34 GB**
- **Files**: 10,904
  - `images/`: 10,898 PNG files
  - `test_images/`: 5 files
- **Annotations**: `annotations.csv`

#### 3. **dataset_realistic**
- **Location**: `data/datasets/dataset_realistic/`
- **Size**: **0.31 GB**
- **Files**: 10,006
  - `images/`: 10,000 PNG files
  - `test_images/`: 5 files
- **Annotations**: `annotations.csv`

---

## ?? SUMMARY

| Metric | Value |
|--------|-------|
| **Total Size to Delete** | **1.25 GB** |
| **Total Files** | **40,923** |
| **Datasets** | 3 |
| **CSV Files** | 3 |
| **Images** | ~40,000 PNG files |

---

## ? WHAT WILL BE DELETED

```
data/datasets/
??? dataset_ir_realistic/        ? DELETE
?   ??? images/                  ? DELETE (20,000 files)
?   ??? samples_visualization/   ? DELETE (12 files)
?   ??? annotations.csv          ? DELETE
??? dataset_main/                ? DELETE
?   ??? images/                  ? DELETE (10,898 files)
?   ??? test_images/             ? DELETE (5 files)
?   ??? annotations.csv          ? DELETE
??? dataset_realistic/           ? DELETE
    ??? images/                  ? DELETE (10,000 files)
    ??? test_images/             ? DELETE (5 files)
    ??? annotations.csv          ? DELETE
```

---

## ? WHAT WILL NOT BE DELETED

? `/data/models/` - **PRESERVED** (all trained models safe)  
? `/src/` - **PRESERVED** (all source code safe)  
? `/include/` - **PRESERVED** (all headers safe)  
? `/scripts/` - **PRESERVED** (all scripts safe)  
? `/CMakeLists.txt` - **PRESERVED**  
? All documentation files - **PRESERVED**

---

## ?? NEXT STEPS AFTER DELETION

After confirmation, the following will happen:

1. **Delete all 3 datasets** (~1.25 GB freed)
2. **Create new IR dataset structure**:
   ```
   data/datasets/dataset_ir_final/
   ??? images/
   ??? test_images/
   ??? annotations.csv
   ```
3. **Generate 50,000 new samples** with:
   - Correct IR backlit physics
   - Bright bullet holes (NOT dark blobs)
   - 17 normalized features
   - 4 balanced classes (~12.5k each)

---

## ? IMPORTANT NOTES

- ? **Deletion is IRREVERSIBLE** - No recovery after deletion
- ?? **Archive options**:
  - Could backup to external drive first
  - Keep git history (commits still available)
- ?? **New dataset will be generated fresh**
  - No reuse of old data
  - Full IR physics compliance
  - Consistent feature extraction

---

## ? CONFIRM BEFORE PROCEEDING

**To delete all datasets and proceed, please confirm:**

> "I understand this will DELETE 1.25 GB of data (~40,000 files) and request deletion."

**Without explicit confirmation, NO DELETION will occur.**

---

**Waiting for user confirmation...**
