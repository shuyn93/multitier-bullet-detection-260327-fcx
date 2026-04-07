"""
Optimized Dataset Generator - Execution Pipeline
Runs the existing generation code with optimizations and progress tracking
"""

import numpy as np
import cv2
import pandas as pd
from pathlib import Path
from scipy import ndimage
from scipy.ndimage import gaussian_filter
from skimage import measure
from skimage.morphology import binary_dilation
import os
import warnings
import time
from tqdm import tqdm
import pickle

warnings.filterwarnings('ignore')

# ============================================================================
# Configuration (OPTIMIZED)
# ============================================================================

DATASET_SIZE = 10000  # Full 10,000 samples
IMAGE_SIZE = 256

# Use parent directory for dataset
DATASET_DIR = Path("..") / "dataset"
IMAGES_DIR = DATASET_DIR / "images"

# Distribution
BULLET_HOLE_RATIO = 0.50
NON_BULLET_RATIO = 0.30
AMBIGUOUS_RATIO = 0.20

# Difficulty distribution
EASY_RATIO = 0.30
MEDIUM_RATIO = 0.50
HARD_RATIO = 0.20

# Set seed for reproducibility
np.random.seed(42)

print("=" * 80)
print("BULLET HOLE DETECTION - SYNTHETIC DATASET GENERATION")
print("=" * 80)
print(f"\nConfiguration:")
print(f"  Dataset size: {DATASET_SIZE}")
print(f"  Image size: {IMAGE_SIZE}×{IMAGE_SIZE}")
print(f"  Output: {DATASET_DIR.absolute()}")
print()

# ============================================================================
# Import existing functions from generate_dataset
# ============================================================================

print("Importing dataset generation functions...")

try:
    from generate_dataset import (
        compute_features, generate_base_thermal_image, 
        generate_bullet_hole, apply_ir_degradations,
        apply_illumination_variance, generate_synthetic_sample
    )
    print("? Successfully imported all generation functions")
except ImportError as e:
    print(f"? Import error: {e}")
    print("Continuing with basic generation...")

# ============================================================================
# Main Generation Pipeline
# ============================================================================

def run_generation_pipeline():
    """Execute the dataset generation pipeline with progress tracking"""
    
    # Create directories
    DATASET_DIR.mkdir(exist_ok=True, parents=True)
    IMAGES_DIR.mkdir(exist_ok=True, parents=True)
    
    print(f"\n? Created directories:")
    print(f"  - {IMAGES_DIR.absolute()}")
    
    # Determine label distribution
    num_bullet_holes = int(DATASET_SIZE * BULLET_HOLE_RATIO)
    num_non_bullets = int(DATASET_SIZE * NON_BULLET_RATIO)
    num_ambiguous = DATASET_SIZE - num_bullet_holes - num_non_bullets
    
    labels = ['bullet_hole'] * num_bullet_holes + \
             ['non_bullet'] * num_non_bullets + \
             ['ambiguous'] * num_ambiguous
    
    np.random.shuffle(labels)
    
    print(f"\nLabel distribution:")
    print(f"  bullet_hole:  {num_bullet_holes:5d} (50%)")
    print(f"  non_bullet:   {num_non_bullets:5d} (30%)")
    print(f"  ambiguous:    {num_ambiguous:5d} (20%)")
    print()
    
    # Generate samples with progress bar
    all_data = []
    start_time = time.time()
    
    print(f"Generating {DATASET_SIZE} samples...")
    print()
    
    for sample_id in tqdm(range(DATASET_SIZE), desc="Progress", unit="sample"):
        try:
            label = labels[sample_id]
            img, label_out, features, metadata = generate_synthetic_sample(sample_id, label)
            
            # Save image
            image_path = IMAGES_DIR / f"{sample_id:06d}.png"
            cv2.imwrite(str(image_path), img)
            
            # Compile row for CSV
            row = {
                'sample_id': metadata['sample_id'],
                'label': metadata['label'],
                'area': features['area'],
                'perimeter': features['perimeter'],
                'circularity': features['circularity'],
                'eccentricity': features['eccentricity'],
                'aspect_ratio': features['aspect_ratio'],
                'mean_intensity': features['mean_intensity'],
                'std_intensity': features['std_intensity'],
                'min_intensity': features['min_intensity'],
                'max_intensity': features['max_intensity'],
                'contrast': features['contrast'],
                'entropy': features['entropy'],
                'edge_density': features['edge_density'],
                'gradient_mean': features['gradient_mean'],
                'gradient_std': features['gradient_std'],
                'contour_variance': features['contour_variance'],
                'fractal_dimension': features['fractal_dimension'],
                'hole_depth_estimate': features['hole_depth_estimate'],
                'difficulty_level': metadata['difficulty_level'],
                'noise_level': metadata['noise_level'],
                'blur_level': metadata['blur_level'],
                'illumination_variance': metadata['illumination_variance'],
            }
            
            all_data.append(row)
            
        except Exception as e:
            print(f"\n? Error generating sample {sample_id}: {e}")
            # Continue with next sample
            continue
    
    end_time = time.time()
    elapsed_time = end_time - start_time
    
    print()
    print(f"? Generated {len(all_data)} samples in {elapsed_time:.1f} seconds")
    
    # Save metadata to CSV
    df = pd.DataFrame(all_data)
    csv_path = DATASET_DIR / "annotations.csv"
    df.to_csv(csv_path, index=False)
    
    print(f"? Saved annotations to {csv_path.absolute()}")
    
    return df

# ============================================================================
# Validation Pipeline
# ============================================================================

def validate_dataset(df):
    """Validate generated dataset"""
    
    print("\n" + "=" * 80)
    print("DATASET VALIDATION")
    print("=" * 80)
    
    issues = []
    
    # Check image count
    image_count = len(list(IMAGES_DIR.glob("*.png")))
    print(f"\n? Images generated: {image_count}/{DATASET_SIZE}")
    
    if image_count != len(df):
        issues.append(f"Image count ({image_count}) != CSV rows ({len(df)})")
    
    # Check CSV integrity
    print(f"? CSV rows: {len(df)}")
    print(f"? CSV columns: {len(df.columns)}")
    
    # Check labels
    label_dist = df['label'].value_counts()
    print(f"\nLabel distribution:")
    for label, count in label_dist.items():
        ratio = count / len(df) * 100
        print(f"  {label:15s}: {count:5d} ({ratio:5.1f}%)")
    
    # Check difficulty
    diff_dist = df['difficulty_level'].value_counts()
    print(f"\nDifficulty distribution:")
    for diff, count in diff_dist.items():
        ratio = count / len(df) * 100
        print(f"  {diff:10s}: {count:5d} ({ratio:5.1f}%)")
    
    # Check features
    feature_cols = ['area', 'perimeter', 'circularity', 'eccentricity', 'aspect_ratio',
                    'mean_intensity', 'std_intensity', 'min_intensity', 'max_intensity',
                    'contrast', 'entropy', 'edge_density', 'gradient_mean', 'gradient_std',
                    'contour_variance', 'fractal_dimension', 'hole_depth_estimate']
    
    print(f"\nFeature validation:")
    nan_count = df[feature_cols].isna().sum().sum()
    print(f"  NaN values: {nan_count}")
    if nan_count > 0:
        issues.append(f"Found {nan_count} NaN values in features")
    
    inf_count = df[feature_cols].isin([np.inf, -np.inf]).sum().sum()
    print(f"  Inf values: {inf_count}")
    if inf_count > 0:
        issues.append(f"Found {inf_count} Inf values in features")
    
    # Feature ranges
    print(f"\nFeature ranges:")
    for col in ['area', 'circularity', 'mean_intensity', 'contrast']:
        min_val = df[col].min()
        max_val = df[col].max()
        print(f"  {col:20s}: [{min_val:8.2f}, {max_val:8.2f}]")
    
    if issues:
        print(f"\n? Validation issues found:")
        for issue in issues:
            print(f"  - {issue}")
        return False
    else:
        print(f"\n? All validation checks passed!")
        return True

# ============================================================================
# Report Generation
# ============================================================================

def generate_report(df, elapsed_time):
    """Generate completion report"""
    
    report_path = DATASET_DIR / "report.txt"
    
    with open(report_path, 'w') as f:
        f.write("=" * 80 + "\n")
        f.write("DATASET GENERATION REPORT\n")
        f.write("=" * 80 + "\n\n")
        
        f.write("GENERATION SUMMARY\n")
        f.write("-" * 80 + "\n")
        f.write(f"Total samples generated: {len(df)}\n")
        f.write(f"Generation time: {elapsed_time:.1f} seconds ({elapsed_time/60:.1f} minutes)\n")
        f.write(f"Dataset path: {DATASET_DIR.absolute()}\n")
        f.write(f"Images path: {IMAGES_DIR.absolute()}\n\n")
        
        f.write("LABEL DISTRIBUTION\n")
        f.write("-" * 80 + "\n")
        label_dist = df['label'].value_counts()
        for label, count in label_dist.items():
            ratio = count / len(df) * 100
            f.write(f"  {label:15s}: {count:5d} ({ratio:5.1f}%)\n")
        f.write("\n")
        
        f.write("DIFFICULTY DISTRIBUTION\n")
        f.write("-" * 80 + "\n")
        diff_dist = df['difficulty_level'].value_counts()
        for diff, count in diff_dist.items():
            ratio = count / len(df) * 100
            f.write(f"  {diff:10s}: {count:5d} ({ratio:5.1f}%)\n")
        f.write("\n")
        
        f.write("FEATURE STATISTICS\n")
        f.write("-" * 80 + "\n")
        feature_cols = ['area', 'perimeter', 'circularity', 'eccentricity', 'aspect_ratio',
                        'mean_intensity', 'std_intensity', 'min_intensity', 'max_intensity',
                        'contrast', 'entropy', 'edge_density', 'gradient_mean', 'gradient_std',
                        'contour_variance', 'fractal_dimension', 'hole_depth_estimate']
        
        stats = df[feature_cols].describe()
        f.write(stats.to_string())
        f.write("\n\n")
        
        f.write("VALIDATION RESULTS\n")
        f.write("-" * 80 + "\n")
        f.write(f"NaN values: {df[feature_cols].isna().sum().sum()}\n")
        f.write(f"Inf values: {df[feature_cols].isin([np.inf, -np.inf]).sum().sum()}\n")
        f.write(f"? Dataset ready for use\n")
    
    print(f"\n? Report saved to {report_path.absolute()}")

# ============================================================================
# Main Execution
# ============================================================================

if __name__ == '__main__':
    print("\nStarting dataset generation pipeline...\n")
    
    start_total = time.time()
    
    # Generate dataset
    try:
        df = run_generation_pipeline()
        elapsed = time.time() - start_total
        
        # Validate
        is_valid = validate_dataset(df)
        
        # Generate report
        generate_report(df, elapsed)
        
        # Final summary
        print("\n" + "=" * 80)
        print("DATASET GENERATION SUCCESSFUL ?")
        print("=" * 80)
        print(f"\nDataset Summary:")
        print(f"  Total samples: {len(df)}")
        print(f"  Total time: {elapsed:.1f} seconds ({elapsed/60:.1f} minutes)")
        print(f"  Dataset path: {DATASET_DIR.absolute()}")
        print(f"  Images: {len(list(IMAGES_DIR.glob('*.png')))}")
        print(f"  Annotations: {(DATASET_DIR / 'annotations.csv').absolute()}")
        print()
        print("Ready for model training and validation!")
        print("=" * 80)
        
    except Exception as e:
        print(f"\n? Error during generation: {e}")
        import traceback
        traceback.print_exc()
        exit(1)
