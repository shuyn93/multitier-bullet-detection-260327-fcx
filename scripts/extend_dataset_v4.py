# -*- coding: utf-8 -*-
"""
Dataset Extension Script - IR Bullet Hole Detection System

Extends the existing dataset_ir_realistic with ~10,000 additional samples
using the same generation pipeline, ensuring:
- No duplicate filenames (continues from last sample ID)
- Consistent feature schema
- Same label encoding
- Improved diversity through different random seeds
"""

import numpy as np
import cv2
import pandas as pd
from pathlib import Path
from scipy.ndimage import gaussian_filter
import sys

# Configuration
ADDITIONAL_SAMPLES = 10000
IMAGE_SIZE = 256
DATASET_DIR = Path("dataset_ir_realistic")
IMAGES_DIR = DATASET_DIR / "images"

BULLET_HOLE_RATIO = 0.50
NON_BULLET_RATIO = 0.30
AMBIGUOUS_RATIO = 0.20

EASY_RATIO = 0.30
MEDIUM_RATIO = 0.50
HARD_RATIO = 0.20

# ============================================================================
# Core Image Generation Functions (same as generate_ir_dataset_v4.py)
# ============================================================================

def create_smooth_background(size, intensity_range=(100, 180)):
    """Generate smooth background using Gaussian blur"""
    h, w = size
    noise = np.random.uniform(0, 1, (h // 8, w // 8))
    noise = cv2.resize(noise, (w, h), interpolation=cv2.INTER_CUBIC)
    noise = gaussian_filter(noise, sigma=5)
    
    noise_norm = (noise - noise.min()) / (noise.max() - noise.min() + 1e-6)
    bg_intensity = intensity_range[0] + noise_norm * (intensity_range[1] - intensity_range[0])
    
    return bg_intensity.astype(np.uint8)

def create_irregular_circle(size, center, radius, irregularity=0.1):
    """Create slightly irregular circular shape"""
    h, w = size
    y, x = np.ogrid[:h, :w]
    
    angles = np.arctan2(y - center[1], x - center[0])
    dist = np.sqrt((x - center[0])**2 + (y - center[1])**2)
    
    perturbation = radius * irregularity * (0.3 * np.sin(4 * angles) + 0.2 * np.sin(6 * angles))
    effective_radius = radius + perturbation
    
    mask = np.exp(-((dist - effective_radius)**2) / (2 * (0.5 * radius)**2))
    return mask

def create_radial_cracks(size, center, radius, num_cracks=4):
    """Generate radial cracks from bullet hole"""
    h, w = size
    cracks = np.zeros((h, w), dtype=np.float32)
    
    crack_length = radius * np.random.uniform(2.0, 3.5)
    
    for i in range(num_cracks):
        angle = (2 * np.pi * i / num_cracks) + np.random.uniform(-0.3, 0.3)
        
        start_x, start_y = int(center[0]), int(center[1])
        end_x = int(center[0] + crack_length * np.cos(angle))
        end_y = int(center[1] + crack_length * np.sin(angle))
        
        line_mask = np.zeros((h, w), dtype=np.float32)
        cv2.line(line_mask, (start_x, start_y), (end_x, end_y), 1, 1)
        
        crack_component = gaussian_filter(line_mask, sigma=1.5)
        crack_strength = np.random.uniform(0.3, 0.7)
        cracks += crack_strength * crack_component
    
    if cracks.max() > 0:
        cracks = cracks / cracks.max()
    
    return cracks

def create_edge_ring(size, center, radius, ring_width=3):
    """Create thermal contrast ring at boundary"""
    h, w = size
    y, x = np.ogrid[:h, :w]
    
    dist = np.sqrt((x - center[0])**2 + (y - center[1])**2)
    ring = np.exp(-((dist - radius)**2) / (2 * ring_width**2))
    
    return ring

def generate_bullet_hole_image(size, difficulty='medium'):
    """Generate realistic IR image with bullet hole"""
    h, w = size
    
    background = create_smooth_background((h, w), (100, 180))
    image = background.astype(np.float32)
    
    # Randomize based on difficulty
    if difficulty == 'easy':
        radius_range = (25, 35)
        irregularity_range = (0.05, 0.1)
        num_cracks = np.random.randint(2, 4)
        hole_intensity = np.random.uniform(15, 35)
    elif difficulty == 'medium':
        radius_range = (15, 30)
        irregularity_range = (0.08, 0.15)
        num_cracks = np.random.randint(2, 5)
        hole_intensity = np.random.uniform(15, 45)
    else:
        radius_range = (8, 20)
        irregularity_range = (0.1, 0.2)
        num_cracks = np.random.randint(1, 4)
        hole_intensity = np.random.uniform(20, 50)
    
    margin = 50
    center_x = np.random.randint(margin, w - margin)
    center_y = np.random.randint(margin, h - margin)
    center = (center_x, center_y)
    hole_radius = np.random.uniform(*radius_range)
    
    # Bullet hole
    hole_mask = create_irregular_circle((h, w), center, hole_radius, 
                                      np.random.uniform(*irregularity_range))
    
    distance_from_center = np.sqrt((np.arange(w)[None, :] - center[0])**2 + 
                                  (np.arange(h)[:, None] - center[1])**2)
    gradient = np.exp(-(distance_from_center**2) / (2 * (hole_radius**2)))
    hole_intensity_map = hole_intensity + (1 - gradient) * 30
    hole_intensity_map = np.clip(hole_intensity_map, 10, 50)
    
    image = image * (1 - hole_mask) + hole_intensity_map * hole_mask
    
    # Edge ring
    edge_ring = create_edge_ring((h, w), center, hole_radius, ring_width=4)
    ring_intensity = np.random.uniform(180, 220)
    image = image * (1 - 0.3 * edge_ring) + 0.3 * edge_ring * ring_intensity
    
    # Cracks
    cracks = create_radial_cracks((h, w), center, hole_radius, num_cracks=num_cracks)
    image = image * (1 - 0.4 * cracks) + 0.4 * cracks * 80
    
    # Noise
    gaussian_noise = np.random.normal(0, 2, (h, w))
    image = image + gaussian_noise
    
    image = np.clip(image, 0, 255).astype(np.uint8)
    
    return image, center, hole_radius

def generate_non_bullet_image(size):
    """Generate IR image without bullet hole"""
    h, w = size
    background = create_smooth_background((h, w), (100, 180))
    
    if np.random.rand() > 0.5:
        for _ in range(np.random.randint(3, 8)):
            spot_center = (np.random.randint(50, w-50), np.random.randint(50, h-50))
            spot_radius = np.random.randint(5, 15)
            spot_intensity = np.random.uniform(120, 160)
            
            y, x = np.ogrid[:h, :w]
            dist = np.sqrt((x - spot_center[0])**2 + (y - spot_center[1])**2)
            mask = np.exp(-(dist**2) / (2 * (1.5 * spot_radius)**2))
            
            background = background * (1 - 0.2 * mask) + spot_intensity * 0.2 * mask
    
    gaussian_noise = np.random.normal(0, 2, (h, w))
    background = background + gaussian_noise
    
    return np.clip(background, 0, 255).astype(np.uint8)

def generate_ambiguous_image(size):
    """Generate ambiguous images"""
    h, w = size
    background = create_smooth_background((h, w), (100, 180))
    image = background.astype(np.float32)
    
    center = (np.random.randint(50, w-50), np.random.randint(50, h-50))
    radius = np.random.uniform(5, 12)
    
    y, x = np.ogrid[:h, :w]
    dist = np.sqrt((x - center[0])**2 + (y - center[1])**2)
    mask = np.exp(-(dist**2) / (2 * (2.0 * radius)**2))
    
    hole_intensity = np.random.uniform(60, 100)
    image = image * (1 - 0.3 * mask) + hole_intensity * 0.3 * mask
    
    gaussian_noise = np.random.normal(0, 2, (h, w))
    image = image + gaussian_noise
    
    return np.clip(image, 0, 255).astype(np.uint8), center, radius

def extract_basic_features(image, center, radius):
    """Extract basic features from image"""
    h, w = image.shape
    
    y, x = np.ogrid[:h, :w]
    dist = np.sqrt((x - center[0])**2 + (y - center[1])**2)
    hole_mask = (dist <= radius * 1.2).astype(np.uint8)
    
    area = np.sum(hole_mask)
    circularity = 0.7 + np.random.uniform(-0.1, 0.1)
    solidity = 0.8 + np.random.uniform(-0.1, 0.1)
    
    hole_region = image[hole_mask > 0]
    mean_intensity = np.mean(hole_region) if len(hole_region) > 0 else 0
    std_intensity = np.std(hole_region) if len(hole_region) > 0 else 0
    
    edges = cv2.Canny(image, 50, 150)
    edge_region = edges[hole_mask > 0]
    edge_density = np.sum(edge_region) / (len(edge_region) + 1e-6) if len(edge_region) > 0 else 0
    
    aspect_ratio = 1.0 + np.random.uniform(-0.2, 0.2)
    
    return {
        'area': float(area),
        'circularity': float(np.clip(circularity, 0, 1)),
        'solidity': float(np.clip(solidity, 0, 1)),
        'mean_intensity': float(mean_intensity),
        'std_intensity': float(std_intensity),
        'edge_density': float(edge_density),
        'aspect_ratio': float(aspect_ratio),
        'center_x': float(center[0]),
        'center_y': float(center[1]),
        'radius': float(radius)
    }

# ============================================================================
# Main Dataset Extension
# ============================================================================

def extend_dataset():
    """Extend existing dataset with new samples"""
    
    print("\n" + "=" * 80)
    print("EXTENDING IR BULLET HOLE DATASET - generate_ir_dataset_v4 GENERATOR")
    print("=" * 80)
    
    # ---- Step 1: Load existing dataset ----
    print("\n[STEP 1] Loading existing dataset...")
    print("-" * 80)
    
    try:
        csv_path = DATASET_DIR / "annotations.csv"
        if not csv_path.exists():
            print(f"? ERROR: No existing dataset found at {DATASET_DIR}")
            return False
        
        existing_df = pd.read_csv(csv_path)
        num_existing = len(existing_df)
        
        # Extract the max image number from existing filenames
        existing_filenames = existing_df['filename'].tolist()
        max_existing_num = max([int(f.split('_')[-1].split('.')[0]) for f in existing_filenames])
        
        print(f"? Loaded {num_existing} existing samples")
        print(f"  Last filename: {existing_filenames[-1]}")
        print(f"  Max image ID: {max_existing_num}")
        print(f"  Dataset directory: {IMAGES_DIR}")
        
    except Exception as e:
        print(f"? ERROR loading existing dataset: {e}")
        import traceback
        traceback.print_exc()
        return False
    
    # ---- Step 2: Generate new samples ----
    print(f"\n[STEP 2] Generating {ADDITIONAL_SAMPLES} new samples...")
    print("-" * 80)
    
    try:
        # Determine label distribution
        num_bullet = int(ADDITIONAL_SAMPLES * BULLET_HOLE_RATIO)
        num_non_bullet = int(ADDITIONAL_SAMPLES * NON_BULLET_RATIO)
        num_ambiguous = ADDITIONAL_SAMPLES - num_bullet - num_non_bullet
        
        annotations = []
        total = 0
        
        # Generate bullet holes
        print(f"\n  [2a] Generating {num_bullet} bullet hole images...")
        for i in range(num_bullet):
            rand = np.random.rand()
            if rand < EASY_RATIO:
                difficulty = 'easy'
            elif rand < EASY_RATIO + MEDIUM_RATIO:
                difficulty = 'medium'
            else:
                difficulty = 'hard'
            
            image, center, radius = generate_bullet_hole_image((IMAGE_SIZE, IMAGE_SIZE), difficulty)
            
            filename = f"img_{max_existing_num + 1 + total:06d}.png"
            cv2.imwrite(str(IMAGES_DIR / filename), image)
            
            features = extract_basic_features(image, center, radius)
            features['filename'] = filename
            features['label'] = 1
            features['difficulty'] = difficulty
            annotations.append(features)
            
            if (i + 1) % 1000 == 0:
                print(f"    Generated {i + 1}/{num_bullet}")
            
            total += 1
        
        # Generate non-bullet images
        print(f"\n  [2b] Generating {num_non_bullet} non-bullet images...")
        for i in range(num_non_bullet):
            image = generate_non_bullet_image((IMAGE_SIZE, IMAGE_SIZE))
            
            filename = f"img_{max_existing_num + 1 + total:06d}.png"
            cv2.imwrite(str(IMAGES_DIR / filename), image)
            
            features = {
                'filename': filename,
                'label': 0,
                'difficulty': 'N/A',
                'area': 0,
                'circularity': 0,
                'solidity': 0,
                'mean_intensity': float(np.mean(image)),
                'std_intensity': float(np.std(image)),
                'edge_density': 0,
                'aspect_ratio': 0,
                'center_x': 0,
                'center_y': 0,
                'radius': 0
            }
            annotations.append(features)
            
            if (i + 1) % 1000 == 0:
                print(f"    Generated {i + 1}/{num_non_bullet}")
            
            total += 1
        
        # Generate ambiguous images
        print(f"\n  [2c] Generating {num_ambiguous} ambiguous images...")
        for i in range(num_ambiguous):
            image, center, radius = generate_ambiguous_image((IMAGE_SIZE, IMAGE_SIZE))
            
            filename = f"img_{max_existing_num + 1 + total:06d}.png"
            cv2.imwrite(str(IMAGES_DIR / filename), image)
            
            features = extract_basic_features(image, center, radius)
            features['filename'] = filename
            features['label'] = -1
            features['difficulty'] = 'ambiguous'
            annotations.append(features)
            
            if (i + 1) % 1000 == 0:
                print(f"    Generated {i + 1}/{num_ambiguous}")
            
            total += 1
        
        new_df = pd.DataFrame(annotations)
        print(f"\n? Generated {len(new_df)} new samples")
        
    except Exception as e:
        print(f"\n? ERROR during generation: {e}")
        import traceback
        traceback.print_exc()
        return False
    
    # ---- Step 3: Merge datasets ----
    print(f"\n[STEP 3] Merging datasets...")
    print("-" * 80)
    
    try:
        merged_df = pd.concat([existing_df, new_df], ignore_index=True)
        
        print(f"? Merged datasets:")
        print(f"  Original samples: {num_existing}")
        print(f"  New samples:      {len(new_df)}")
        print(f"  Total samples:    {len(merged_df)}")
        
    except Exception as e:
        print(f"? ERROR merging datasets: {e}")
        import traceback
        traceback.print_exc()
        return False
    
    # ---- Step 4: Validate merged dataset ----
    print(f"\n[STEP 4] Validating merged dataset...")
    print("-" * 80)
    
    try:
        # Check for missing values
        missing = merged_df.isnull().sum().sum()
        if missing > 0:
            print(f"? WARNING: Found {missing} missing values in CSV")
        else:
            print(f"? No missing values")
        
        # Check label distribution
        label_counts = merged_df['label'].value_counts()
        print(f"\n? Label distribution:")
        label_names = {1: "bullet_hole", 0: "non_bullet", -1: "ambiguous"}
        for label, count in sorted(label_counts.items()):
            pct = 100 * count / len(merged_df)
            label_name = label_names.get(label, f"unknown ({label})")
            print(f"  {label_name:15s}: {count:6d} ({pct:5.1f}%)")
        
        # Check difficulty distribution
        diff_counts = merged_df['difficulty'].value_counts()
        print(f"\n? Difficulty distribution:")
        for diff, count in sorted(diff_counts.items()):
            pct = 100 * count / len(merged_df)
            print(f"  {diff:15s}: {count:6d} ({pct:5.1f}%)")
        
        # Verify image count matches CSV
        image_files = list(IMAGES_DIR.glob("*.png"))
        print(f"\n? File count verification:")
        print(f"  CSV rows:  {len(merged_df)}")
        print(f"  PNG files: {len(image_files)}")
        
        if len(image_files) != len(merged_df):
            print(f"? WARNING: Mismatch between CSV rows and PNG files!")
        else:
            print(f"? All samples have corresponding images")
        
        # Validate feature ranges (new samples)
        print(f"\n? Feature statistics (new samples only):")
        bullet_new = new_df[new_df['label'] == 1]
        if len(bullet_new) > 0:
            print(f"  Bullet hole samples: {len(bullet_new)}")
            print(f"    - Area:          {bullet_new['area'].min():.1f} - {bullet_new['area'].max():.1f} (mean={bullet_new['area'].mean():.1f})")
            print(f"    - Circularity:   {bullet_new['circularity'].min():.3f} - {bullet_new['circularity'].max():.3f} (mean={bullet_new['circularity'].mean():.3f})")
            print(f"    - Mean Intensity: {bullet_new['mean_intensity'].min():.1f} - {bullet_new['mean_intensity'].max():.1f} (mean={bullet_new['mean_intensity'].mean():.1f})")
        
    except Exception as e:
        print(f"\n? ERROR during validation: {e}")
        import traceback
        traceback.print_exc()
        return False
    
    # ---- Step 5: Save merged dataset ----
    print(f"\n[STEP 5] Saving merged dataset...")
    print("-" * 80)
    
    try:
        csv_path = DATASET_DIR / "annotations.csv"
        merged_df.to_csv(csv_path, index=False)
        print(f"? Saved merged annotations to: {csv_path}")
        print(f"  Total rows: {len(merged_df)}")
        
    except Exception as e:
        print(f"? ERROR saving dataset: {e}")
        import traceback
        traceback.print_exc()
        return False
    
    # ---- Step 6: Display summary ----
    print("\n" + "=" * 80)
    print("DATASET EXTENSION COMPLETE")
    print("=" * 80)
    
    print(f"\n? SUMMARY:")
    print(f"  Samples added:     {len(new_df):,}")
    print(f"  Total dataset:     {len(merged_df):,}")
    print(f"  Output directory:  {DATASET_DIR.absolute()}")
    print(f"  Annotations file:  {csv_path.absolute()}")
    
    print(f"\n? STATISTICS:")
    bullet_total = (merged_df['label'] == 1).sum()
    non_bullet_total = (merged_df['label'] == 0).sum()
    ambiguous_total = (merged_df['label'] == -1).sum()
    
    print(f"  Total bullet holes:  {bullet_total:,} ({100*bullet_total/len(merged_df):.1f}%)")
    print(f"  Total non-bullets:   {non_bullet_total:,} ({100*non_bullet_total/len(merged_df):.1f}%)")
    print(f"  Total ambiguous:     {ambiguous_total:,} ({100*ambiguous_total/len(merged_df):.1f}%)")
    
    return True


if __name__ == '__main__':
    success = extend_dataset()
    sys.exit(0 if success else 1)
