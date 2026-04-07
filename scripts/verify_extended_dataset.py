"""
Dataset Validation and Visual Inspection Script

Displays random samples from the extended dataset and validates consistency
"""

import numpy as np
import cv2
import pandas as pd
from pathlib import Path
import matplotlib.pyplot as plt
import random

DATASET_DIR = Path("dataset_ir_realistic")
IMAGES_DIR = DATASET_DIR / "images"

def validate_dataset():
    """Comprehensive dataset validation"""
    
    print("\n" + "=" * 80)
    print("DATASET VALIDATION - EXTENDED IR BULLET HOLE DATASET")
    print("=" * 80)
    
    # Load annotations
    csv_path = DATASET_DIR / "annotations.csv"
    df = pd.read_csv(csv_path)
    
    print(f"\n[1/6] Basic Statistics")
    print("-" * 80)
    print(f"Total samples: {len(df):,}")
    print(f"Dataset directory: {DATASET_DIR.absolute()}")
    print(f"Images directory: {IMAGES_DIR.absolute()}")
    
    # Check file consistency
    print(f"\n[2/6] File Consistency Check")
    print("-" * 80)
    
    # Verify all images exist
    missing_images = []
    for filename in df['filename']:
        image_path = IMAGES_DIR / filename
        if not image_path.exists():
            missing_images.append(filename)
    
    image_files = list(IMAGES_DIR.glob("*.png"))
    
    print(f"Expected images (from CSV): {len(df)}")
    print(f"Actual images (on disk):    {len(image_files)}")
    print(f"Missing images:             {len(missing_images)}")
    
    if missing_images:
        print(f"? WARNING: {len(missing_images)} images referenced in CSV but not found!")
        print(f"  Examples: {missing_images[:5]}")
    else:
        print(f"? All images exist and are accounted for")
    
    # Check CSV consistency
    print(f"\n[3/6] CSV Data Quality")
    print("-" * 80)
    
    null_counts = df.isnull().sum()
    total_nulls = null_counts.sum()
    
    print(f"Null values per column:")
    for col in df.columns:
        if df[col].isnull().sum() > 0:
            print(f"  {col:20s}: {df[col].isnull().sum():5d}")
    
    print(f"Total null values: {total_nulls}")
    
    # Label distribution
    print(f"\n[4/6] Label Distribution")
    print("-" * 80)
    
    label_map = {1: "Bullet Hole", 0: "Non-Bullet", -1: "Ambiguous"}
    label_dist = df['label'].value_counts().sort_index()
    
    for label, count in label_dist.items():
        pct = 100 * count / len(df)
        label_name = label_map.get(label, f"Unknown ({label})")
        print(f"  {label_name:15s}: {count:6d} ({pct:5.1f}%)")
    
    # Difficulty distribution
    print(f"\n[5/6] Difficulty Distribution")
    print("-" * 80)
    
    diff_dist = df['difficulty'].value_counts().sort_values(ascending=False)
    for diff, count in diff_dist.items():
        pct = 100 * count / len(df)
        print(f"  {str(diff):15s}: {count:6d} ({pct:5.1f}%)")
    
    # Feature statistics
    print(f"\n[6/6] Feature Statistics")
    print("-" * 80)
    
    print(f"\nAll Samples:")
    print(f"  Mean Intensity:  {df['mean_intensity'].describe()}")
    print(f"\n  Std Intensity:   {df['std_intensity'].describe()}")
    
    print(f"\n\nBullet Hole Samples (label=1):")
    bullet_df = df[df['label'] == 1]
    if len(bullet_df) > 0:
        print(f"  Count: {len(bullet_df):,}")
        print(f"  Area:      {bullet_df['area'].describe()}")
        print(f"  Radius:    {bullet_df['radius'].describe()}")
        print(f"  Intensity: {bullet_df['mean_intensity'].describe()}")
    
    print(f"\n\nNon-Bullet Samples (label=0):")
    non_bullet_df = df[df['label'] == 0]
    if len(non_bullet_df) > 0:
        print(f"  Count: {len(non_bullet_df):,}")
        print(f"  Intensity: {non_bullet_df['mean_intensity'].describe()}")
    
    print(f"\n\nAmbiguous Samples (label=-1):")
    ambiguous_df = df[df['label'] == -1]
    if len(ambiguous_df) > 0:
        print(f"  Count: {len(ambiguous_df):,}")
        print(f"  Intensity: {ambiguous_df['mean_intensity'].describe()}")
    
    print("\n" + "=" * 80)
    print("VALIDATION COMPLETE")
    print("=" * 80)

def display_random_samples(num_samples=5):
    """Display random samples from the new dataset"""
    
    print("\n" + "=" * 80)
    print("VISUAL INSPECTION - RANDOM SAMPLES")
    print("=" * 80)
    
    csv_path = DATASET_DIR / "annotations.csv"
    df = pd.read_csv(csv_path)
    
    # Get samples only from the newly added ones (ID >= 10000)
    new_samples_df = df[df.index >= 10000].copy()
    
    print(f"\nShowing {min(num_samples, len(new_samples_df))} random newly-generated samples...\n")
    
    if len(new_samples_df) == 0:
        print("No new samples found!")
        return
    
    # Sample indices
    sample_indices = random.sample(range(len(new_samples_df)), min(num_samples, len(new_samples_df)))
    
    for idx, sample_idx in enumerate(sample_indices):
        sample = new_samples_df.iloc[sample_idx]
        filename = sample['filename']
        image_path = IMAGES_DIR / filename
        
        if not image_path.exists():
            print(f"Sample {idx + 1}: {filename} - IMAGE NOT FOUND")
            continue
        
        # Load image
        image = cv2.imread(str(image_path), cv2.IMREAD_GRAYSCALE)
        
        # Prepare info
        label_map = {1: "Bullet Hole", 0: "Non-Bullet", -1: "Ambiguous"}
        label = label_map.get(sample['label'], f"Unknown ({sample['label']})")
        
        print(f"Sample {idx + 1}:")
        print(f"  Filename:      {filename}")
        print(f"  Label:         {label}")
        print(f"  Difficulty:    {sample['difficulty']}")
        print(f"  Mean Intensity: {sample['mean_intensity']:.1f}")
        print(f"  Std Intensity:  {sample['std_intensity']:.1f}")
        print(f"  Area:          {sample['area']:.1f}")
        print(f"  Radius:        {sample['radius']:.2f}")
        print(f"  Circularity:   {sample['circularity']:.3f}")
        print()

def display_comparative_stats():
    """Display before/after statistics"""
    
    print("\n" + "=" * 80)
    print("DATASET EXTENSION SUMMARY")
    print("=" * 80)
    
    csv_path = DATASET_DIR / "annotations.csv"
    df = pd.read_csv(csv_path)
    
    # Split into original and new
    original_df = df[df.index < 10000]
    new_df = df[df.index >= 10000]
    
    print(f"\n[BEFORE EXTENSION]")
    print(f"  Total samples:      {len(original_df):,}")
    print(f"  Bullet holes:       {(original_df['label'] == 1).sum():,}")
    print(f"  Non-bullets:        {(original_df['label'] == 0).sum():,}")
    print(f"  Ambiguous:          {(original_df['label'] == -1).sum():,}")
    
    print(f"\n[NEW SAMPLES ADDED]")
    print(f"  Total samples:      {len(new_df):,}")
    print(f"  Bullet holes:       {(new_df['label'] == 1).sum():,}")
    print(f"  Non-bullets:        {(new_df['label'] == 0).sum():,}")
    print(f"  Ambiguous:          {(new_df['label'] == -1).sum():,}")
    
    print(f"\n[AFTER EXTENSION]")
    print(f"  Total samples:      {len(df):,}")
    print(f"  Bullet holes:       {(df['label'] == 1).sum():,} ({100*(df['label'] == 1).sum()/len(df):.1f}%)")
    print(f"  Non-bullets:        {(df['label'] == 0).sum():,} ({100*(df['label'] == 0).sum()/len(df):.1f}%)")
    print(f"  Ambiguous:          {(df['label'] == -1).sum():,} ({100*(df['label'] == -1).sum()/len(df):.1f}%)")
    
    print(f"\n[GROWTH METRICS]")
    print(f"  Samples increased by:  {100 * len(new_df) / len(original_df):.1f}%")
    print(f"  New/Original ratio:    {len(new_df) / len(original_df):.2f}x")

if __name__ == '__main__':
    validate_dataset()
    display_comparative_stats()
    display_random_samples(num_samples=5)
    
    print("\n" + "=" * 80)
    print("VERIFICATION COMPLETE")
    print("=" * 80)
