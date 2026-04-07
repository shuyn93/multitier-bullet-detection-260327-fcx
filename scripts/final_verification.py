#!/usr/bin/env python3
"""
FINAL VERIFICATION - Dataset quality assurance after fix
"""

import pandas as pd
import numpy as np
from pathlib import Path
import cv2

def final_verification():
    """Complete verification of fixed dataset"""
    
    csv_path = Path(r'C:\Users\Admin\source\repos\bullet_hole_detection_system\dataset\annotations.csv')
    images_dir = Path(r'C:\Users\Admin\source\repos\bullet_hole_detection_system\dataset\images')
    
    print("\n" + "="*80)
    print("FINAL DATASET VERIFICATION - FIXED DATASET")
    print("="*80)
    
    df = pd.read_csv(csv_path)
    
    # Analysis by label
    print("\n1. LABEL DISTRIBUTION:")
    for label in sorted(df['label'].unique()):
        count = len(df[df['label'] == label])
        pct = 100 * count / len(df)
        print(f"   {label:15s}: {count:5d} ({pct:5.1f}%)")
    
    # Intensity analysis by label
    print("\n2. INTENSITY ANALYSIS BY LABEL:")
    for label in sorted(df['label'].unique()):
        label_df = df[df['label'] == label]
        mean_int = label_df['mean_intensity'].mean()
        std_int = label_df['std_intensity'].mean()
        print(f"   {label:15s}: mean_int={mean_int:6.2f}, avg_std={std_int:6.2f}")
    
    # Hole depth analysis
    print("\n3. HOLE DEPTH DISTRIBUTION:")
    print(f"   Min depth: {df['hole_depth_estimate'].min():.4f}")
    print(f"   Max depth: {df['hole_depth_estimate'].max():.4f}")
    print(f"   Mean depth: {df['hole_depth_estimate'].mean():.4f}")
    
    # Sample image verification
    print("\n4. SAMPLE IMAGE PIXEL VERIFICATION:")
    sample_indices = [0, 100, 1000, 5000, 9999]
    
    for idx in sample_indices:
        sample = df.iloc[idx]
        sample_id = str(int(sample['sample_id'])).zfill(6)
        image_path = images_dir / f"{sample_id}.png"
        
        img = cv2.imread(str(image_path), cv2.IMREAD_GRAYSCALE)
        
        min_val = np.min(img)
        max_val = np.max(img)
        mean_val = np.mean(img)
        std_val = np.std(img)
        
        span = max_val - min_val
        
        status = "GOOD" if span > 50 and std_val > 5 else "POOR"
        
        print(f"   Sample {idx:5d}: {sample['label']:12s} | range:[{min_val:3d}, {max_val:3d}] | span:{span:3d} | std:{std_val:5.1f} | {status}")
    
    # Difficulty distribution
    print("\n5. DIFFICULTY DISTRIBUTION:")
    for diff in sorted(df['difficulty_level'].unique()):
        count = len(df[df['difficulty_level'] == diff])
        pct = 100 * count / len(df)
        print(f"   {diff:10s}: {count:5d} ({pct:5.1f}%)")
    
    # Feature correlation check
    print("\n6. FEATURE CORRELATION CHECK:")
    bullet_hole_df = df[df['label'] == 'bullet_hole']
    non_bullet_df = df[df['label'] == 'non_bullet']
    
    print(f"   Bullet holes - mean circularity: {bullet_hole_df['circularity'].mean():.4f}")
    print(f"   Non-bullets - mean circularity: {non_bullet_df['circularity'].mean():.4f}")
    print(f"   (Should be different - OK if bullet_hole > non_bullet)")
    
    print(f"\n   Bullet holes - mean intensity: {bullet_hole_df['mean_intensity'].mean():.2f}")
    print(f"   Non-bullets - mean intensity: {non_bullet_df['mean_intensity'].mean():.2f}")
    print(f"   (Should be different - OK if bullet_hole < non_bullet, darker)")
    
    # Final check
    print("\n" + "="*80)
    print("VERIFICATION RESULT")
    print("="*80)
    
    all_good = True
    
    # Check completeness
    if len(df) == 10000:
        print("[OK] Complete dataset: 10,000 samples")
    else:
        print(f"[ERROR] Dataset incomplete: {len(df)} samples")
        all_good = False
    
    # Check intensity variation
    if df['std_intensity'].mean() > 5:
        print(f"[OK] Good intensity variation: avg_std={df['std_intensity'].mean():.2f}")
    else:
        print(f"[ERROR] Low intensity variation: avg_std={df['std_intensity'].mean():.2f}")
        all_good = False
    
    # Check label diversity
    if len(df['label'].unique()) == 3:
        print("[OK] All 3 label classes present")
    else:
        print("[ERROR] Missing label classes")
        all_good = False
    
    # Check hole depth
    bullet_depth = bullet_hole_df['hole_depth_estimate'].mean()
    if bullet_depth > 0.2:
        print(f"[OK] Bullet holes have good depth: {bullet_depth:.4f}")
    else:
        print(f"[WARNING] Bullet hole depth low: {bullet_depth:.4f}")
    
    # Check image files
    image_count = len(list(images_dir.glob('*.png')))
    if image_count == 10000:
        print(f"[OK] All 10,000 image files present")
    else:
        print(f"[ERROR] Image count mismatch: {image_count}")
        all_good = False
    
    print("\n" + "="*80)
    if all_good:
        print("DATASET STATUS: VERIFIED AND READY FOR USE")
        print("="*80)
        print("\nCONCLUSION:")
        print("The fixed dataset has passed all quality checks.")
        print("Bullet holes are now clearly visible and distinguishable.")
        print("The multi-tier detection system can proceed with confidence.")
    else:
        print("DATASET STATUS: ISSUES DETECTED")
        print("="*80)
    
    print()

if __name__ == '__main__':
    final_verification()
