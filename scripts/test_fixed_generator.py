#!/usr/bin/env python3
"""
Test the fixed generator on a small sample before full regeneration
"""

import numpy as np
import cv2
from pathlib import Path
import sys

sys.path.insert(0, r'C:\Users\Admin\source\repos\bullet_hole_detection_system\scripts')

from generate_dataset_fixed import (
    generate_synthetic_sample_fixed,
    IMAGE_SIZE,
    DATASET_DIR,
    IMAGES_DIR
)

def test_fixed_generator():
    """Test generate 5 samples and analyze them"""
    
    print("\n" + "="*80)
    print("TESTING FIXED GENERATOR")
    print("="*80)
    
    DATASET_DIR.mkdir(exist_ok=True)
    test_dir = DATASET_DIR / "test_images"
    test_dir.mkdir(exist_ok=True)
    
    for i, label in enumerate(['bullet_hole', 'non_bullet', 'ambiguous', 'bullet_hole', 'non_bullet']):
        img, label_out, features, metadata = generate_synthetic_sample_fixed(i, label)
        
        # Save test image
        test_path = test_dir / f"test_{i:02d}_{label}.png"
        cv2.imwrite(str(test_path), img)
        
        # Analyze
        min_val = np.min(img)
        max_val = np.max(img)
        mean_val = np.mean(img)
        std_val = np.std(img)
        
        print(f"\nSample {i} - {label}:")
        print(f"  Range: [{min_val}, {max_val}] (span: {max_val-min_val})")
        print(f"  Mean: {mean_val:.2f}, Std: {std_val:.2f}")
        print(f"  Features:")
        print(f"    Area: {features['area']:.1f}")
        print(f"    Circularity: {features['circularity']:.4f}")
        print(f"    Mean intensity in feature: {features['mean_intensity']:.2f}")
        print(f"    Contrast: {features['contrast']:.2f}")
        
        if std_val < 5:
            print(f"  WARNING: Still low variation (std={std_val:.2f})")
        else:
            print(f"  OK: Good variation (std={std_val:.2f})")
    
    print(f"\nTest images saved to: {test_dir}")

if __name__ == '__main__':
    test_fixed_generator()
