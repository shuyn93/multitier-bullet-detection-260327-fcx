#!/usr/bin/env python3
"""
FIXED DATASET GENERATOR - Addresses all visibility issues
Generates synthetic IR images where bullet holes are CLEARLY VISIBLE
"""

import numpy as np
import cv2
import pandas as pd
from pathlib import Path
from scipy import ndimage
import warnings
warnings.filterwarnings('ignore')

# Configuration
DATASET_SIZE = 10000
IMAGE_SIZE = 256
DATASET_DIR = Path("dataset")
IMAGES_DIR = DATASET_DIR / "images"

BULLET_HOLE_RATIO = 0.50
NON_BULLET_RATIO = 0.30
AMBIGUOUS_RATIO = 0.20

EASY_RATIO = 0.30
MEDIUM_RATIO = 0.50
HARD_RATIO = 0.20

class IRPhysicsConfig:
    """Fixed configuration for realistic IR imaging"""
    THERMAL_NOISE_STD_EASY = 2.0
    THERMAL_NOISE_STD_MEDIUM = 3.0
    THERMAL_NOISE_STD_HARD = 5.0
    
    BLUR_RADIUS_EASY = (0.5, 1.0)
    BLUR_RADIUS_MEDIUM = (1.0, 1.5)
    BLUR_RADIUS_HARD = (1.5, 2.5)
    
    ILLUM_VAR_EASY = (0.05, 0.15)
    ILLUM_VAR_MEDIUM = (0.15, 0.35)
    ILLUM_VAR_HARD = (0.35, 0.50)

def compute_features(binary_mask, intensity_image):
    """Compute 17 features from binary mask and intensity image"""
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
    features['fractal_dimension'] = 1.5
    
    if area > 0 and features['mean_intensity'] < 128:
        depth_estimate = (128 - features['mean_intensity']) / 128.0
    else:
        depth_estimate = 0.0
    features['hole_depth_estimate'] = float(np.clip(depth_estimate, 0, 1))
    
    return features

def generate_bullet_hole_fixed(base_img, center, size_params, difficulty_level):
    """
    FIXED: Generate bullet hole with STRONG contrast
    """
    img = base_img.copy()
    mean_radius, std_radius, irregularity = size_params
    actual_radius = max(3, int(np.random.normal(mean_radius, std_radius)))
    
    y, x = np.ogrid[-actual_radius:actual_radius+1, -actual_radius:actual_radius+1]
    
    if np.random.random() < irregularity:
        angle = np.random.uniform(0, 2 * np.pi)
        aspect = np.random.uniform(0.6, 1.5)
        x_rot = x * np.cos(angle) + y * np.sin(angle)
        y_rot = -x * np.sin(angle) + y * np.cos(angle)
        mask = (x_rot ** 2 / (actual_radius * aspect) ** 2 + 
                y_rot ** 2 / actual_radius ** 2) <= 1
    else:
        mask = (x ** 2 + y ** 2) <= actual_radius ** 2
    
    cx, cy = int(center[0]), int(center[1])
    x_start = max(0, cx - actual_radius - 1)
    x_end = min(img.shape[1], cx + actual_radius + 2)
    y_start = max(0, cy - actual_radius - 1)
    y_end = min(img.shape[0], cy + actual_radius + 2)
    
    mask_y, mask_x = np.where(mask)
    valid_mask = ((mask_x + x_start >= 0) & (mask_x + x_start < img.shape[1]) &
                  (mask_y + y_start >= 0) & (mask_y + y_start < img.shape[0]))
    
    for my, mx in zip(mask_y[valid_mask], mask_x[valid_mask]):
        img_y = my + y_start
        img_x = mx + x_start
        if 0 <= img_y < img.shape[0] and 0 <= img_x < img.shape[1]:
            # FIX: STRONG darkening (80-90% reduction instead of 70%)
            # Make hole MUCH darker than background
            img[img_y, img_x] = img[img_y, img_x] * 0.1
    
    return img, mask

def generate_base_thermal_image(difficulty_level):
    """Generate base thermal image"""
    base_intensity = np.random.randint(100, 140)
    
    img = np.ones((IMAGE_SIZE, IMAGE_SIZE), dtype=np.float32) * base_intensity
    
    y, x = np.meshgrid(np.arange(IMAGE_SIZE), np.arange(IMAGE_SIZE), indexing='ij')
    
    gradient = (0.2 * np.sin(2 * np.pi * x / IMAGE_SIZE) * 15 +
                0.2 * np.cos(2 * np.pi * y / IMAGE_SIZE) * 15 +
                0.1 * np.sin(np.pi * (x + y) / IMAGE_SIZE) * 10)
    
    img += gradient
    
    num_blobs = np.random.randint(2, 5)
    for _ in range(num_blobs):
        blob_x = np.random.randint(0, IMAGE_SIZE)
        blob_y = np.random.randint(0, IMAGE_SIZE)
        blob_size = np.random.uniform(30, 80)
        blob_intensity = np.random.uniform(-8, 8)
        
        dy = np.arange(IMAGE_SIZE) - blob_y
        dx = np.arange(IMAGE_SIZE) - blob_x
        xx, yy = np.meshgrid(dx, dy, indexing='ij')
        distance = np.sqrt(xx ** 2 + yy ** 2)
        
        blob_mask = np.exp(-(distance ** 2) / (2 * blob_size ** 2))
        img += blob_mask.T * blob_intensity
    
    return np.clip(img, 10, 240)

def apply_ir_degradations_fixed(img, difficulty_level):
    """
    FIXED: Apply degradations BEFORE creating holes
    This way holes remain visible despite noise
    """
    if difficulty_level == 'easy':
        thermal_noise_std = IRPhysicsConfig.THERMAL_NOISE_STD_EASY
        blur_radius = np.random.uniform(*IRPhysicsConfig.BLUR_RADIUS_EASY)
    elif difficulty_level == 'medium':
        thermal_noise_std = IRPhysicsConfig.THERMAL_NOISE_STD_MEDIUM
        blur_radius = np.random.uniform(*IRPhysicsConfig.BLUR_RADIUS_MEDIUM)
    else:
        thermal_noise_std = IRPhysicsConfig.THERMAL_NOISE_STD_HARD
        blur_radius = np.random.uniform(*IRPhysicsConfig.BLUR_RADIUS_HARD)
    
    img = img.astype(np.float32)
    
    # Thermal noise (BEFORE blur)
    img += np.random.normal(0, thermal_noise_std, img.shape)
    
    # Light blur (much smaller than original)
    kernel_size = max(3, int(blur_radius * 2 + 1))
    if kernel_size % 2 == 0:
        kernel_size += 1
    img = cv2.GaussianBlur(img, (kernel_size, kernel_size), blur_radius)
    
    img = np.clip(img, 0, 255)
    return img

def apply_illumination_variance(img, variance_level):
    """Apply illumination variance"""
    img = img.astype(np.float32)
    
    y, x = np.meshgrid(np.arange(IMAGE_SIZE), np.arange(IMAGE_SIZE), indexing='ij')
    center_y, center_x = IMAGE_SIZE / 2, IMAGE_SIZE / 2
    distance_from_center = np.sqrt((x - center_x) ** 2 + (y - center_y) ** 2)
    max_distance = np.sqrt(center_x ** 2 + center_y ** 2)
    
    vignette = 1 - (variance_level * (distance_from_center / max_distance) ** 2)
    vignette = np.clip(vignette, 0.7, 1.0)
    
    img *= vignette
    img = np.clip(img, 0, 255)
    return img

def generate_synthetic_sample_fixed(sample_id, label_type):
    """
    FIXED: Generate sample with proper feature visibility
    - Create base image
    - Apply light degradations
    - CREATE HOLES (with strong contrast)
    - Apply final contrast enhancement
    """
    difficulty_rand = np.random.random()
    if difficulty_rand < EASY_RATIO:
        difficulty_level = 'easy'
    elif difficulty_rand < EASY_RATIO + MEDIUM_RATIO:
        difficulty_level = 'medium'
    else:
        difficulty_level = 'hard'
    
    # Generate base
    base_img = generate_base_thermal_image(difficulty_level)
    img = base_img.copy()
    full_mask = np.zeros((IMAGE_SIZE, IMAGE_SIZE), dtype=bool)
    
    # Apply degradations FIRST
    img = apply_ir_degradations_fixed(img, difficulty_level)
    
    # NOW create holes/artifacts (after degradation, so they remain visible)
    if label_type == 'bullet_hole':
        num_holes = np.random.choice([1, 2], p=[0.7, 0.3])
        
        for _ in range(num_holes):
            if difficulty_level == 'easy':
                mean_radius = np.random.uniform(18, 28)
                std_radius = 2
                irregularity = 0.2
            elif difficulty_level == 'medium':
                mean_radius = np.random.uniform(10, 22)
                std_radius = 3
                irregularity = 0.3
            else:
                mean_radius = np.random.uniform(6, 15)
                std_radius = 2
                irregularity = 0.4
            
            margin = int(mean_radius) + 5
            cx = np.random.randint(margin, IMAGE_SIZE - margin)
            cy = np.random.randint(margin, IMAGE_SIZE - margin)
            
            img, hole_mask = generate_bullet_hole_fixed(img, (cx, cy), 
                                                       (mean_radius, std_radius, irregularity),
                                                       difficulty_level)
            
            cy_start = max(0, cy - int(mean_radius) - 1)
            cy_end = min(IMAGE_SIZE, cy + int(mean_radius) + 2)
            cx_start = max(0, cx - int(mean_radius) - 1)
            cx_end = min(IMAGE_SIZE, cx + int(mean_radius) + 2)
            
            h_mask, w_mask = hole_mask.shape
            h_region = min(h_mask, cy_end - cy_start)
            w_region = min(w_mask, cx_end - cx_start)
            
            full_mask[cy_start:cy_start+h_region, cx_start:cx_start+w_region] |= hole_mask[:h_region, :w_region]
    
    elif label_type == 'non_bullet':
        # Small artifacts
        artifact_type = np.random.choice(['dust', 'scratch'])
        
        if artifact_type == 'dust':
            num_dust = np.random.randint(1, 3)
            for _ in range(num_dust):
                dx = np.random.randint(8, 20)
                dy = np.random.randint(8, 20)
                dox = np.random.randint(10, IMAGE_SIZE - dx - 10)
                doy = np.random.randint(10, IMAGE_SIZE - dy - 10)
                
                dust_region = img[doy:doy+dy, dox:dox+dx]
                dust_region = dust_region * 0.8 + np.random.uniform(20, 40)
                img[doy:doy+dy, dox:dox+dx] = np.clip(dust_region, 0, 255)
                
                full_mask[doy:doy+dy, dox:dox+dx] = True
        else:
            x1, y1 = np.random.randint(0, IMAGE_SIZE-50, 2)
            x2, y2 = x1 + np.random.randint(30, 70), y1 + np.random.randint(-30, 30)
            x2 = np.clip(x2, 0, IMAGE_SIZE - 1)
            y2 = np.clip(y2, 0, IMAGE_SIZE - 1)
            
            cv2.line(img, (x1, y1), (x2, y2), 
                    int(np.random.uniform(60, 100)), thickness=1)
    
    else:  # ambiguous
        if np.random.random() < 0.5:
            # Tiny hole
            cx = np.random.randint(30, IMAGE_SIZE - 30)
            cy = np.random.randint(30, IMAGE_SIZE - 30)
            
            mean_radius = np.random.uniform(3, 7)
            std_radius = 1
            irregularity = 0.4
            
            img, hole_mask = generate_bullet_hole_fixed(img, (cx, cy),
                                                       (mean_radius, std_radius, irregularity),
                                                       'hard')
        else:
            # Medium hole
            cx = np.random.randint(30, IMAGE_SIZE - 30)
            cy = np.random.randint(30, IMAGE_SIZE - 30)
            
            mean_radius = np.random.uniform(12, 20)
            std_radius = 2
            irregularity = 0.3
            
            img, hole_mask = generate_bullet_hole_fixed(img, (cx, cy),
                                                       (mean_radius, std_radius, irregularity),
                                                       'medium')
    
    # Apply illumination variance
    illum_variance = {'easy': np.random.uniform(*IRPhysicsConfig.ILLUM_VAR_EASY),
                     'medium': np.random.uniform(*IRPhysicsConfig.ILLUM_VAR_MEDIUM),
                     'hard': np.random.uniform(*IRPhysicsConfig.ILLUM_VAR_HARD)}[difficulty_level]
    img = apply_illumination_variance(img, illum_variance)
    
    # Final contrast enhancement using CLAHE
    img = np.clip(img, 0, 255).astype(np.uint8)
    clahe = cv2.createCLAHE(clipLimit=2.0, tileGridSize=(8, 8))
    img = clahe.apply(img)
    
    blur_level = {'easy': np.random.uniform(*IRPhysicsConfig.BLUR_RADIUS_EASY) / 5,
                 'medium': np.random.uniform(*IRPhysicsConfig.BLUR_RADIUS_MEDIUM) / 5,
                 'hard': np.random.uniform(*IRPhysicsConfig.BLUR_RADIUS_HARD) / 5}[difficulty_level]
    noise_level = {'easy': 0.2, 'medium': 0.5, 'hard': 0.8}[difficulty_level]
    
    features = compute_features(full_mask, img.astype(np.float32))
    
    metadata = {
        'sample_id': sample_id,
        'label': label_type,
        'difficulty_level': difficulty_level,
        'noise_level': noise_level,
        'blur_level': blur_level,
        'illumination_variance': illum_variance,
    }
    
    return img, label_type, features, metadata

def generate_dataset_fixed():
    """Generate fixed dataset"""
    
    DATASET_DIR.mkdir(exist_ok=True)
    IMAGES_DIR.mkdir(exist_ok=True)
    
    print(f"Generating FIXED dataset with {DATASET_SIZE} samples...")
    print(f"Output: {DATASET_DIR.absolute()}")
    
    num_bullet_holes = int(DATASET_SIZE * BULLET_HOLE_RATIO)
    num_non_bullets = int(DATASET_SIZE * NON_BULLET_RATIO)
    num_ambiguous = DATASET_SIZE - num_bullet_holes - num_non_bullets
    
    labels = ['bullet_hole'] * num_bullet_holes + \
             ['non_bullet'] * num_non_bullets + \
             ['ambiguous'] * num_ambiguous
    
    np.random.shuffle(labels)
    
    all_data = []
    
    for sample_id in range(DATASET_SIZE):
        if (sample_id + 1) % 500 == 0:
            print(f"  Progress: {sample_id + 1} / {DATASET_SIZE}")
        
        label = labels[sample_id]
        img, label_out, features, metadata = generate_synthetic_sample_fixed(sample_id, label)
        
        image_path = IMAGES_DIR / f"{sample_id:06d}.png"
        cv2.imwrite(str(image_path), img)
        
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
    
    df = pd.DataFrame(all_data)
    csv_path = DATASET_DIR / "annotations.csv"
    df.to_csv(csv_path, index=False)
    
    print(f"\nOK: Dataset generation complete!")
    print(f"  Images: {IMAGES_DIR.absolute()}")
    print(f"  Annotations: {csv_path.absolute()}")

if __name__ == '__main__':
    generate_dataset_fixed()
