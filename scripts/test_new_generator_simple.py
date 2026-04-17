#!/usr/bin/env python3
"""Test realistic bullet hole generator"""

import sys
import numpy as np
import cv2
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))

from realistic_bullet_hole_generator import RealisticBulletHoleGenerator


def test_generator():
    """Test the realistic generator"""
    print("=" * 70)
    print("TESTING REALISTIC BULLET HOLE GENERATOR")
    print("=" * 70)
    
    gen = RealisticBulletHoleGenerator(image_size=256)
    
    print("\n[1] Generating 12 sample images...")
    images, labels = gen.generate_batch(num_samples=12)
    
    # Analyze
    print("\n[2] Analysis Results:")
    print("-" * 70)
    print(f"{'Class':<15} {'Max':<8} {'Mean':<8} {'Min':<8} {'Realistic':<12}")
    print("-" * 70)
    
    realistic_count = 0
    for img, lbl in zip(images, labels):
        stats = gen.analyze_generated_image(img, lbl)
        is_real = "? YES" if stats['is_realistic'] else "? NO"
        realistic_count += stats['is_realistic']
        
        print(f"{lbl:<15} {stats['max_intensity']:<8.0f} "
              f"{stats['mean_intensity']:<8.0f} {stats['min_intensity']:<8.0f} {is_real:<12}")
    
    print("-" * 70)
    print(f"Realistic: {realistic_count}/12 ({100*realistic_count/12:.0f}%)\n")
    
    # Key metrics
    print("[3] Key Metrics:")
    print("-" * 70)
    
    max_intensities = [img.max() for img in images]
    
    print(f"Max intensity across all:")
    print(f"  Min:  {min(max_intensities):.0f}")
    print(f"  Mean: {np.mean(max_intensities):.0f}")
    print(f"  Max:  {max(max_intensities):.0f}")
    print(f"  Target: ? 230")
    
    saturated = sum(1 for i in images if i.max() == 255)
    print(f"\nSaturated pixels (255): {saturated}")
    print(f"  Target: 0")
    
    print("\n" + "=" * 70)
    if saturated == 0 and max(max_intensities) <= 230:
        print("? TEST PASSED - Generator working correctly!")
    else:
        print("? TEST WARNING - Check values above")
    print("=" * 70)
    
    return images


if __name__ == '__main__':
    images = test_generator()
