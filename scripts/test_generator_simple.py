#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Test IR Bullet Hole Dataset Generator"""

import sys
import os
sys.path.insert(0, os.getcwd())

import numpy as np
import cv2
from pathlib import Path

print("=" * 70)
print("IR BULLET HOLE DATASET GENERATOR - TEST RUN")
print("=" * 70)

# Import the generator
from scripts.generate_ir_realistic_v3 import (
    generate_bullet_hole_image,
    generate_non_bullet_image,
    generate_ambiguous_image,
    display_samples
)

# Create test directory
test_dir = Path("dataset_ir_test")
test_dir.mkdir(exist_ok=True)
(test_dir / "images").mkdir(exist_ok=True)

print("\n[TEST 1] Generating sample bullet hole image...")
try:
    image, center, radius = generate_bullet_hole_image((256, 256), difficulty='medium')
    cv2.imwrite(str(test_dir / "images" / "test_bullet_001.png"), image)
    print(f"  SUCCESS - Image shape: {image.shape}")
    print(f"  Intensity range: {image.min()}-{image.max()}")
    print(f"  Hole center: {center}, radius: {radius:.1f}")
except Exception as e:
    print(f"  ERROR: {e}")

print("\n[TEST 2] Generating sample non-bullet image...")
try:
    image = generate_non_bullet_image((256, 256))
    cv2.imwrite(str(test_dir / "images" / "test_nonbullet_001.png"), image)
    print(f"  SUCCESS - Image shape: {image.shape}")
    print(f"  Intensity range: {image.min()}-{image.max()}")
except Exception as e:
    print(f"  ERROR: {e}")

print("\n[TEST 3] Generating sample ambiguous image...")
try:
    image, center, radius = generate_ambiguous_image((256, 256))
    cv2.imwrite(str(test_dir / "images" / "test_ambiguous_001.png"), image)
    print(f"  SUCCESS - Image shape: {image.shape}")
    print(f"  Intensity range: {image.min()}-{image.max()}")
except Exception as e:
    print(f"  ERROR: {e}")

print("\n" + "=" * 70)
print("TEST COMPLETE - Sample images generated")
print("Location: " + str(test_dir / "images"))
print("=" * 70)
