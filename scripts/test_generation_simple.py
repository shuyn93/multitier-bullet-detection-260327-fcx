#!/usr/bin/env python
# -*- coding: utf-8 -*-

import sys
import os
sys.path.insert(0, os.getcwd())

import numpy as np
import cv2

print("Generating realistic IR bullet hole test dataset...")
print("")

try:
    # Test imports
    from scipy import ndimage
    from scipy.ndimage import gaussian_filter
    from scipy.interpolate import RegularGridInterpolator
    import pandas as pd
    from pathlib import Path
    
    print("[OK] All imports successful")
    
    # Create directories
    output_dir = Path("dataset_realistic_test")
    images_dir = output_dir / "images"
    output_dir.mkdir(exist_ok=True)
    images_dir.mkdir(exist_ok=True)
    
    print("[OK] Directories created")
    
    # Generate simple test image
    IMAGE_SIZE = 256
    
    # Simple background
    bg = np.ones((IMAGE_SIZE, IMAGE_SIZE), dtype=np.float32) * 140.0
    
    # Add simple gradient
    for i in range(IMAGE_SIZE):
        for j in range(IMAGE_SIZE):
            bg[i, j] += 10 * np.sin(2 * np.pi * i / IMAGE_SIZE)
    
    # Add simple hole
    cy, cx = IMAGE_SIZE // 2, IMAGE_SIZE // 2
    for i in range(IMAGE_SIZE):
        for j in range(IMAGE_SIZE):
            dist = np.sqrt((i - cy) ** 2 + (j - cx) ** 2)
            if dist < 20:
                # Dark center with gradient
                bg[i, j] = 40 + 60 * (dist / 20.0)
            elif dist < 25:
                # Bright edge ring
                bg[i, j] = 200
    
    # Clip and convert
    img = np.clip(bg, 0, 255).astype(np.uint8)
    
    # Save test image
    cv2.imwrite(str(images_dir / "000000.png"), img)
    print("[OK] Test image created: 000000.png")
    
    # Create simple annotations
    data = {
        'sample_id': [0],
        'label': ['bullet_hole'],
        'area': [1200],
        'perimeter': [150],
        'circularity': [0.8],
        'eccentricity': [0.2],
        'aspect_ratio': [1.0],
        'mean_intensity': [140.0],
        'std_intensity': [30.0],
        'min_intensity': [40.0],
        'max_intensity': [200.0],
        'contrast': [50.0],
        'entropy': [5.0],
        'edge_density': [0.3],
        'gradient_mean': [15.0],
        'gradient_std': [5.0],
        'contour_variance': [0.15],
        'fractal_dimension': [2.0],
        'hole_depth_estimate': [0.7],
        'difficulty_level': ['easy'],
    }
    
    df = pd.DataFrame(data)
    df.to_csv(str(output_dir / "annotations.csv"), index=False)
    print("[OK] Annotations created")
    
    print("")
    print("SUCCESS: Test dataset created at " + str(output_dir.absolute()))
    print("Next: Run validation script to verify quality")
    
except Exception as e:
    print("ERROR: " + str(e))
    import traceback
    traceback.print_exc()
    sys.exit(1)
