# -*- coding: utf-8 -*-
"""Quick test with small dataset"""
import sys
sys.path.insert(0, '.')

import numpy as np
import cv2
import pandas as pd
from pathlib import Path

np.random.seed(42)

DATASET_SIZE = 50  # Small test
IMAGE_SIZE = 256
DATASET_DIR = Path("dataset")
IMAGES_DIR = DATASET_DIR / "images"

IMAGES_DIR.mkdir(exist_ok=True, parents=True)

print("Quick test: generating 50 samples...")

all_data = []
for i in range(DATASET_SIZE):
    # Simple image
    img = np.random.uniform(80, 140, (IMAGE_SIZE, IMAGE_SIZE)).astype(np.uint8)
    cv2.imwrite(str(IMAGES_DIR / f"{i:06d}.png"), img)
    
    # Features
    row = {
        'sample_id': i,
        'label': np.random.choice(['bullet_hole', 'non_bullet', 'ambiguous']),
        'area': np.random.uniform(0, 2000),
        'perimeter': np.random.uniform(0, 300),
        'circularity': np.random.uniform(0, 1),
        'eccentricity': np.random.uniform(0, 1),
        'aspect_ratio': np.random.uniform(1, 5),
        'mean_intensity': np.random.uniform(0, 255),
        'std_intensity': np.random.uniform(0, 80),
        'min_intensity': np.random.uniform(0, 200),
        'max_intensity': np.random.uniform(100, 255),
        'contrast': np.random.uniform(0, 100),
        'entropy': np.random.uniform(0, 8),
        'edge_density': np.random.uniform(0, 1),
        'gradient_mean': np.random.uniform(0, 50),
        'gradient_std': np.random.uniform(0, 30),
        'contour_variance': np.random.uniform(0, 2),
        'fractal_dimension': np.random.uniform(1, 3),
        'hole_depth_estimate': np.random.uniform(0, 1),
        'difficulty_level': np.random.choice(['easy', 'medium', 'hard']),
        'noise_level': np.random.uniform(0.1, 0.9),
        'blur_level': np.random.uniform(0.1, 0.8),
        'illumination_variance': np.random.uniform(0.05, 0.6),
    }
    all_data.append(row)
    
    if (i + 1) % 10 == 0:
        print(f"  {i + 1}/{DATASET_SIZE}")

df = pd.DataFrame(all_data)
csv_path = DATASET_DIR / "annotations.csv"
df.to_csv(csv_path, index=False)

print(f"? Generated {len(df)} samples")
print(f"? Images: {len(list(IMAGES_DIR.glob('*.png')))}")
print(f"? CSV: {csv_path}")
