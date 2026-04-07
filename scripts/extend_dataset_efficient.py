"""
Efficient Dataset Extension Script - Generates new samples in batches
with progress tracking and recovery capability
"""

import numpy as np
import cv2
import pandas as pd
from pathlib import Path
import sys
import json
from datetime import datetime

# Set random seed for reproducibility within session
np.random.seed(42)

# Configuration
BATCH_SIZE = 500  # Process in smaller batches for progress reporting
TOTAL_NEW_SAMPLES = 10000
IMAGE_SIZE = 256
DATASET_DIR = Path("dataset")
IMAGES_DIR = DATASET_DIR / "images"

# Distribution (same as original)
BULLET_HOLE_RATIO = 0.50
NON_BULLET_RATIO = 0.30
AMBIGUOUS_RATIO = 0.20

# Difficulty distribution
EASY_RATIO = 0.30
MEDIUM_RATIO = 0.50
HARD_RATIO = 0.20

# Track progress
PROGRESS_FILE = Path("extension_progress.json")

def load_progress():
    """Load previous progress if exists"""
    if PROGRESS_FILE.exists():
        with open(PROGRESS_FILE, 'r') as f:
            return json.load(f)
    return {'samples_generated': 0, 'start_time': datetime.now().isoformat()}

def save_progress(completed_count):
    """Save progress to file"""
    progress = {
        'samples_generated': completed_count,
        'total_target': TOTAL_NEW_SAMPLES,
        'start_time': datetime.now().isoformat(),
        'timestamp': datetime.now().isoformat()
    }
    with open(PROGRESS_FILE, 'w') as f:
        json.dump(progress, f, indent=2)

def generate_perlin_noise(shape, scale=50):
    """Generate Perlin-like noise using interpolated random gradients."""
    h, w = shape
    
    grid_h = (h // scale) + 2
    grid_w = (w // scale) + 2
    
    gradients = np.random.uniform(-1, 1, (grid_h, grid_w))
    
    x = np.linspace(0, grid_w - 1, w)
    y = np.linspace(0, grid_h - 1, h)
    
    from scipy.interpolate import RegularGridInterpolator
    
    points = (np.arange(grid_h), np.arange(grid_w))
    interp_func = RegularGridInterpolator(points, gradients, bounds_error=False, 
                                         fill_value=0, method='cubic')
    
    yy, xx = np.meshgrid(y, x, indexing='ij')
    coords = np.stack([yy, xx], axis=-1)
    
    noise = interp_func(coords)
    noise = (noise - noise.min()) / (noise.max() - noise.min() + 1e-6)
    
    return noise


def generate_smooth_background(difficulty_level):
    """Generate smooth, realistic IR background."""
    base_intensity = np.random.randint(120, 160)
    background = np.ones((IMAGE_SIZE, IMAGE_SIZE), dtype=np.float32) * base_intensity
    
    gradient_scale = np.random.uniform(0.05, 0.15)
    y, x = np.meshgrid(np.arange(IMAGE_SIZE), np.arange(IMAGE_SIZE), indexing='ij')
    
    thermal_gradient = gradient_scale * 30 * (
        0.5 * np.sin(2 * np.pi * x / IMAGE_SIZE) + 
        0.3 * np.cos(2 * np.pi * y / IMAGE_SIZE) +
        0.2 * np.sin(np.pi * (x + y) / (2 * IMAGE_SIZE))
    )
    
    background += thermal_gradient
    
    num_blobs = np.random.randint(2, 4)
    blob_noise = generate_perlin_noise((IMAGE_SIZE, IMAGE_SIZE), scale=60)
    
    for _ in range(num_blobs):
        blob_intensity = np.random.uniform(-8, 8)
        background += blob_noise * blob_intensity
    
    cy, cx = IMAGE_SIZE / 2, IMAGE_SIZE / 2
    distance = np.sqrt((x - cy) ** 2 + (y - cx) ** 2)
    max_distance = np.sqrt(cy ** 2 + cx ** 2)
    vignette = 1.0 - 0.05 * (distance / max_distance) ** 2
    background *= vignette
    
    background = np.clip(background, 100, 180)
    
    return background


def generate_bullet_hole_physics(center, radius, background_intensity, 
                                  material='metal', impact_angle=0):
    """Generate physically-realistic bullet hole using thermal physics."""
    
    hole_image = np.zeros((IMAGE_SIZE, IMAGE_SIZE), dtype=np.float32)
    hole_mask = np.zeros((IMAGE_SIZE, IMAGE_SIZE), dtype=bool)
    
    cy, cx = int(center[0]), int(center[1])
    
    y, x = np.meshgrid(np.arange(IMAGE_SIZE), np.arange(IMAGE_SIZE), indexing='ij')
    dx = x - cx
    dy = y - cy
    distance = np.sqrt(dx ** 2 + dy ** 2)
    
    if impact_angle != 0:
        aspect_ratio = 1.0 / (1.0 + 0.3 * np.sin(impact_angle))
        distance_ellipse = np.sqrt((dx ** 2) / (aspect_ratio ** 2) + dy ** 2)
    else:
        distance_ellipse = distance
    
    hole_mask = distance <= radius
    
    if material == 'metal':
        hole_center_intensity = np.random.uniform(15, 40)
        falloff_rate = 3.0
    elif material == 'wood':
        hole_center_intensity = np.random.uniform(35, 60)
        falloff_rate = 2.0
    else:  # fabric
        hole_center_intensity = np.random.uniform(50, 80)
        falloff_rate = 1.0
    
    normalized_distance = np.clip(distance_ellipse / radius, 0, 1)
    gradient_inner = np.exp(-falloff_rate * (1 - normalized_distance) ** 2)
    gradient_intensity = hole_center_intensity + gradient_inner * (background_intensity - hole_center_intensity)
    
    hole_image[hole_mask] = gradient_intensity[hole_mask]
    
    edge_width = 2 + np.random.randint(0, 3)
    edge_mask = (distance > radius) & (distance <= radius + edge_width)
    
    if material == 'metal':
        edge_intensity = np.random.uniform(190, 220)
    elif material == 'wood':
        edge_intensity = np.random.uniform(170, 200)
    else:  # fabric
        edge_intensity = np.random.uniform(150, 180)
    
    edge_distance = (distance - radius) / (edge_width + 1)
    edge_distance = np.clip(edge_distance, 0, 1)
    edge_falloff = np.exp(-2 * edge_distance ** 2)
    
    hole_image[edge_mask] = edge_intensity * edge_falloff[edge_mask]
    
    irregularity_scale = np.random.uniform(0.1, 0.3)
    irregularity = generate_perlin_noise((IMAGE_SIZE, IMAGE_SIZE), scale=40)
    irregularity = (irregularity - 0.5) * irregularity_scale * radius
    
    boundary_mask = (distance > radius - 1) & (distance < radius + edge_width + 2)
    hole_image[boundary_mask] *= (1 + irregularity[boundary_mask] * 0.05)
    
    num_cracks = np.random.randint(2, 6)
    
    for _ in range(num_cracks):
        crack_angle = np.random.uniform(0, 2 * np.pi)
        crack_length = radius + np.random.randint(5, 20)
        
        crack_angles = np.arctan2(dy, dx)
        angle_diff = np.abs(crack_angles - crack_angle)
        angle_diff = np.minimum(angle_diff, 2 * np.pi - angle_diff)
        
        crack_mask = (angle_diff < 0.1) & (distance >= radius * 0.7) & (distance <= crack_length)
        crack_intensity = background_intensity * np.random.uniform(0.6, 0.8)
        hole_image[crack_mask] = crack_intensity
    
    from scipy.ndimage import gaussian_filter
    hole_image = gaussian_filter(hole_image, sigma=0.5)
    
    return hole_image, hole_mask


def add_sensor_noise(image, difficulty_level):
    """Add realistic IR sensor noise."""
    
    if difficulty_level == 'easy':
        thermal_std = np.random.uniform(1.5, 2.5)
        hot_pixel_ratio = 0.0001
        banding_prob = 0.0
    elif difficulty_level == 'medium':
        thermal_std = np.random.uniform(3.0, 4.5)
        hot_pixel_ratio = 0.0003
        banding_prob = 0.1
    else:  # hard
        thermal_std = np.random.uniform(5.0, 7.0)
        hot_pixel_ratio = 0.0008
        banding_prob = 0.3
    
    noisy_image = image.copy()
    thermal_noise = np.random.normal(0, thermal_std, image.shape)
    noisy_image += thermal_noise
    
    num_anomalies = int(IMAGE_SIZE * IMAGE_SIZE * hot_pixel_ratio)
    if num_anomalies > 0:
        anomaly_coords = np.random.choice(IMAGE_SIZE * IMAGE_SIZE, min(num_anomalies, IMAGE_SIZE*IMAGE_SIZE), replace=False)
        anomaly_y = anomaly_coords // IMAGE_SIZE
        anomaly_x = anomaly_coords % IMAGE_SIZE
        
        for ay, ax in zip(anomaly_y, anomaly_x):
            if np.random.random() < 0.5:
                noisy_image[ay, ax] = 255
            else:
                noisy_image[ay, ax] = 0
    
    if np.random.random() < banding_prob:
        if np.random.random() < 0.5:
            band_y = np.random.randint(0, IMAGE_SIZE)
            band_height = np.random.randint(1, 3)
            band_intensity = np.random.uniform(-3, 3)
            noisy_image[band_y:band_y+band_height, :] += band_intensity
        else:
            band_x = np.random.randint(0, IMAGE_SIZE)
            band_width = np.random.randint(1, 3)
            band_intensity = np.random.uniform(-3, 3)
            noisy_image[:, band_x:band_x+band_width] += band_intensity
    
    return noisy_image


def compute_features(binary_mask, intensity_image):
    """Compute 17 features from binary mask and intensity image."""
    from scipy import ndimage
    
    features = {}
    labeled_mask, num_features = ndimage.label(binary_mask)
    
    if num_features == 0:
        return {
            'area': 0, 'perimeter': 0, 'circularity': 0, 'eccentricity': 0,
            'aspect_ratio': 0, 'mean_intensity': 0, 'std_intensity': 0,
            'min_intensity': 0, 'max_intensity': 0, 'contrast': 0,
            'entropy': 0, 'edge_density': 0, 'gradient_mean': 0, 'gradient_std': 0,
            'contour_variance': 0, 'fractal_dimension': 0, 'hole_depth_estimate': 0
        }
    
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
        circularity = np.clip(circularity, 0, 1)
    else:
        circularity = 0
    features['circularity'] = float(circularity)
    
    contours, _ = cv2.findContours(largest_mask.astype(np.uint8), 
                                    cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    if len(contours) > 0:
        cnt = max(contours, key=cv2.contourArea)
        if len(cnt) >= 5:
            ellipse = cv2.fitEllipse(cnt)
            (cx, cy), (major, minor), angle = ellipse
            aspect_ratio = max(major, minor) / (min(major, minor) + 1e-6)
            eccentricity = np.sqrt(1 - (min(major, minor) / (max(major, minor) + 1e-6)) ** 2)
        else:
            aspect_ratio = 1.0
            eccentricity = 0.0
    else:
        aspect_ratio = 1.0
        eccentricity = 0.0
    
    features['aspect_ratio'] = float(np.clip(aspect_ratio, 0.1, 10))
    features['eccentricity'] = float(np.clip(eccentricity, 0, 1))
    
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
    
    hist, _ = np.histogram(mask_intensity, bins=32, range=(0, 256))
    hist = hist / (hist.sum() + 1e-6)
    entropy = -np.sum(hist[hist > 0] * np.log2(hist[hist > 0] + 1e-6))
    features['entropy'] = float(entropy)
    
    edges = cv2.Canny(intensity_image.astype(np.uint8), 50, 150)
    edge_density = np.sum(edges[largest_mask] > 0) / (area + 1)
    features['edge_density'] = float(np.clip(edge_density, 0, 1))
    
    gx = cv2.Sobel(intensity_image.astype(np.float32), cv2.CV_32F, 1, 0, ksize=3)
    gy = cv2.Sobel(intensity_image.astype(np.float32), cv2.CV_32F, 0, 1, ksize=3)
    gradient_magnitude = np.sqrt(gx ** 2 + gy ** 2)
    
    gradient_in_mask = gradient_magnitude[largest_mask]
    features['gradient_mean'] = float(np.mean(gradient_in_mask) if len(gradient_in_mask) > 0 else 0)
    features['gradient_std'] = float(np.std(gradient_in_mask) if len(gradient_in_mask) > 0 else 0)
    
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
    
    def estimate_fractal_dimension(mask, min_size=2, max_size=64):
        counts = []
        sizes = []
        size = min_size
        while size <= max_size and size <= mask.shape[0]:
            count = 0
            for i in range(0, mask.shape[0], size):
                for j in range(0, mask.shape[1], size):
                    box = mask[i:i+size, j:j+size]
                    if np.any(box):
                        count += 1
            counts.append(count)
            sizes.append(size)
            size *= 2
        if len(counts) > 1:
            log_sizes = np.log(sizes)
            log_counts = np.log(counts)
            coeffs = np.polyfit(log_sizes, log_counts, 1)
            fractal_dim = -coeffs[0]
        else:
            fractal_dim = 2.0
        return np.clip(fractal_dim, 1.0, 3.0)
    
    fractal_dim = estimate_fractal_dimension(largest_mask)
    features['fractal_dimension'] = float(fractal_dim)
    
    if area > 0 and features['mean_intensity'] < 128:
        depth_estimate = (128 - features['mean_intensity']) / 128.0
    else:
        depth_estimate = 0.0
    features['hole_depth_estimate'] = float(np.clip(depth_estimate, 0, 1))
    
    return features


def generate_non_bullet_artifact(background):
    """Generate non-bullet artifacts: scratches, dust, reflections"""
    
    img = background.copy()
    mask = np.zeros((IMAGE_SIZE, IMAGE_SIZE), dtype=bool)
    
    artifact_type = np.random.choice(['scratch', 'dust', 'reflection'])
    
    if artifact_type == 'scratch':
        x1, y1 = np.random.randint(10, IMAGE_SIZE - 10, 2)
        x2 = x1 + np.random.randint(-80, 80)
        y2 = y1 + np.random.randint(-80, 80)
        x2 = np.clip(x2, 0, IMAGE_SIZE - 1)
        y2 = np.clip(y2, 0, IMAGE_SIZE - 1)
        
        scratch_intensity = background[y1, x1] * 0.6
        cv2.line(img, (x1, y1), (x2, y2), int(scratch_intensity), thickness=1)
        
        if x2 != x1:
            y_interp = np.interp(np.arange(min(x1, x2), max(x1, x2) + 1),
                                [x1, x2], [y1, y2]).astype(int)
            for x, y in zip(np.arange(min(x1, x2), max(x1, x2) + 1), y_interp):
                if 0 <= x < IMAGE_SIZE and 0 <= y < IMAGE_SIZE:
                    mask[y, x] = True
    
    elif artifact_type == 'dust':
        num_dust = np.random.randint(2, 5)
        for _ in range(num_dust):
            dx = np.random.randint(5, 12)
            dy = np.random.randint(5, 12)
            dox = np.random.randint(0, IMAGE_SIZE - dx)
            doy = np.random.randint(0, IMAGE_SIZE - dy)
            
            dust_intensity = np.random.uniform(200, 230)
            img[doy:doy+dy, dox:dox+dx] = dust_intensity
            mask[doy:doy+dy, dox:dox+dx] = True
    
    else:  # reflection
        num_reflections = np.random.randint(1, 2)
        for _ in range(num_reflections):
            ref_x = np.random.randint(20, IMAGE_SIZE - 20)
            ref_y = np.random.randint(20, IMAGE_SIZE - 20)
            ref_size = np.random.randint(8, 15)
            
            y, x = np.meshgrid(np.arange(IMAGE_SIZE), np.arange(IMAGE_SIZE), indexing='ij')
            dx = x - ref_x
            dy = y - ref_y
            distance = np.sqrt(dx ** 2 + dy ** 2)
            
            ref_mask = np.exp(-(distance ** 2) / (2 * ref_size ** 2))
            ref_intensity = np.random.uniform(200, 230)
            
            img += ref_mask * ref_intensity * 0.5
            mask |= (ref_mask > 0.1)
    
    return img, mask


def generate_synthetic_sample(sample_id, label_type):
    """Generate a complete synthetic sample with realistic IR physics"""
    
    difficulty_rand = np.random.random()
    if difficulty_rand < EASY_RATIO:
        difficulty_level = 'easy'
    elif difficulty_rand < EASY_RATIO + MEDIUM_RATIO:
        difficulty_level = 'medium'
    else:
        difficulty_level = 'hard'
    
    background = generate_smooth_background(difficulty_level)
    
    img = background.copy()
    full_mask = np.zeros((IMAGE_SIZE, IMAGE_SIZE), dtype=bool)
    
    if label_type == 'bullet_hole':
        num_holes = np.random.choice([1, 2, 3], p=[0.7, 0.25, 0.05])
        
        for _ in range(num_holes):
            if difficulty_level == 'easy':
                radius = np.random.randint(15, 25)
                material = np.random.choice(['metal', 'wood'], p=[0.7, 0.3])
            elif difficulty_level == 'medium':
                radius = np.random.randint(10, 18)
                material = np.random.choice(['metal', 'wood', 'fabric'], p=[0.5, 0.3, 0.2])
            else:  # hard
                radius = np.random.randint(5, 12)
                material = np.random.choice(['metal', 'wood', 'fabric'], p=[0.3, 0.4, 0.3])
            
            margin = radius + 5
            cy = np.random.randint(margin, IMAGE_SIZE - margin)
            cx = np.random.randint(margin, IMAGE_SIZE - margin)
            
            impact_angle = np.random.uniform(0, np.pi / 6) if np.random.random() < 0.3 else 0
            
            hole_img, hole_mask = generate_bullet_hole_physics(
                (cy, cx), radius, background[cy, cx],
                material=material, impact_angle=impact_angle
            )
            
            img += hole_img
            full_mask |= hole_mask
    
    elif label_type == 'non_bullet':
        img, artifact_mask = generate_non_bullet_artifact(background)
        full_mask = artifact_mask
    
    else:  # ambiguous
        ambiguity_type = np.random.choice(['tiny_hole', 'blurry_hole', 'artifact_confusion'])
        
        if ambiguity_type == 'tiny_hole':
            radius = np.random.randint(3, 6)
            cy = np.random.randint(50, IMAGE_SIZE - 50)
            cx = np.random.randint(50, IMAGE_SIZE - 50)
            
            hole_img, hole_mask = generate_bullet_hole_physics(
                (cy, cx), radius, background[cy, cx],
                material='fabric', impact_angle=0
            )
            
            img += hole_img
            full_mask = hole_mask
        
        elif ambiguity_type == 'blurry_hole':
            radius = np.random.randint(12, 18)
            cy = np.random.randint(30, IMAGE_SIZE - 30)
            cx = np.random.randint(30, IMAGE_SIZE - 30)
            
            hole_img, hole_mask = generate_bullet_hole_physics(
                (cy, cx), radius, background[cy, cx],
                material='fabric', impact_angle=0
            )
            
            from scipy.ndimage import gaussian_filter
            hole_img = gaussian_filter(hole_img, sigma=3.0)
            img += hole_img
            full_mask = hole_mask
        
        else:  # artifact confusion
            img, artifact_mask = generate_non_bullet_artifact(background)
            full_mask = artifact_mask
    
    img = add_sensor_noise(img, difficulty_level)
    img = np.clip(img, 0, 255).astype(np.uint8)
    
    features = compute_features(full_mask, img.astype(np.float32))
    
    metadata = {
        'sample_id': sample_id,
        'label': label_type,
        'difficulty_level': difficulty_level,
    }
    
    return img, label_type, features, metadata


def extend_dataset_batch():
    """Main extension function"""
    
    print("\n" + "=" * 80)
    print("EFFICIENT DATASET EXTENSION - Batch Generation")
    print("=" * 80)
    
    # Load existing dataset
    print("\n[1] Loading existing dataset...")
    try:
        existing_df = pd.read_csv(DATASET_DIR / "annotations.csv")
        num_existing = len(existing_df)
        max_existing_id = existing_df['sample_id'].max()
        
        print(f"? Loaded {num_existing} existing samples")
        print(f"  Max sample ID: {max_existing_id}")
        
    except Exception as e:
        print(f"? ERROR: {e}")
        return False
    
    # Determine label distribution
    num_bullet_holes = int(TOTAL_NEW_SAMPLES * BULLET_HOLE_RATIO)
    num_non_bullets = int(TOTAL_NEW_SAMPLES * NON_BULLET_RATIO)
    num_ambiguous = TOTAL_NEW_SAMPLES - num_bullet_holes - num_non_bullets
    
    labels = ['bullet_hole'] * num_bullet_holes + \
             ['non_bullet'] * num_non_bullets + \
             ['ambiguous'] * num_ambiguous
    
    np.random.shuffle(labels)
    
    print(f"\n[2] Generating {TOTAL_NEW_SAMPLES} new samples in batches...")
    print(f"    Label distribution: {num_bullet_holes} bullet holes, {num_non_bullets} non-bullets, {num_ambiguous} ambiguous")
    print("-" * 80)
    
    all_new_data = []
    
    for batch_idx in range(0, TOTAL_NEW_SAMPLES, BATCH_SIZE):
        batch_end = min(batch_idx + BATCH_SIZE, TOTAL_NEW_SAMPLES)
        batch_samples = batch_end - batch_idx
        
        print(f"\nBatch {batch_idx // BATCH_SIZE + 1}: Generating samples {batch_idx + 1}-{batch_end}...")
        
        for local_idx in range(batch_samples):
            global_idx = batch_idx + local_idx
            sample_id = max_existing_id + 1 + global_idx
            label = labels[global_idx]
            
            # Generate sample
            img, _, features, metadata = generate_synthetic_sample(sample_id, label)
            
            # Save image
            image_path = IMAGES_DIR / f"{sample_id:06d}.png"
            cv2.imwrite(str(image_path), img)
            
            # Compile row
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
            }
            
            all_new_data.append(row)
            
            if (local_idx + 1) % 100 == 0:
                print(f"  ? {local_idx + 1}/{batch_samples} samples generated")
    
    print(f"\n? Generated {len(all_new_data)} samples")
    
    # Merge with existing
    print(f"\n[3] Merging datasets...")
    new_df = pd.DataFrame(all_new_data)
    merged_df = pd.concat([existing_df, new_df], ignore_index=False)
    merged_df = merged_df.reset_index(drop=True)
    
    print(f"? Merged: {num_existing} existing + {len(new_df)} new = {len(merged_df)} total")
    
    # Validate and save
    print(f"\n[4] Validating merged dataset...")
    missing = merged_df.isnull().sum().sum()
    print(f"? Missing values: {missing}")
    
    label_counts = merged_df['label'].value_counts()
    print(f"\n? Label distribution in merged dataset:")
    for label, count in label_counts.items():
        pct = 100 * count / len(merged_df)
        print(f"  {label:15s}: {count:6,d} ({pct:5.1f}%)")
    
    print(f"\n[5] Saving merged dataset...")
    csv_path = DATASET_DIR / "annotations.csv"
    merged_df.to_csv(csv_path, index=False)
    print(f"? Saved to: {csv_path}")
    
    # Final report
    print("\n" + "=" * 80)
    print("DATASET EXTENSION COMPLETE")
    print("=" * 80)
    print(f"\n? Summary:")
    print(f"  Original samples:  {num_existing:,}")
    print(f"  New samples:       {len(new_df):,}")
    print(f"  Total samples:     {len(merged_df):,}")
    print(f"  Target:            ~{TOTAL_NEW_SAMPLES + num_existing:,}")
    print(f"  Expansion ratio:   {len(merged_df) / num_existing:.2f}x")
    
    return True


if __name__ == '__main__':
    try:
        success = extend_dataset_batch()
        sys.exit(0 if success else 1)
    except Exception as e:
        print(f"\n? FATAL ERROR: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)
