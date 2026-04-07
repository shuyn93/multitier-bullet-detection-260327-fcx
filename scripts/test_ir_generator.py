#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Quick test run for IR dataset generator - small sample"""

import sys
import os
sys.path.insert(0, os.getcwd())

# Set to generate only 100 images for testing
import scripts.generate_ir_realistic_v3 as gen

gen.DATASET_SIZE = 100
gen.DATASET_DIR = gen.Path("dataset_ir_test")
gen.IMAGES_DIR = gen.DATASET_DIR / "images"

if __name__ == "__main__":
    print("Starting test run with 100 images...")
    df = gen.generate_dataset(100)
    
    print("\nTest dataset statistics:")
    print(df.groupby('label').size())
    
    print("\nDisplaying 5 sample images...")
    gen.display_samples(5)
    
    print("\nTest complete! Check dataset_ir_test/samples_visualization/")
