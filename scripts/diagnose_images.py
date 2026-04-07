#!/usr/bin/env python3
"""
STEP 1: DIAGNOSE BLACK IMAGE ISSUE
Analyze pixel value distributions and image quality
"""

import numpy as np
from pathlib import Path
import cv2
from PIL import Image
import matplotlib.pyplot as plt

def diagnose_images():
    """Analyze sample images for visibility issues"""
    
    images_dir = Path(r'C:\Users\Admin\source\repos\bullet_hole_detection_system\dataset\images')
    csv_path = Path(r'C:\Users\Admin\source\repos\bullet_hole_detection_system\dataset\annotations.csv')
    
    if not images_dir.exists():
        print("ERROR: Images directory not found!")
        return
    
    # Get image files
    image_files = sorted(list(images_dir.glob('*.png')))[:10]  # First 10 images
    
    import pandas as pd
    df = pd.read_csv(csv_path)
    
    print("\n" + "="*80)
    print("STEP 1: DIAGNOSIS - IMAGE QUALITY ANALYSIS")
    print("="*80)
    
    all_intensity_data = []
    
    for img_path in image_files:
        sample_id = int(img_path.stem)
        
        # Load image
        img = cv2.imread(str(img_path), cv2.IMREAD_GRAYSCALE)
        if img is None:
            print(f"ERROR: Could not load {img_path}")
            continue
        
        # Get label
        sample_row = df[df['sample_id'] == sample_id]
        if sample_row.empty:
            print(f"WARNING: No annotation for sample {sample_id}")
            continue
        
        label = sample_row.iloc[0]['label']
        
        # Analyze pixel values
        min_val = np.min(img)
        max_val = np.max(img)
        mean_val = np.mean(img)
        std_val = np.std(img)
        median_val = np.median(img)
        
        # Count pixels in different ranges
        pixels_black = np.sum(img < 10)
        pixels_dark = np.sum((img >= 10) & (img < 50))
        pixels_mid = np.sum((img >= 50) & (img < 150))
        pixels_bright = np.sum((img >= 150) & (img < 255))
        pixels_white = np.sum(img >= 255)
        
        all_intensity_data.append(mean_val)
        
        print(f"\nSample {sample_id:06d} - Label: {label}")
        print(f"  Pixel Range: [{min_val}, {max_val}] (span: {max_val-min_val})")
        print(f"  Mean: {mean_val:.2f}, Median: {median_val:.2f}, Std: {std_val:.2f}")
        print(f"  Distribution:")
        print(f"    Black [0-10]:       {pixels_black:6d} pixels ({100*pixels_black/(img.size):.1f}%)")
        print(f"    Dark [10-50]:       {pixels_dark:6d} pixels ({100*pixels_dark/(img.size):.1f}%)")
        print(f"    Mid [50-150]:       {pixels_mid:6d} pixels ({100*pixels_mid/(img.size):.1f}%)")
        print(f"    Bright [150-255]:   {pixels_bright:6d} pixels ({100*pixels_bright/(img.size):.1f}%)")
        print(f"    White [255]:        {pixels_white:6d} pixels ({100*pixels_white/(img.size):.1f}%)")
    
    print(f"\n" + "="*80)
    print("ANALYSIS SUMMARY")
    print("="*80)
    
    if len(all_intensity_data) > 0:
        avg_mean_intensity = np.mean(all_intensity_data)
        print(f"\nAverage mean intensity across {len(all_intensity_data)} samples: {avg_mean_intensity:.2f}")
        
        if avg_mean_intensity < 30:
            print("\n*** CRITICAL ISSUE DETECTED ***")
            print("Images are TOO DARK (average < 30)")
            print("\nROOT CAUSE ANALYSIS:")
            print("1. Hole generation sets pixels to: img * 0.3 (darkens too much)")
            print("2. Intensity values are not properly scaled between [0-255]")
            print("3. Background thermal baseline too low")
            print("4. Clipping to [0,255] but values concentrated in dark range")
        elif avg_mean_intensity < 50:
            print("\n*** MODERATE ISSUE DETECTED ***")
            print("Images are quite dark (average 30-50)")
            print("Visibility is compromised")
        else:
            print(f"\n? Image intensity appears reasonable: {avg_mean_intensity:.2f}")

if __name__ == '__main__':
    diagnose_images()
