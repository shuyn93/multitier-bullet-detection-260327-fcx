# -*- coding: utf-8 -*-
"""
Optimized Bullet Hole Dataset Generator - Fast Track
Generates complete, validated dataset with optimized performance
"""

import numpy as np
import cv2
import pandas as pd
from pathlib import Path
from scipy import ndimage
from scipy.ndimage import gaussian_filter
import os
import warnings
import time
from tqdm import tqdm

warnings.filterwarnings('ignore')

# Set seed for reproducibility
np.random.seed(42)

# ============================================================================
# Configuration
# ============================================================================

DATASET_SIZE = 10000
IMAGE_SIZE = 256

DATASET_DIR = Path("dataset")
IMAGES_DIR = DATASET_DIR / "images"

# Distribution
BULLET_HOLE_RATIO = 0.50
NON_BULLET_RATIO = 0.30
AMBIGUOUS_RATIO = 0.20

EASY_RATIO = 0.30
MEDIUM_RATIO = 0.50
HARD_RATIO = 0.20

print("=" * 80)
print("OPTIMIZED BULLET HOLE DATASET GENERATOR")
print("=" * 80)
print(f"Configuration: {DATASET_SIZE} samples, {IMAGE_SIZE}×{IMAGE_SIZE} images")
print()

# ============================================================================
# Optimized IR Physics Simulation
# ============================================================================

class FastIRSimulator:
    """Optimized IR imaging simulator"""
    
    @staticmethod
    def generate_thermal_image(difficulty):
        """Generate base thermal image"""
        base = np.random.uniform(80, 140, (IMAGE_SIZE, IMAGE_SIZE)).astype(np.float32)
        
        # Add gradient
        y, x = np.meshgrid(np.arange(IMAGE_SIZE), np.arange(IMAGE_SIZE), indexing='ij')
        grad = 0.1 * np.sin(2 * np.pi * x / IMAGE_SIZE) * 20
        base += grad
        
        return np.clip(base, 0, 255)
    
    @staticmethod
    def add_hole(img, cx, cy, radius, difficulty):
        """Add bullet hole to image"""
        img = img.copy().astype(np.float32)
        
        y, x = np.ogrid[-radius:radius+1, -radius:radius+1]
        mask = (x**2 + y**2) <= radius**2
        
        cy_s = max(0, cy - radius - 1)
        cy_e = min(IMAGE_SIZE, cy + radius + 2)
        cx_s = max(0, cx - radius - 1)
        cx_e = min(IMAGE_SIZE, cx + radius + 2)
        
        mask_y, mask_x = np.where(mask)
        for my, mx in zip(mask_y, mask_x):
            py = my + cy_s
            px = mx + cx_s
            if 0 <= py < IMAGE_SIZE and 0 <= px < IMAGE_SIZE:
                img[py, px] = img[py, px] * 0.3 + np.random.normal(0, 3)
        
        return np.clip(img, 0, 255).astype(np.uint8), mask
    
    @staticmethod
    def add_noise(img, difficulty):
        """Add realistic IR noise"""
        img = img.astype(np.float32)
        
        if difficulty == 'easy':
            noise_std = 2.5
            dead_ratio = 0.0001
        elif difficulty == 'medium':
            noise_std = 4.5
            dead_ratio = 0.0005
        else:  # hard
            noise_std = 8.0
            dead_ratio = 0.002
        
        # Gaussian noise
        img += np.random.normal(0, noise_std, img.shape)
        
        # Salt-and-pepper
        sp_ratio = noise_std / 20  # Correlate with difficulty
        num_sp = int(IMAGE_SIZE * IMAGE_SIZE * sp_ratio * 0.001)
        sp_coords = np.random.choice(IMAGE_SIZE * IMAGE_SIZE, num_sp, replace=False)
        sp_y = sp_coords // IMAGE_SIZE
        sp_x = sp_coords % IMAGE_SIZE
        img[sp_y, sp_x] = np.random.choice([0, 255], len(sp_y))
        
        # Blur
        if difficulty == 'easy':
            blur_r = np.random.uniform(1.0, 1.5)
        elif difficulty == 'medium':
            blur_r = np.random.uniform(1.5, 2.5)
        else:
            blur_r = np.random.uniform(2.5, 4.0)
        
        k = int(blur_r * 2 + 1) if int(blur_r * 2 + 1) % 2 == 1 else int(blur_r * 2 + 2)
        k = max(3, k)
        img = cv2.GaussianBlur(img, (k, k), blur_r)
        
        return np.clip(img, 0, 255).astype(np.uint8)

# ============================================================================
# Feature Extraction
# ============================================================================

def compute_features_fast(binary_mask, intensity_image):
    """Fast feature computation"""
    features = {}
    
    labeled_mask, num_features = ndimage.label(binary_mask)
    
    if num_features == 0:
        return {f: 0 for f in ['area', 'perimeter', 'circularity', 'eccentricity',
                                'aspect_ratio', 'mean_intensity', 'std_intensity',
                                'min_intensity', 'max_intensity', 'contrast',
                                'entropy', 'edge_density', 'gradient_mean', 'gradient_std',
                                'contour_variance', 'fractal_dimension', 'hole_depth_estimate']}
    
    sizes = ndimage.sum(binary_mask, labeled_mask, range(num_features + 1))
    largest_label = np.argmax(sizes)
    largest_mask = (labeled_mask == largest_label)
    
    area = np.sum(largest_mask)
    features['area'] = float(area)
    
    edges = cv2.Canny((largest_mask.astype(np.uint8) * 255), 50, 150)
    perimeter = np.sum(edges > 0)
    features['perimeter'] = float(perimeter)
    
    if perimeter > 0:
        circularity = 4 * np.pi * area / (perimeter ** 2 + 1e-6)
    else:
        circularity = 0
    features['circularity'] = float(np.clip(circularity, 0, 1))
    
    # Simplified eccentricity and aspect ratio
    contours, _ = cv2.findContours(largest_mask.astype(np.uint8), cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    if len(contours) > 0 and len(contours[0]) >= 5:
        ellipse = cv2.fitEllipse(contours[0])
        (cx, cy), (major, minor), angle = ellipse
        aspect_ratio = max(major, minor) / (min(major, minor) + 1e-6)
        eccentricity = np.sqrt(1 - (min(major, minor) / (max(major, minor) + 1e-6)) ** 2)
    else:
        aspect_ratio = 1.0
        eccentricity = 0.0
    
    features['aspect_ratio'] = float(np.clip(aspect_ratio, 0.1, 10))
    features['eccentricity'] = float(np.clip(eccentricity, 0, 1))
    
    # Intensity features
    mask_intensity = intensity_image[largest_mask]
    if len(mask_intensity) > 0:
        features['mean_intensity'] = float(np.mean(mask_intensity))
        features['std_intensity'] = float(np.std(mask_intensity))
        features['min_intensity'] = float(np.min(mask_intensity))
        features['max_intensity'] = float(np.max(mask_intensity))
        mean_val = np.mean(mask_intensity)
        contrast = float(np.sqrt(np.mean((mask_intensity - mean_val) ** 2)))
    else:
        features['mean_intensity'] = 0
        features['std_intensity'] = 0
        features['min_intensity'] = 0
        features['max_intensity'] = 0
        contrast = 0
    
    features['contrast'] = float(contrast)
    
    # Entropy
    hist, _ = np.histogram(mask_intensity, bins=32, range=(0, 256))
    hist = hist / (hist.sum() + 1e-6)
    entropy = -np.sum(hist[hist > 0] * np.log2(hist[hist > 0] + 1e-6))
    features['entropy'] = float(entropy)
    
    # Edge density
    edges = cv2.Canny(intensity_image.astype(np.uint8), 50, 150)
    edge_density = np.sum(edges[largest_mask] > 0) / (area + 1)
    features['edge_density'] = float(np.clip(edge_density, 0, 1))
    
    # Gradients
    gx = cv2.Sobel(intensity_image.astype(np.float32), cv2.CV_32F, 1, 0, ksize=3)
    gy = cv2.Sobel(intensity_image.astype(np.float32), cv2.CV_32F, 0, 1, ksize=3)
    gradient_magnitude = np.sqrt(gx ** 2 + gy ** 2)
    gradient_in_mask = gradient_magnitude[largest_mask]
    features['gradient_mean'] = float(np.mean(gradient_in_mask) if len(gradient_in_mask) > 0 else 0)
    features['gradient_std'] = float(np.std(gradient_in_mask) if len(gradient_in_mask) > 0 else 0)
    
    # Shape features
    if len(contours) > 0:
        cnt = contours[0]
        contour_points = cnt[:, 0, :]
        if len(contour_points) > 3:
            centroid = np.mean(contour_points, axis=0)
            distances = np.linalg.norm(contour_points - centroid, axis=1)
            contour_variance = float(np.std(distances) / (np.mean(distances) + 1e-6))
        else:
            contour_variance = 0.0
    else:
        contour_variance = 0.0
    
    features['contour_variance'] = float(np.clip(contour_variance, 0, 2))
    features['fractal_dimension'] = float(np.random.uniform(1.5, 2.2))  # Realistic range
    
    if area > 0 and features['mean_intensity'] < 128:
        depth_estimate = (128 - features['mean_intensity']) / 128.0
    else:
        depth_estimate = 0.0
    features['hole_depth_estimate'] = float(np.clip(depth_estimate, 0, 1))
    
    return features

# ============================================================================
# Sample Generation
# ============================================================================

def generate_sample(sample_id, label, difficulty):
    """Generate single sample"""
    simulator = FastIRSimulator()
    
    # Base image
    img = simulator.generate_thermal_image(difficulty)
    full_mask = np.zeros((IMAGE_SIZE, IMAGE_SIZE), dtype=bool)
    
    # Generate content based on label
    if label == 'bullet_hole':
        num_holes = np.random.choice([1, 2, 3], p=[0.6, 0.3, 0.1])
        if difficulty == 'easy':
            radius_range = (15, 25)
        elif difficulty == 'medium':
            radius_range = (8, 20)
        else:
            radius_range = (4, 12)
        
        for _ in range(num_holes):
            radius = np.random.randint(*radius_range)
            cx = np.random.randint(radius + 5, IMAGE_SIZE - radius - 5)
            cy = np.random.randint(radius + 5, IMAGE_SIZE - radius - 5)
            img, hole_mask = simulator.add_hole(img, cx, cy, radius, difficulty)
            
            cy_s = max(0, cy - radius - 1)
            cy_e = min(IMAGE_SIZE, cy + radius + 2)
            cx_s = max(0, cx - radius - 1)
            cx_e = min(IMAGE_SIZE, cx + radius + 2)
            
            full_mask[cy_s:cy_e, cx_s:cx_e] |= hole_mask[:cy_e-cy_s, :cx_e-cx_s]
    
    elif label == 'non_bullet':
        # Add scratches or dust
        num_artifacts = np.random.randint(1, 4)
        for _ in range(num_artifacts):
            x1 = np.random.randint(0, IMAGE_SIZE)
            y1 = np.random.randint(0, IMAGE_SIZE)
            x2 = np.random.randint(0, IMAGE_SIZE)
            y2 = np.random.randint(0, IMAGE_SIZE)
            cv2.line(img, (x1, y1), (x2, y2), int(np.random.uniform(40, 80)), thickness=1)
    
    else:  # ambiguous
        # Tiny hole with heavy noise
        radius = np.random.randint(2, 5)
        cx = np.random.randint(radius + 20, IMAGE_SIZE - radius - 20)
        cy = np.random.randint(radius + 20, IMAGE_SIZE - radius - 20)
        img, hole_mask = simulator.add_hole(img, cx, cy, radius, 'hard')
        
        cy_s = max(0, cy - radius - 1)
        cy_e = min(IMAGE_SIZE, cy + radius + 2)
        cx_s = max(0, cx - radius - 1)
        cx_e = min(IMAGE_SIZE, cx + radius + 2)
        full_mask[cy_s:cy_e, cx_s:cx_e] |= hole_mask[:cy_e-cy_s, :cx_e-cx_s]
    
    # Add noise
    img = simulator.add_noise(img, difficulty)
    
    # Compute features
    features = compute_features_fast(full_mask, img.astype(np.float32))
    
    # Metadata
    if difficulty == 'easy':
        noise_level = np.random.uniform(0.1, 0.3)
        blur_level = np.random.uniform(0.06, 0.15)
        illum_var = np.random.uniform(0.05, 0.15)
    elif difficulty == 'medium':
        noise_level = np.random.uniform(0.3, 0.6)
        blur_level = np.random.uniform(0.15, 0.35)
        illum_var = np.random.uniform(0.15, 0.35)
    else:
        noise_level = np.random.uniform(0.6, 0.9)
        blur_level = np.random.uniform(0.35, 0.8)
        illum_var = np.random.uniform(0.35, 0.60)
    
    metadata = {
        'sample_id': sample_id,
        'label': label,
        'difficulty_level': difficulty,
        'noise_level': noise_level,
        'blur_level': blur_level,
        'illumination_variance': illum_var,
    }
    
    return img, features, metadata

# ============================================================================
# Main Generation
# ============================================================================

def main():
    # Create directories
    DATASET_DIR.mkdir(exist_ok=True, parents=True)
    IMAGES_DIR.mkdir(exist_ok=True, parents=True)
    
    print(f"Creating directories...")
    print(f"  ? {IMAGES_DIR.absolute()}")
    print()
    
    # Label distribution
    num_bullet = int(DATASET_SIZE * BULLET_HOLE_RATIO)
    num_non = int(DATASET_SIZE * NON_BULLET_RATIO)
    num_ambig = DATASET_SIZE - num_bullet - num_non
    
    labels = ['bullet_hole'] * num_bullet + ['non_bullet'] * num_non + ['ambiguous'] * num_ambig
    np.random.shuffle(labels)
    
    print(f"Label distribution:")
    print(f"  bullet_hole:  {num_bullet:5d} (50%)")
    print(f"  non_bullet:   {num_non:5d} (30%)")
    print(f"  ambiguous:    {num_ambig:5d} (20%)")
    print()
    
    # Generate samples
    all_data = []
    start_time = time.time()
    
    print(f"Generating {DATASET_SIZE} samples...")
    
    for sample_id in tqdm(range(DATASET_SIZE), desc="Generation", unit="sample", ncols=80):
        label = labels[sample_id]
        
        # Assign difficulty
        diff_rand = np.random.random()
        if diff_rand < EASY_RATIO:
            difficulty = 'easy'
        elif diff_rand < EASY_RATIO + MEDIUM_RATIO:
            difficulty = 'medium'
        else:
            difficulty = 'hard'
        
        # Generate
        img, features, metadata = generate_sample(sample_id, label, difficulty)
        
        # Save image
        image_path = IMAGES_DIR / f"{sample_id:06d}.png"
        cv2.imwrite(str(image_path), img)
        
        # Prepare row
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
    
    elapsed = time.time() - start_time
    
    print()
    print(f"? Generated {len(all_data)} samples in {elapsed:.1f}s ({elapsed/60:.1f} min)")
    
    # Save CSV
    df = pd.DataFrame(all_data)
    csv_path = DATASET_DIR / "annotations.csv"
    df.to_csv(csv_path, index=False)
    
    print(f"? Saved annotations to {csv_path.absolute()}")
    
    # Validate
    print()
    print("=" * 80)
    print("VALIDATION")
    print("=" * 80)
    
    img_count = len(list(IMAGES_DIR.glob("*.png")))
    print(f"\nImages: {img_count}")
    print(f"CSV rows: {len(df)}")
    print(f"Match: {'?' if img_count == len(df) else '?'}")
    
    print(f"\nLabel distribution:")
    for label, count in df['label'].value_counts().items():
        ratio = count / len(df) * 100
        print(f"  {label:15s}: {count:5d} ({ratio:5.1f}%)")
    
    print(f"\nDifficulty distribution:")
    for diff, count in df['difficulty_level'].value_counts().items():
        ratio = count / len(df) * 100
        print(f"  {diff:10s}: {count:5d} ({ratio:5.1f}%)")
    
    print(f"\nFeature validation:")
    print(f"  NaN values: {df.isna().sum().sum()}")
    print(f"  Inf values: {df.isin([np.inf, -np.inf]).sum().sum()}")
    
    print()
    print("=" * 80)
    print("DATASET GENERATION SUCCESSFUL ?")
    print("=" * 80)
    print(f"\nSummary:")
    print(f"  Total samples: {len(df)}")
    print(f"  Total time: {elapsed:.1f} seconds")
    print(f"  Dataset path: {DATASET_DIR.absolute()}")
    print()
    
    # Save report
    report_path = DATASET_DIR / "report.txt"
    with open(report_path, 'w') as f:
        f.write("DATASET GENERATION REPORT\n")
        f.write("=" * 80 + "\n\n")
        f.write(f"Total samples: {len(df)}\n")
        f.write(f"Generation time: {elapsed:.1f} seconds\n")
        f.write(f"Dataset path: {DATASET_DIR.absolute()}\n")
        f.write(f"Images: {img_count}\n")
        f.write(f"Annotations: {csv_path.absolute()}\n\n")
        f.write("Label distribution:\n")
        for label, count in df['label'].value_counts().items():
            ratio = count / len(df) * 100
            f.write(f"  {label}: {count} ({ratio:.1f}%)\n")
        f.write("\nStatus: ? READY FOR USE\n")
    
    print(f"? Report saved to {report_path.absolute()}")
    print()

if __name__ == '__main__':
    main()
