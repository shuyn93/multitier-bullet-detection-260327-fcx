#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Generate Full IR Bullet Hole Dataset - 10,000 images"""

import sys
import os
sys.path.insert(0, os.getcwd())

import time
from pathlib import Path

print("=" * 80)
print("GENERATING FULL IR BULLET HOLE DATASET")
print("=" * 80)
print("\nConfiguration:")
print("  - Total images: 10,000")
print("  - Image size: 256x256")
print("  - Dataset split:")
print("    * Bullet holes (50%): 5,000")
print("    * Non-bullets (30%): 3,000")
print("    * Ambiguous (20%): 2,000")
print("  - Output directory: dataset_ir_realistic/")

# Import the generator
from scripts.generate_ir_realistic_v3 import generate_dataset, display_samples

start_time = time.time()

print("\n[PHASE 1] Generating dataset...")
try:
    df = generate_dataset(10000)
    elapsed = time.time() - start_time
    print(f"\nDataset generation completed in {elapsed:.1f} seconds")
    print(f"Average: {elapsed/10000*1000:.2f}ms per image")
except Exception as e:
    print(f"ERROR during generation: {e}")
    import traceback
    traceback.print_exc()
    sys.exit(1)

print("\n[PHASE 2] Dataset Statistics:")
print(f"  Total images: {len(df)}")
print(f"  Bullet holes: {(df['label'] == 1).sum()}")
print(f"  Non-bullets: {(df['label'] == 0).sum()}")
print(f"  Ambiguous: {(df['label'] == -1).sum()}")

print("\n[PHASE 3] Feature Analysis (Bullet Holes):")
bullet_df = df[df['label'] == 1]
print(f"  Area:")
print(f"    - Mean: {bullet_df['area'].mean():.1f}")
print(f"    - Std:  {bullet_df['area'].std():.1f}")
print(f"    - Range: {bullet_df['area'].min():.1f} - {bullet_df['area'].max():.1f}")

print(f"  Circularity:")
print(f"    - Mean: {bullet_df['circularity'].mean():.3f}")
print(f"    - Std:  {bullet_df['circularity'].std():.3f}")

print(f"  Mean Intensity:")
print(f"    - Bullet holes: {bullet_df['mean_intensity'].mean():.1f}")
print(f"    - Background: {df[df['label']==0]['mean_intensity'].mean():.1f}")

print("\n[PHASE 4] Visual Quality Check - Generating 10 sample visualizations...")
try:
    display_samples(num_samples=10)
    print("  Samples saved to: dataset_ir_realistic/samples_visualization/")
except Exception as e:
    print(f"  Warning: Could not generate visualizations: {e}")

print("\n" + "=" * 80)
print("DATASET GENERATION COMPLETE!")
print("=" * 80)
print(f"Location: dataset_ir_realistic/")
print(f"Images: dataset_ir_realistic/images/ ({len(df)} files)")
print(f"Annotations: dataset_ir_realistic/annotations.csv")
print(f"Visualizations: dataset_ir_realistic/samples_visualization/")
print("=" * 80)
