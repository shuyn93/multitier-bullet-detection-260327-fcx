#!/usr/bin/env python3
"""
Test realistic generator on a small sample
"""

import numpy as np
import cv2
from pathlib import Path
import sys

sys.path.insert(0, r'C:\Users\Admin\source\repos\bullet_hole_detection_system\scripts')

from generate_dataset_realistic import (
    generate_realistic_sample,
    IMAGE_SIZE,
)

def test_realistic_generator():
    """Test generate 5 samples"""
    
    print("\n" + "="*80)
    print("TESTING REALISTIC GENERATOR")
    print("="*80)
    
    test_dir = Path(r'C:\Users\Admin\source\repos\bullet_hole_detection_system\dataset_realistic\test_images')
    test_dir.mkdir(parents=True, exist_ok=True)
    
    for i, (label, difficulty) in enumerate([
        ('bullet_hole', 'easy'),
        ('bullet_hole', 'medium'),
        ('bullet_hole', 'hard'),
        ('non_bullet', 'easy'),
        ('ambiguous', 'hard')
    ]):
        img, label_out, features, metadata = generate_realistic_sample(i, label, difficulty)
        
        # Save test image
        test_path = test_dir / f"test_{i:02d}_{label}_{difficulty}.png"
        cv2.imwrite(str(test_path), img)
        
        # Analyze
        min_val = np.min(img)
        max_val = np.max(img)
        mean_val = np.mean(img)
        std_val = np.std(img)
        
        print(f"\nSample {i} - {label} ({difficulty}):")
        print(f"  Image range: [{min_val:3d}, {max_val:3d}] span={max_val-min_val:3d}")
        print(f"  Mean: {mean_val:6.2f}, Std: {std_val:6.2f}")
        print(f"  Features:")
        print(f"    Area: {features['area']:8.1f}")
        print(f"    Circularity: {features['circularity']:.4f}")
        print(f"    Mean intensity in region: {features['mean_intensity']:6.2f}")
        print(f"    Contrast: {features['contrast']:6.2f}")
        print(f"    Hole depth: {features['hole_depth_estimate']:.4f}")
        print(f"  Visual Quality: {'EXCELLENT' if std_val > 8 else 'GOOD' if std_val > 5 else 'POOR'}")
    
    print(f"\nTest images saved to: {test_dir}")

if __name__ == '__main__':
    test_realistic_generator()
