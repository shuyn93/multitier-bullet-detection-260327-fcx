# ?? DATASETS QUICK REFERENCE

## ? ACTIVE DATASETS (Currently Used)

### 1?? dataset_main
- ?? Path: `data/datasets/dataset_main/`
- ??? Images: **10,903** PNG files
- ?? Annotations: `annotations.csv`
- ?? Purpose: Primary training dataset

### 2?? dataset_realistic  
- ?? Path: `data/datasets/dataset_realistic/`
- ??? Images: **10,005** PNG files
- ?? Annotations: `annotations.csv`
- ?? Purpose: Realistic synthetic data

### 3?? dataset_ir_realistic
- ?? Path: `data/datasets/dataset_ir_realistic/`
- ??? Images: **20,012** PNG files
- ?? Annotations: `annotations.csv`
- ?? Purpose: IR/Thermal imaging dataset

---

## ?? ARCHIVED DATASETS (Legacy/Backup)

### 1?? dataset_previous
- ?? Path: `data/datasets_archive/dataset_previous/`
- ??? Images: **10,005** PNG files
- ?? Annotations: `annotations.csv`
- ?? Status: ARCHIVED

### 2?? dataset_ir_test
- ?? Path: `data/datasets_archive/dataset_ir_test/`
- ??? Images: EMPTY
- ?? Status: ARCHIVED

### 3?? dataset_realistic_test
- ?? Path: `data/datasets_archive/dataset_realistic_test/`
- ??? Images: **1** PNG file
- ?? Annotations: `annotations.csv`
- ?? Status: ARCHIVED

---

## ?? STATISTICS

| Category | Count |
|----------|-------|
| **Total Active Images** | 40,920 |
| **Total Archived Images** | 10,006 |
| **Active CSV Files** | 3 |
| **Archived CSV Files** | 2 |
| **Total Datasets** | 6 |

---

## ?? RELATED SCRIPTS

| Script | Purpose |
|--------|---------|
| `scripts/generate_dataset.py` | Generate main dataset |
| `scripts/generate_dataset_realistic.py` | Generate realistic data |
| `scripts/generate_ir_dataset_v4.py` | Generate IR data |
| `scripts/validate_dataset.py` | Validate dataset |
| `scripts/verify_dataset_for_cpp.py` | C++ compatibility check |

---

## ?? KEY DIRECTORIES

```
data/
??? datasets/              ? ACTIVE datasets
??? datasets_archive/      ? ARCHIVED datasets  
??? models/                ? Trained models (NOT dataset)
```

---

**For detailed information, see: DATASETS_INVENTORY_REPORT.md**
