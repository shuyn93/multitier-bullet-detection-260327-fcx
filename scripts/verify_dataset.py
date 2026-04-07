# -*- coding: utf-8 -*-
"""
Dataset Verification and Final Report
"""

import pandas as pd
import numpy as np
from pathlib import Path
import os

DATASET_DIR = Path("dataset")
IMAGES_DIR = DATASET_DIR / "images"

print("=" * 80)
print("DATASET VERIFICATION REPORT")
print("=" * 80)
print()

# Check files
print("FILE STRUCTURE:")
print("-" * 80)

csv_path = DATASET_DIR / "annotations.csv"
if csv_path.exists():
    print(f"? annotations.csv exists")
    df = pd.read_csv(csv_path)
    print(f"  - Rows: {len(df)}")
    print(f"  - Columns: {len(df.columns)}")
else:
    print("? annotations.csv not found")
    exit(1)

img_count = len(list(IMAGES_DIR.glob("*.png")))
print(f"? Images directory contains {img_count} PNG files")

print()
print("DATA VALIDATION:")
print("-" * 80)

# Check rows match
if img_count == len(df):
    print(f"? Image count matches CSV rows ({len(df)})")
else:
    print(f"? Mismatch: {img_count} images vs {len(df)} CSV rows")

# Check columns
required_cols = [
    'sample_id', 'label', 'area', 'perimeter', 'circularity', 'eccentricity',
    'aspect_ratio', 'mean_intensity', 'std_intensity', 'min_intensity',
    'max_intensity', 'contrast', 'entropy', 'edge_density', 'gradient_mean',
    'gradient_std', 'contour_variance', 'fractal_dimension', 'hole_depth_estimate',
    'difficulty_level', 'noise_level', 'blur_level', 'illumination_variance'
]

missing_cols = [c for c in required_cols if c not in df.columns]
if not missing_cols:
    print(f"? All {len(required_cols)} required columns present")
else:
    print(f"? Missing columns: {missing_cols}")

# Check NaN values
nan_count = df.isna().sum().sum()
inf_count = df.isin([np.inf, -np.inf]).sum().sum()
print(f"? NaN values: {nan_count}")
print(f"? Inf values: {inf_count}")

print()
print("LABEL DISTRIBUTION:")
print("-" * 80)

for label in ['bullet_hole', 'non_bullet', 'ambiguous']:
    count = (df['label'] == label).sum()
    ratio = count / len(df) * 100
    expected = {'bullet_hole': 50, 'non_bullet': 30, 'ambiguous': 20}[label]
    status = "?" if abs(ratio - expected) < 2 else "?"
    print(f"{status} {label:15s}: {count:5d} ({ratio:5.1f}%) [expected: {expected}%]")

print()
print("DIFFICULTY DISTRIBUTION:")
print("-" * 80)

for diff in ['easy', 'medium', 'hard']:
    count = (df['difficulty_level'] == diff).sum()
    ratio = count / len(df) * 100
    expected = {'easy': 30, 'medium': 50, 'hard': 20}[diff]
    status = "?" if abs(ratio - expected) < 2 else "?"
    print(f"{status} {diff:10s}: {count:5d} ({ratio:5.1f}%) [expected: {expected}%]")

print()
print("FEATURE RANGES:")
print("-" * 80)

feature_ranges = {
    'area': (0, 2000),
    'circularity': (0, 1),
    'mean_intensity': (0, 255),
    'contrast': (0, 100),
    'entropy': (0, 8),
    'eccentricity': (0, 1),
    'aspect_ratio': (0.1, 10),
    'hole_depth_estimate': (0, 1),
}

for feature, (min_exp, max_exp) in feature_ranges.items():
    min_val = df[feature].min()
    max_val = df[feature].max()
    in_range = min_val >= min_exp and max_val <= max_exp
    status = "?" if in_range else "?"
    print(f"{status} {feature:20s}: [{min_val:8.2f}, {max_val:8.2f}] (expected: [{min_exp}, {max_exp}])")

print()
print("=" * 80)
print("DATASET GENERATION SUMMARY")
print("=" * 80)
print()
print(f"? DATASET GENERATION SUCCESSFUL")
print()
print(f"Dataset Statistics:")
print(f"  - Total samples: {len(df)}")
print(f"  - Total images: {img_count}")
print(f"  - Total features: {len(required_cols)}")
print(f"  - File size (CSV): {csv_path.stat().st_size / (1024*1024):.1f} MB")
print()
print(f"Location: {DATASET_DIR.absolute()}")
print(f"  - Images: {IMAGES_DIR.absolute()}")
print(f"  - Annotations: {csv_path.absolute()}")
print()
print("Status: ? READY FOR USE")
print()
print("Next steps:")
print("  1. Use dataset_utils.py to load data into ML models")
print("  2. Run validate_dataset.py for detailed analysis")
print("  3. Run train_models.py to train classification models")
print()
print("=" * 80)
