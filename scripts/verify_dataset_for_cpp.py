#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Quick utility to verify dataset and prepare for C++ training
"""

import pandas as pd
from pathlib import Path

print("=" * 70)
print("DATASET VERIFICATION FOR C++ TRAINING")
print("=" * 70)

dataset_dir = Path("dataset_ir_realistic")
csv_path = dataset_dir / "annotations.csv"

# Load CSV
df = pd.read_csv(csv_path)

print(f"\nDataset loaded: {len(df)} samples")
print(f"Columns: {df.columns.tolist()}")

# Check labels
print(f"\nLabel distribution:")
print(f"  Bullet holes (label=1): {(df['label'] == 1).sum()}")
print(f"  Non-bullets (label=0): {(df['label'] == 0).sum()}")
print(f"  Ambiguous (label=-1): {(df['label'] == -1).sum()}")

# Check features
feature_cols = ['area', 'circularity', 'solidity', 'mean_intensity', 
                'std_intensity', 'edge_density', 'aspect_ratio', 
                'center_x', 'center_y', 'radius']

print(f"\nFeature statistics:")
for col in feature_cols:
    if col in df.columns:
        print(f"  {col}: min={df[col].min():.2f}, max={df[col].max():.2f}, mean={df[col].mean():.2f}")

# Check for missing values
print(f"\nMissing values:")
print(df.isnull().sum())

print(f"\nDataset is ready for C++ training pipeline!")
print(f"CSV path: {csv_path}")
print("=" * 70)
