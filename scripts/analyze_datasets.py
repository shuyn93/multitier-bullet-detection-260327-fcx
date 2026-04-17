#!/usr/bin/env python3
"""
CSV Dataset Analysis Tool
Analyzes all annotation CSV files in the datasets folder
"""

import pandas as pd
import os
from pathlib import Path

# Define dataset paths
DATASETS = {
    'dataset_main': 'data/datasets/dataset_main/annotations.csv',
    'dataset_realistic': 'data/datasets/dataset_realistic/annotations.csv',
    'dataset_ir_realistic': 'data/datasets/dataset_ir_realistic/annotations.csv',
}

def analyze_csv(filepath, dataset_name):
    """Analyze a single CSV file"""
    print(f"\n{'='*70}")
    print(f"?? ANALYZING: {dataset_name}")
    print(f"{'='*70}")
    print(f"?? Path: {filepath}\n")
    
    if not os.path.exists(filepath):
        print(f"? File not found: {filepath}")
        return
    
    # Read CSV
    try:
        df = pd.read_csv(filepath)
    except Exception as e:
        print(f"? Error reading CSV: {e}")
        return
    
    # Basic statistics
    print(f"?? BASIC STATISTICS")
    print(f"  Total records: {len(df)}")
    print(f"  Total columns: {len(df.columns)}")
    print(f"  Columns: {list(df.columns)}\n")
    
    # Label distribution
    if 'label' in df.columns:
        print(f"??? LABEL DISTRIBUTION")
        label_dist = df['label'].value_counts()
        for label, count in label_dist.items():
            percentage = (count / len(df)) * 100
            print(f"  {str(label):20s}: {int(count):6d} ({percentage:5.1f}%)")
    
    # Difficulty distribution
    if 'difficulty_level' in df.columns:
        print(f"\n?? DIFFICULTY DISTRIBUTION")
        difficulty_dist = df['difficulty_level'].value_counts()
        for diff, count in difficulty_dist.items():
            percentage = (count / len(df)) * 100
            print(f"  {str(diff):15s}: {int(count):6d} ({percentage:5.1f}%)")
    elif 'difficulty' in df.columns:
        print(f"\n?? DIFFICULTY DISTRIBUTION")
        difficulty_dist = df['difficulty'].value_counts()
        for diff, count in difficulty_dist.items():
            percentage = (count / len(df)) * 100
            print(f"  {str(diff):15s}: {int(count):6d} ({percentage:5.1f}%)")
    
    # Feature statistics
    print(f"\n?? FEATURE STATISTICS (Numerical Columns)")
    numeric_cols = df.select_dtypes(include=['float64', 'int64']).columns
    
    for col in numeric_cols[:10]:  # Show first 10 features
        if df[col].dtype in ['float64', 'int64']:
            mean_val = df[col].mean()
            std_val = df[col].std()
            min_val = df[col].min()
            max_val = df[col].max()
            print(f"  {col:25s}: ?={mean_val:10.3f}, ?={std_val:10.3f}, [{min_val:10.3f}, {max_val:10.3f}]")
    
    # Check for missing values
    print(f"\n?? MISSING VALUES")
    missing = df.isnull().sum()
    if missing.sum() == 0:
        print("  ? No missing values detected")
    else:
        for col, count in missing[missing > 0].items():
            print(f"  {col}: {count} missing")
    
    # Noise and blur levels
    if 'noise_level' in df.columns and 'blur_level' in df.columns:
        print(f"\n?? NOISE & BLUR STATISTICS")
        print(f"  Noise level - Mean: {df['noise_level'].mean():.3f}, Range: [{df['noise_level'].min():.3f}, {df['noise_level'].max():.3f}]")
        print(f"  Blur level  - Mean: {df['blur_level'].mean():.3f}, Range: [{df['blur_level'].min():.3f}, {df['blur_level'].max():.3f}]")

def main():
    print("\n" + "="*70)
    print("?? BULLET HOLE DETECTION - DATASET CSV ANALYSIS")
    print("="*70)
    
    for dataset_name, filepath in DATASETS.items():
        analyze_csv(filepath, dataset_name)
    
    # Summary comparison
    print(f"\n{'='*70}")
    print("?? SUMMARY COMPARISON")
    print(f"{'='*70}\n")
    
    total_images = 0
    for dataset_name, filepath in DATASETS.items():
        if os.path.exists(filepath):
            df = pd.read_csv(filepath)
            total_images += len(df)
            print(f"  {dataset_name:30s}: {len(df):6d} images")
    
    print(f"  {'?'*30}\n  {'TOTAL':30s}: {total_images:6d} images\n")

if __name__ == '__main__':
    main()
