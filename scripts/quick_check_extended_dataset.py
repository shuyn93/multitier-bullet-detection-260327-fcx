# -*- coding: utf-8 -*-
"""
Quick Visual Check - Display newly generated samples from dataset_ir_realistic
"""

import cv2
import pandas as pd
from pathlib import Path
import numpy as np

DATASET_DIR = Path("dataset_ir_realistic")
IMAGES_DIR = DATASET_DIR / "images"

def display_quick_check():
    """Quick check showing 5 random newly generated samples"""
    
    print("\n" + "=" * 80)
    print("QUICK VISUAL CHECK - NEWLY GENERATED SAMPLES")
    print("=" * 80)
    
    csv_path = DATASET_DIR / "annotations.csv"
    df = pd.read_csv(csv_path)
    
    # Get new samples (indices >= 10000)
    new_df = df[df.index >= 10000].reset_index(drop=True)
    
    print(f"\nNew samples available: {len(new_df)}")
    
    # Pick 5 random samples
    np.random.seed(42)
    sample_indices = np.random.choice(len(new_df), size=min(5, len(new_df)), replace=False)
    
    print(f"\nDisplaying 5 random newly generated samples:\n")
    
    for i, idx in enumerate(sample_indices, 1):
        sample = new_df.iloc[idx]
        filename = sample['filename']
        image_path = IMAGES_DIR / filename
        
        # Map labels
        label_map = {1: "BULLET_HOLE", 0: "NON_BULLET", -1: "AMBIGUOUS"}
        label_text = label_map.get(sample['label'], "UNKNOWN")
        
        # Read image
        img = cv2.imread(str(image_path), cv2.IMREAD_GRAYSCALE)
        
        if img is None:
            print(f"[{i}] {filename:20s} - ERROR: IMAGE NOT FOUND")
            continue
        
        # Image statistics
        img_min, img_max = img.min(), img.max()
        img_mean, img_std = img.mean(), img.std()
        
        print(f"[{i}] {filename}")
        print(f"    Label:          {label_text}")
        print(f"    Difficulty:     {sample['difficulty']}")
        print(f"    Image min/max:  {img_min:3d} / {img_max:3d}")
        print(f"    Image mean:     {img_mean:6.1f} +- {img_std:5.1f}")
        print(f"    Area:           {sample['area']:8.1f} px2")
        print(f"    Radius:         {sample['radius']:6.2f} px")
        print(f"    Circularity:    {sample['circularity']:6.3f}")
        print()

def quick_stats():
    """Quick stats summary"""
    
    print("\n" + "=" * 80)
    print("DATASET QUICK STATISTICS")
    print("=" * 80)
    
    csv_path = DATASET_DIR / "annotations.csv"
    df = pd.read_csv(csv_path)
    
    print(f"\nTotal samples:        {len(df):,}")
    print(f"Total images on disk: {len(list(IMAGES_DIR.glob('*.png'))):,}")
    
    # Label breakdown
    label_counts = df['label'].value_counts()
    print(f"\nLabel distribution:")
    print(f"  Bullet holes  (1):  {label_counts.get(1, 0):,} ({100*label_counts.get(1, 0)/len(df):.1f}%)")
    print(f"  Non-bullets   (0):  {label_counts.get(0, 0):,} ({100*label_counts.get(0, 0)/len(df):.1f}%)")
    print(f"  Ambiguous    (-1):  {label_counts.get(-1, 0):,} ({100*label_counts.get(-1, 0)/len(df):.1f}%)")
    
    # Difficulty breakdown
    print(f"\nDifficulty distribution:")
    for diff in ['easy', 'medium', 'hard', 'N/A', 'ambiguous']:
        count = (df['difficulty'] == diff).sum()
        if count > 0:
            print(f"  {diff:10s}: {count:,} ({100*count/len(df):.1f}%)")
    
    # Feature ranges (bullet holes only)
    bullet_df = df[df['label'] == 1]
    if len(bullet_df) > 0:
        print(f"\nBullet hole feature ranges:")
        print(f"  Area:    {bullet_df['area'].min():.0f} - {bullet_df['area'].max():.0f} (mean={bullet_df['area'].mean():.0f})")
        print(f"  Radius:  {bullet_df['radius'].min():.1f} - {bullet_df['radius'].max():.1f} (mean={bullet_df['radius'].mean():.1f})")
        print(f"  Intensity: {bullet_df['mean_intensity'].min():.1f} - {bullet_df['mean_intensity'].max():.1f} (mean={bullet_df['mean_intensity'].mean():.1f})")

if __name__ == '__main__':
    quick_stats()
    display_quick_check()
    
    print("\n" + "=" * 80)
    print("Quick check complete!")
    print("=" * 80)

