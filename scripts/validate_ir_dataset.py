#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Visualize and validate the generated IR dataset"""

import numpy as np
import cv2
import pandas as pd
from pathlib import Path
import random

DATASET_DIR = Path("dataset_ir_realistic")
IMAGES_DIR = DATASET_DIR / "images"
SAMPLES_DIR = DATASET_DIR / "samples_visualization"

def visualize_samples(num_samples=10):
    """Create visualizations with annotations"""
    print(f"\n{'='*70}")
    print(f"GENERATING VISUALIZATIONS FOR {num_samples} SAMPLES")
    print(f"{'='*70}\n")
    
    SAMPLES_DIR.mkdir(parents=True, exist_ok=True)
    
    # Load annotations
    df = pd.read_csv(DATASET_DIR / "annotations.csv")
    
    # Get bullet hole images
    bullet_files = df[df['label'] == 1]['filename'].tolist()
    non_bullet_files = df[df['label'] == 0]['filename'].tolist()
    ambiguous_files = df[df['label'] == -1]['filename'].tolist()
    
    sample_idx = 0
    
    # Sample from each category
    for category, files in [("bullet", bullet_files), ("non_bullet", non_bullet_files), ("ambiguous", ambiguous_files)]:
        samples_per_category = num_samples // 3
        
        for i in range(samples_per_category):
            if i >= len(files):
                break
            
            filename = files[i]
            img_path = IMAGES_DIR / filename
            image = cv2.imread(str(img_path), cv2.IMREAD_GRAYSCALE)
            
            if image is None:
                continue
            
            # Create annotated version
            annotated = cv2.cvtColor(image, cv2.COLOR_GRAY2BGR)
            
            # Add text info
            row = df[df['filename'] == filename].iloc[0]
            cv2.putText(annotated, f"Label: {row['label']}", (10, 25), 
                       cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 255), 1)
            cv2.putText(annotated, f"Difficulty: {row['difficulty']}", (10, 50),
                       cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 255), 1)
            
            if row['label'] != 0:
                cv2.putText(annotated, f"Intensity: {row['mean_intensity']:.1f}", (10, 75),
                           cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 255), 1)
                cv2.putText(annotated, f"Area: {row['area']:.0f}", (10, 100),
                           cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 255), 1)
                
                # Draw hole circle
                center = (int(row['center_x']), int(row['center_y']))
                radius = int(row['radius'])
                cv2.circle(annotated, center, radius, (0, 255, 0), 2)
                cv2.circle(annotated, center, 3, (0, 0, 255), -1)
            
            # Save
            output_file = SAMPLES_DIR / f"sample_{sample_idx:03d}_{category}.png"
            cv2.imwrite(str(output_file), annotated)
            
            print(f"[{sample_idx+1}/{num_samples}] {output_file.name}")
            sample_idx += 1

def print_dataset_analysis():
    """Print detailed dataset analysis"""
    print(f"\n{'='*70}")
    print("DATASET ANALYSIS")
    print(f"{'='*70}\n")
    
    df = pd.read_csv(DATASET_DIR / "annotations.csv")
    
    print("Dataset Distribution:")
    print(f"  Bullet holes: {(df['label'] == 1).sum()} (50%)")
    print(f"  Non-bullets: {(df['label'] == 0).sum()} (30%)")
    print(f"  Ambiguous: {(df['label'] == -1).sum()} (20%)")
    
    bullet_df = df[df['label'] == 1]
    non_bullet_df = df[df['label'] == 0]
    
    print(f"\nBullet Hole Statistics:")
    print(f"  Area: {bullet_df['area'].min():.0f} - {bullet_df['area'].max():.0f} (mean: {bullet_df['area'].mean():.0f})")
    print(f"  Circularity: {bullet_df['circularity'].min():.3f} - {bullet_df['circularity'].max():.3f} (mean: {bullet_df['circularity'].mean():.3f})")
    print(f"  Mean Intensity: {bullet_df['mean_intensity'].min():.1f} - {bullet_df['mean_intensity'].max():.1f} (mean: {bullet_df['mean_intensity'].mean():.1f})")
    print(f"  Std Intensity: {bullet_df['std_intensity'].min():.1f} - {bullet_df['std_intensity'].max():.1f} (mean: {bullet_df['std_intensity'].mean():.1f})")
    
    print(f"\nNon-Bullet Background Statistics:")
    print(f"  Mean Intensity: {non_bullet_df['mean_intensity'].min():.1f} - {non_bullet_df['mean_intensity'].max():.1f} (mean: {non_bullet_df['mean_intensity'].mean():.1f})")
    print(f"  Std Intensity: {non_bullet_df['std_intensity'].min():.1f} - {non_bullet_df['std_intensity'].max():.1f} (mean: {non_bullet_df['std_intensity'].mean():.1f})")
    
    print(f"\nDifficulty Distribution (Bullet Holes):")
    for diff in ['easy', 'medium', 'hard']:
        count = (bullet_df['difficulty'] == diff).sum()
        pct = count / len(bullet_df) * 100
        print(f"  {diff}: {count} ({pct:.1f}%)")

def check_intensity_calibration():
    """Verify intensity ranges"""
    print(f"\n{'='*70}")
    print("INTENSITY CALIBRATION CHECK")
    print(f"{'='*70}\n")
    
    sample_files = list(IMAGES_DIR.glob("img_*.png"))[:20]
    
    for img_file in sample_files:
        image = cv2.imread(str(img_file), cv2.IMREAD_GRAYSCALE)
        
        print(f"{img_file.name}: min={image.min()}, max={image.max()}, mean={image.mean():.1f}, std={image.std():.1f}")

if __name__ == "__main__":
    print("\n" + "="*70)
    print("IR BULLET HOLE DATASET - QUALITY VERIFICATION")
    print("="*70)
    
    # Check if dataset exists
    if not DATASET_DIR.exists():
        print("ERROR: Dataset not found. Please run generate_ir_dataset_v4.py first.")
        exit(1)
    
    # Analysis
    print_dataset_analysis()
    
    # Intensity check
    check_intensity_calibration()
    
    # Visualizations
    visualize_samples(num_samples=12)
    
    print(f"\n{'='*70}")
    print("VISUALIZATION COMPLETE")
    print(f"Samples saved to: {SAMPLES_DIR}")
    print(f"{'='*70}\n")
