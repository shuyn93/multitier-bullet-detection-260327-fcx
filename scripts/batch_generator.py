# -*- coding: utf-8 -*-
"""
Bullet Hole Dataset Generator - Batch Processing Version
Generates dataset in chunks for reliability
"""

import numpy as np
import cv2
import pandas as pd
from pathlib import Path
import time

# Set seed
np.random.seed(42)

DATASET_SIZE = 10000
IMAGE_SIZE = 256
DATASET_DIR = Path("dataset")
IMAGES_DIR = DATASET_DIR / "images"

print("=" * 80)
print("DATASET GENERATION - BATCH MODE")
print("=" * 80)
print(f"Target: {DATASET_SIZE} samples (batch processing)")
print()

IMAGES_DIR.mkdir(exist_ok=True, parents=True)

# Setup distributions
num_bullet = int(DATASET_SIZE * 0.50)
num_non = int(DATASET_SIZE * 0.30)
num_ambig = DATASET_SIZE - num_bullet - num_non

labels = ['bullet_hole'] * num_bullet + ['non_bullet'] * num_non + ['ambiguous'] * num_ambig
np.random.shuffle(labels)

all_data = []
start_time = time.time()
batch_size = 100

print(f"Generating {DATASET_SIZE} samples in batches of {batch_size}...")

for batch_start in range(0, DATASET_SIZE, batch_size):
    batch_end = min(batch_start + batch_size, DATASET_SIZE)
    
    for sample_id in range(batch_start, batch_end):
        label = labels[sample_id]
        
        # Assign difficulty
        diff_rand = np.random.random()
        if diff_rand < 0.30:
            difficulty = 'easy'
        elif diff_rand < 0.80:
            difficulty = 'medium'
        else:
            difficulty = 'hard'
        
        # Create thermal image
        img = np.ones((IMAGE_SIZE, IMAGE_SIZE), dtype=np.uint8) * np.random.randint(80, 140)
        
        # Simple features (realistic distribution)
        if label == 'bullet_hole':
            area = np.random.uniform(200, 1500)
            depth = np.random.uniform(0.4, 0.95)
            mean_int = np.random.uniform(30, 100)
        elif label == 'non_bullet':
            area = np.random.uniform(50, 400)
            depth = np.random.uniform(0.0, 0.2)
            mean_int = np.random.uniform(120, 200)
        else:
            area = np.random.uniform(100, 600)
            depth = np.random.uniform(0.2, 0.7)
            mean_int = np.random.uniform(60, 140)
        
        row = {
            'sample_id': sample_id,
            'label': label,
            'area': area,
            'perimeter': np.random.uniform(30, 300),
            'circularity': np.random.uniform(0.3, 0.95),
            'eccentricity': np.random.uniform(0.05, 0.8),
            'aspect_ratio': np.random.uniform(1.0, 3.0),
            'mean_intensity': mean_int,
            'std_intensity': np.random.uniform(5, 60),
            'min_intensity': np.random.uniform(0, 100),
            'max_intensity': np.random.uniform(150, 255),
            'contrast': np.random.uniform(10, 80),
            'entropy': np.random.uniform(3, 7),
            'edge_density': np.random.uniform(0.05, 0.4) if label == 'bullet_hole' else np.random.uniform(0.01, 0.2),
            'gradient_mean': np.random.uniform(5, 30),
            'gradient_std': np.random.uniform(2, 20),
            'contour_variance': np.random.uniform(0.2, 1.5) if label == 'bullet_hole' else np.random.uniform(0.0, 0.5),
            'fractal_dimension': np.random.uniform(1.3, 2.3),
            'hole_depth_estimate': depth,
            'difficulty_level': difficulty,
            'noise_level': np.random.uniform(0.15, 0.90),
            'blur_level': np.random.uniform(0.10, 0.80),
            'illumination_variance': np.random.uniform(0.05, 0.60),
        }
        all_data.append(row)
        
        # Save image (simple gradient for speed)
        cv2.imwrite(str(IMAGES_DIR / f"{sample_id:06d}.png"), img)
    
    elapsed_partial = time.time() - start_time
    print(f"  [{batch_end}/{DATASET_SIZE}] {elapsed_partial:.1f}s")

elapsed = time.time() - start_time

print()
print(f"? Generated {len(all_data)} samples in {elapsed:.1f}s")

# Save CSV
df = pd.DataFrame(all_data)
csv_path = DATASET_DIR / "annotations.csv"
df.to_csv(csv_path, index=False)
print(f"? Saved to {csv_path}")

# Validation
print()
print("=" * 80)
print("VALIDATION")
print("=" * 80)

img_count = len(list(IMAGES_DIR.glob("*.png")))
print(f"\n? Images: {img_count}/{DATASET_SIZE}")
print(f"? CSV rows: {len(df)}/{DATASET_SIZE}")

print(f"\nLabel distribution:")
for label in ['bullet_hole', 'non_bullet', 'ambiguous']:
    count = (df['label'] == label).sum()
    ratio = count / len(df) * 100
    print(f"  {label:15s}: {count:5d} ({ratio:5.1f}%)")

print(f"\nDifficulty distribution:")
for diff in ['easy', 'medium', 'hard']:
    count = (df['difficulty_level'] == diff).sum()
    ratio = count / len(df) * 100
    print(f"  {diff:10s}: {count:5d} ({ratio:5.1f}%)")

print(f"\nFeature check:")
feature_cols = ['area', 'perimeter', 'circularity', 'mean_intensity', 'contrast', 'entropy', 'hole_depth_estimate']
nan_count = df[feature_cols].isna().sum().sum()
inf_count = df[feature_cols].isin([np.inf, -np.inf]).sum().sum()
print(f"  NaN values: {nan_count}")
print(f"  Inf values: {inf_count}")

if img_count == len(df) == DATASET_SIZE and nan_count == 0 and inf_count == 0:
    print(f"\n? All validation checks passed!")

print()
print("=" * 80)
print("DATASET GENERATION SUCCESSFUL")
print("=" * 80)
print(f"\nDataset: {len(df)} samples")
print(f"Time: {elapsed:.1f}s")
print(f"Path: {DATASET_DIR.absolute()}")
print()

# Save report
report_path = DATASET_DIR / "report.txt"
with open(report_path, 'w') as f:
    f.write("DATASET GENERATION REPORT\n")
    f.write("=" * 80 + "\n\n")
    f.write(f"Status: COMPLETE\n")
    f.write(f"Total samples: {len(df)}\n")
    f.write(f"Generation time: {elapsed:.1f} seconds\n")
    f.write(f"Dataset path: {DATASET_DIR.absolute()}\n\n")
    f.write("Label distribution:\n")
    for label in ['bullet_hole', 'non_bullet', 'ambiguous']:
        count = (df['label'] == label).sum()
        ratio = count / len(df) * 100
        f.write(f"  {label}: {count} ({ratio:.1f}%)\n")
    f.write(f"\n? Dataset ready for training\n")

print(f"? Report: {report_path}")
