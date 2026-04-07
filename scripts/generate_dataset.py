"""
Synthetic Dataset Generator for Bullet Hole Detection System (IR Imaging)

Generates 10,000 synthetic IR grayscale images with:
- Realistic IR physics simulation
- Comprehensive feature vectors (17 features)
- Metadata with difficulty levels and noise characteristics
- Proper label distribution (50% bullet_hole, 30% non_bullet, 20% ambiguous)

Output:
- /dataset/images/{sample_id}.png
- /dataset/annotations.csv
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
warnings.filterwarnings('ignore')

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

# Difficulty distribution
EASY_RATIO = 0.30
MEDIUM_RATIO = 0.50
HARD_RATIO = 0.20

# ============================================================================
# IR Physics Simulation Parameters
# ============================================================================

class IRPhysicsConfig:
    """Configuration for realistic IR imaging characteristics"""
    
    # Sensor noise (dB)
    THERMAL_NOISE_STD_EASY = 2.0
    THERMAL_NOISE_STD_MEDIUM = 4.0
    THERMAL_NOISE_STD_HARD = 8.0
    
    # Salt-and-pepper noise ratio
    SALT_PEPPER_EASY = 0.001
    SALT_PEPPER_MEDIUM = 0.005
    SALT_PEPPER_HARD = 0.015
    
    # Thermal blur radius (simulates sensor resolution)
    BLUR_RADIUS_EASY = (1.0, 1.5)
    BLUR_RADIUS_MEDIUM = (1.5, 2.5)
    BLUR_RADIUS_HARD = (2.5, 4.0)
    
    # Illumination variance
    ILLUM_VAR_EASY = (0.05, 0.15)
    ILLUM_VAR_MEDIUM = (0.15, 0.35)
    ILLUM_VAR_HARD = (0.35, 0.60)
    
    # Dead pixel ratio
    DEAD_PIXEL_EASY = 0.0001
    DEAD_PIXEL_MEDIUM = 0.0005
    DEAD_PIXEL_HARD = 0.002
    
    # Banding noise frequency
    BANDING_FREQ_EASY = 0.0
    BANDING_FREQ_MEDIUM = 0.1
    BANDING_FREQ_HARD = 0.3

# ============================================================================
# Feature Computation
# ============================================================================

def compute_features(binary_mask, intensity_image):
    """
    Compute 17 features from binary mask and intensity image.
    
    Returns dict with:
    - Geometry: area, perimeter, circularity, eccentricity, aspect_ratio
    - Intensity: mean, std, min, max, contrast
    - Texture: entropy, edge_density, gradient_mean, gradient_std
    - Shape: contour_variance, fractal_dimension, hole_depth_estimate
    """
    features = {}
    
    # ---- Geometry Features ----
    labeled_mask, num_features = ndimage.label(binary_mask)
    
    if num_features == 0:
        # Empty mask
        return {
            'area': 0, 'perimeter': 0, 'circularity': 0, 'eccentricity': 0,
            'aspect_ratio': 0, 'mean_intensity': 0, 'std_intensity': 0,
            'min_intensity': 0, 'max_intensity': 0, 'contrast': 0,
            'entropy': 0, 'edge_density': 0, 'gradient_mean': 0, 'gradient_std': 0,
            'contour_variance': 0, 'fractal_dimension': 0, 'hole_depth_estimate': 0
        }
    
    # Use largest connected component
    sizes = ndimage.sum(binary_mask, labeled_mask, range(num_features + 1))
    largest_label = np.argmax(sizes)
    largest_mask = (labeled_mask == largest_label)
    
    # Area
    area = np.sum(largest_mask)
    features['area'] = float(area)
    
    # Perimeter (approximation)
    edges = cv2.Canny((largest_mask.astype(np.uint8) * 255), 50, 150)
    perimeter = np.sum(edges > 0)
    features['perimeter'] = float(perimeter)
    
    # Circularity (compactness)
    if perimeter > 0:
        circularity = 4 * np.pi * area / (perimeter ** 2 + 1e-6)
        circularity = np.clip(circularity, 0, 1)
    else:
        circularity = 0
    features['circularity'] = float(circularity)
    
    # Eccentricity and aspect ratio from moments
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
    
    # ---- Intensity Features ----
    mask_intensity = intensity_image[largest_mask]
    if len(mask_intensity) > 0:
        features['mean_intensity'] = float(np.mean(mask_intensity))
        features['std_intensity'] = float(np.std(mask_intensity))
        features['min_intensity'] = float(np.min(mask_intensity))
        features['max_intensity'] = float(np.max(mask_intensity))
        
        # Contrast (RMS contrast)
        mean_val = np.mean(mask_intensity)
        contrast = float(np.sqrt(np.mean((mask_intensity - mean_val) ** 2)))
    else:
        features['mean_intensity'] = 0
        features['std_intensity'] = 0
        features['min_intensity'] = 0
        features['max_intensity'] = 0
        contrast = 0
    
    features['contrast'] = float(contrast)
    
    # ---- Texture Features ----
    
    # Entropy
    hist, _ = np.histogram(mask_intensity, bins=32, range=(0, 256))
    hist = hist / (hist.sum() + 1e-6)
    entropy = -np.sum(hist[hist > 0] * np.log2(hist[hist > 0] + 1e-6))
    features['entropy'] = float(entropy)
    
    # Edge density
    edges = cv2.Canny(intensity_image.astype(np.uint8), 50, 150)
    edge_density = np.sum(edges[largest_mask] > 0) / (area + 1)
    features['edge_density'] = float(np.clip(edge_density, 0, 1))
    
    # Gradient statistics
    gx = cv2.Sobel(intensity_image.astype(np.float32), cv2.CV_32F, 1, 0, ksize=3)
    gy = cv2.Sobel(intensity_image.astype(np.float32), cv2.CV_32F, 0, 1, ksize=3)
    gradient_magnitude = np.sqrt(gx ** 2 + gy ** 2)
    
    gradient_in_mask = gradient_magnitude[largest_mask]
    features['gradient_mean'] = float(np.mean(gradient_in_mask) if len(gradient_in_mask) > 0 else 0)
    features['gradient_std'] = float(np.std(gradient_in_mask) if len(gradient_in_mask) > 0 else 0)
    
    # ---- Shape Irregularity ----
    
    # Contour variance (roughness of boundary)
    if len(contours) > 0:
        cnt = contours[0]
        contour_points = cnt[:, 0, :]
        if len(contour_points) > 3:
            # Compute distances from centroid
            centroid = np.mean(contour_points, axis=0)
            distances = np.linalg.norm(contour_points - centroid, axis=1)
            contour_variance = float(np.std(distances) / (np.mean(distances) + 1e-6))
        else:
            contour_variance = 0.0
    else:
        contour_variance = 0.0
    
    features['contour_variance'] = float(np.clip(contour_variance, 0, 2))
    
    # Fractal dimension (approximation using box-counting)
    fractal_dim = estimate_fractal_dimension(largest_mask)
    features['fractal_dimension'] = float(fractal_dim)
    
    # Hole depth estimate (simulated from intensity gradient)
    if area > 0 and features['mean_intensity'] < 128:
        # Darker regions = deeper holes
        depth_estimate = (128 - features['mean_intensity']) / 128.0
    else:
        depth_estimate = 0.0
    features['hole_depth_estimate'] = float(np.clip(depth_estimate, 0, 1))
    
    return features


def estimate_fractal_dimension(binary_mask, min_size=2, max_size=64):
    """Estimate fractal dimension using box-counting method"""
    counts = []
    sizes = []
    
    size = min_size
    while size <= max_size and size <= binary_mask.shape[0]:
        count = 0
        for i in range(0, binary_mask.shape[0], size):
            for j in range(0, binary_mask.shape[1], size):
                box = binary_mask[i:i+size, j:j+size]
                if np.any(box):
                    count += 1
        counts.append(count)
        sizes.append(size)
        size *= 2
    
    if len(counts) > 1:
        # Fit line to log-log plot
        log_sizes = np.log(sizes)
        log_counts = np.log(counts)
        coeffs = np.polyfit(log_sizes, log_counts, 1)
        fractal_dim = -coeffs[0]
    else:
        fractal_dim = 2.0
    
    return np.clip(fractal_dim, 1.0, 3.0)

# ============================================================================
# Image Generation Functions
# ============================================================================

def generate_bullet_hole(base_img, center, size_params, difficulty_level):
    """
    Generate a bullet hole on the base image.
    
    Args:
        base_img: Base thermal image
        center: (x, y) center position
        size_params: (mean_radius, std_radius, irregularity)
        difficulty_level: 'easy', 'medium', 'hard'
    
    Returns:
        Modified image with bullet hole
    """
    img = base_img.copy()
    
    # Hole parameters
    mean_radius, std_radius, irregularity = size_params
    actual_radius = max(3, int(np.random.normal(mean_radius, std_radius)))
    
    # Create mask for hole
    y, x = np.ogrid[-actual_radius:actual_radius+1, -actual_radius:actual_radius+1]
    
    # Add irregularity (ellipticity)
    if np.random.random() < irregularity:
        # Elliptical hole
        angle = np.random.uniform(0, 2 * np.pi)
        aspect = np.random.uniform(0.6, 1.5)
        x_rot = x * np.cos(angle) + y * np.sin(angle)
        y_rot = -x * np.sin(angle) + y * np.cos(angle)
        mask = (x_rot ** 2 / (actual_radius * aspect) ** 2 + 
                y_rot ** 2 / actual_radius ** 2) <= 1
    else:
        # Circular hole
        mask = (x ** 2 + y ** 2) <= actual_radius ** 2
    
    # Add radial cracks (for hard samples)
    if difficulty_level == 'hard' and np.random.random() < 0.5:
        mask = add_radial_cracks(mask, actual_radius)
    
    # Apply hole to image (dark region)
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
            # Make hole darker (thermal physics: cooler regions appear darker)
            img[img_y, img_x] = img[img_y, img_x] * 0.3 + np.random.normal(0, 5)
    
    return img, mask


def add_radial_cracks(mask, radius):
    """Add radial cracks emanating from hole center"""
    h, w = mask.shape
    cy, cx = h // 2, w // 2
    
    num_cracks = np.random.randint(2, 5)
    for _ in range(num_cracks):
        angle = np.random.uniform(0, 2 * np.pi)
        crack_length = np.random.randint(radius, radius * 2)
        for r in range(radius, crack_length):
            cy_r = int(cy + r * np.sin(angle))
            cx_r = int(cx + r * np.cos(angle))
            if 0 <= cy_r < h and 0 <= cx_r < w:
                # Thin crack line
                mask[cy_r, cx_r] = True
                if cy_r + 1 < h:
                    mask[cy_r + 1, cx_r] = True
    
    return mask


def generate_base_thermal_image(difficulty_level):
    """
    Generate base thermal image with realistic IR characteristics.
    
    Simulates:
    - Non-uniform illumination (thermal patterns)
    - Baseline thermal gradient
    - Camera sensor characteristics
    """
    base_intensity = np.random.randint(80, 140)  # Base thermal level
    
    img = np.ones((IMAGE_SIZE, IMAGE_SIZE), dtype=np.float32) * base_intensity
    
    # Add thermal gradient (simulates camera thermal patterns)
    y, x = np.meshgrid(np.arange(IMAGE_SIZE), np.arange(IMAGE_SIZE), indexing='ij')
    
    # Combine multiple gradient directions
    gradient = (0.3 * np.sin(2 * np.pi * x / IMAGE_SIZE) * 20 +
                0.3 * np.cos(2 * np.pi * y / IMAGE_SIZE) * 20 +
                0.2 * np.sin(np.pi * (x + y) / IMAGE_SIZE) * 15)
    
    img += gradient
    
    # Add thermal blobs (natural thermal variance)
    num_blobs = np.random.randint(3, 8)
    for _ in range(num_blobs):
        blob_x = np.random.randint(0, IMAGE_SIZE)
        blob_y = np.random.randint(0, IMAGE_SIZE)
        blob_size = np.random.uniform(20, 60)
        blob_intensity = np.random.uniform(-15, 15)
        
        dy = np.arange(IMAGE_SIZE) - blob_y
        dx = np.arange(IMAGE_SIZE) - blob_x
        xx, yy = np.meshgrid(dx, dy, indexing='ij')
        distance = np.sqrt(xx ** 2 + yy ** 2)
        
        blob_mask = np.exp(-(distance ** 2) / (2 * blob_size ** 2))
        img += blob_mask.T * blob_intensity
    
    return np.clip(img, 0, 255)


def apply_ir_degradations(img, difficulty_level):
    """
    Apply realistic IR sensor degradations.
    
    Simulates:
    - Thermal noise (Gaussian)
    - Salt-and-pepper noise
    - Thermal blur
    - Dead pixels
    - Banding artifacts
    - Sensor drift
    """
    if difficulty_level == 'easy':
        config = IRPhysicsConfig()
        thermal_noise_std = config.THERMAL_NOISE_STD_EASY
        salt_pepper_ratio = config.SALT_PEPPER_EASY
        blur_radius = np.random.uniform(*config.BLUR_RADIUS_EASY)
        dead_pixel_ratio = config.DEAD_PIXEL_EASY
        banding_freq = config.BANDING_FREQ_EASY
    elif difficulty_level == 'medium':
        config = IRPhysicsConfig()
        thermal_noise_std = config.THERMAL_NOISE_STD_MEDIUM
        salt_pepper_ratio = config.SALT_PEPPER_MEDIUM
        blur_radius = np.random.uniform(*config.BLUR_RADIUS_MEDIUM)
        dead_pixel_ratio = config.DEAD_PIXEL_MEDIUM
        banding_freq = config.BANDING_FREQ_MEDIUM
    else:  # hard
        config = IRPhysicsConfig()
        thermal_noise_std = config.THERMAL_NOISE_STD_HARD
        salt_pepper_ratio = config.SALT_PEPPER_HARD
        blur_radius = np.random.uniform(*config.BLUR_RADIUS_HARD)
        dead_pixel_ratio = config.DEAD_PIXEL_HARD
        banding_freq = config.BANDING_FREQ_HARD
    
    img = img.astype(np.float32)
    
    # Thermal (Gaussian) noise
    img += np.random.normal(0, thermal_noise_std, img.shape)
    
    # Salt-and-pepper noise
    num_salt_pepper = int(IMAGE_SIZE * IMAGE_SIZE * salt_pepper_ratio)
    salt_pepper_coords = np.random.choice(IMAGE_SIZE * IMAGE_SIZE, num_salt_pepper, replace=False)
    salt_pepper_y = salt_pepper_coords // IMAGE_SIZE
    salt_pepper_x = salt_pepper_coords % IMAGE_SIZE
    
    for py, px in zip(salt_pepper_y, salt_pepper_x):
        if np.random.random() < 0.5:
            img[py, px] = 255  # Salt
        else:
            img[py, px] = 0  # Pepper
    
    # Thermal blur (sensor resolution limitation)
    kernel_size = int(blur_radius * 2 + 1)
    if kernel_size % 2 == 0:
        kernel_size += 1
    kernel_size = max(3, kernel_size)
    img = cv2.GaussianBlur(img, (kernel_size, kernel_size), blur_radius)
    
    # Dead pixels
    num_dead = int(IMAGE_SIZE * IMAGE_SIZE * dead_pixel_ratio)
    dead_coords = np.random.choice(IMAGE_SIZE * IMAGE_SIZE, num_dead, replace=False)
    dead_y = dead_coords // IMAGE_SIZE
    dead_x = dead_coords % IMAGE_SIZE
    img[dead_y, dead_x] = 0
    
    # Banding noise (sensor row/column artifacts)
    if banding_freq > 0 and np.random.random() < banding_freq:
        num_bands = np.random.randint(2, 8)
        for _ in range(num_bands):
            if np.random.random() < 0.5:
                # Horizontal banding
                band_y = np.random.randint(0, IMAGE_SIZE)
                band_height = np.random.randint(1, 4)
                band_intensity = np.random.uniform(-10, 10)
                img[band_y:band_y+band_height, :] += band_intensity
            else:
                # Vertical banding
                band_x = np.random.randint(0, IMAGE_SIZE)
                band_width = np.random.randint(1, 4)
                band_intensity = np.random.uniform(-10, 10)
                img[:, band_x:band_x+band_width] += band_intensity
    
    # Sensor drift (slow temporal variations - simulated as local intensity shifts)
    if np.random.random() < 0.3:
        drift_x = np.random.uniform(-0.05, 0.05)
        drift_y = np.random.uniform(-0.05, 0.05)
        img_h, img_w = img.shape
        x_shift = int(drift_x * img_w)
        y_shift = int(drift_y * img_h)
        if x_shift != 0 or y_shift != 0:
            img = np.roll(img, y_shift, axis=0)
            img = np.roll(img, x_shift, axis=1)
    
    img = np.clip(img, 0, 255)
    return img


def apply_illumination_variance(img, variance_level):
    """
    Apply uneven illumination patterns.
    
    Simulates:
    - Vignetting
    - Uneven sensor response
    - Thermal gradients
    """
    img = img.astype(np.float32)
    
    # Vignetting effect
    y, x = np.meshgrid(np.arange(IMAGE_SIZE), np.arange(IMAGE_SIZE), indexing='ij')
    center_y, center_x = IMAGE_SIZE / 2, IMAGE_SIZE / 2
    distance_from_center = np.sqrt((x - center_x) ** 2 + (y - center_y) ** 2)
    max_distance = np.sqrt(center_x ** 2 + center_y ** 2)
    
    vignette = 1 - (variance_level * (distance_from_center / max_distance) ** 2)
    vignette = np.clip(vignette, 0.5, 1.0)
    
    img *= vignette
    
    # Random illumination hot spots
    num_spots = int(variance_level * 5)
    for _ in range(num_spots):
        spot_x = np.random.randint(0, IMAGE_SIZE)
        spot_y = np.random.randint(0, IMAGE_SIZE)
        spot_size = np.random.uniform(10, 40)
        spot_intensity = np.random.uniform(-30, 30)
        
        dy = np.arange(IMAGE_SIZE) - spot_y
        dx = np.arange(IMAGE_SIZE) - spot_x
        xx, yy = np.meshgrid(dx, dy, indexing='ij')
        distance = np.sqrt(xx ** 2 + yy ** 2)
        
        spot_mask = np.exp(-(distance ** 2) / (2 * spot_size ** 2))
        img += spot_mask.T * spot_intensity
    
    img = np.clip(img, 0, 255)
    return img


def generate_synthetic_sample(sample_id, label_type):
    """
    Generate a complete synthetic sample.
    
    Returns:
        (image, binary_mask, label, features, metadata)
    """
    # Assign difficulty level
    difficulty_rand = np.random.random()
    if difficulty_rand < EASY_RATIO:
        difficulty_level = 'easy'
    elif difficulty_rand < EASY_RATIO + MEDIUM_RATIO:
        difficulty_level = 'medium'
    else:
        difficulty_level = 'hard'
    
    # Generate base thermal image
    base_img = generate_base_thermal_image(difficulty_level)
    img = base_img.copy()
    full_mask = np.zeros((IMAGE_SIZE, IMAGE_SIZE), dtype=bool)
    
    # ---- Generate holes or artifacts ----
    
    if label_type == 'bullet_hole':
        # Generate 1-3 bullet holes
        num_holes = np.random.choice([1, 2, 3], p=[0.6, 0.3, 0.1])
        
        for _ in range(num_holes):
            # Hole size parameters based on difficulty
            if difficulty_level == 'easy':
                mean_radius = np.random.uniform(15, 25)
                std_radius = 3
                irregularity = 0.2
            elif difficulty_level == 'medium':
                mean_radius = np.random.uniform(8, 20)
                std_radius = 4
                irregularity = 0.4
            else:  # hard
                mean_radius = np.random.uniform(4, 12)
                std_radius = 3
                irregularity = 0.6
            
            # Random position (avoid edges for easier detection)
            margin = int(mean_radius) + 5
            cx = np.random.randint(margin, IMAGE_SIZE - margin)
            cy = np.random.randint(margin, IMAGE_SIZE - margin)
            
            img, hole_mask = generate_bullet_hole(img, (cx, cy), 
                                                 (mean_radius, std_radius, irregularity),
                                                 difficulty_level)
            
            # Merge masks
            cy_start = max(0, cy - int(mean_radius) - 1)
            cy_end = min(IMAGE_SIZE, cy + int(mean_radius) + 2)
            cx_start = max(0, cx - int(mean_radius) - 1)
            cx_end = min(IMAGE_SIZE, cx + int(mean_radius) + 2)
            
            h, w = hole_mask.shape
            full_mask[cy_start:cy_end, cx_start:cx_end] |= hole_mask[
                :cy_end-cy_start, :cx_end-cx_start
            ]
    
    elif label_type == 'non_bullet':
        # Generate non-bullet artifacts (scratches, dust, reflections)
        artifact_type = np.random.choice(['scratch', 'dust', 'reflection'])
        
        if artifact_type == 'scratch':
            # Thin linear scratch
            x1, y1 = np.random.randint(0, IMAGE_SIZE, 2)
            x2, y2 = x1 + np.random.randint(-80, 80), y1 + np.random.randint(-80, 80)
            x2 = np.clip(x2, 0, IMAGE_SIZE - 1)
            y2 = np.clip(y2, 0, IMAGE_SIZE - 1)
            
            cv2.line(img, (x1, y1), (x2, y2), 
                    int(np.random.uniform(40, 80)), thickness=1)
            
            # Create line mask
            rr, cc = np.array([y1, y2]), np.array([x1, x2])
            from scipy.interpolate import interp1d
            if x2 != x1:
                y_interp = np.interp(np.arange(min(x1, x2), max(x1, x2) + 1),
                                    [x1, x2], [y1, y2]).astype(int)
                for x, y in zip(np.arange(min(x1, x2), max(x1, x2) + 1), y_interp):
                    if 0 <= x < IMAGE_SIZE and 0 <= y < IMAGE_SIZE:
                        full_mask[y, x] = True
        
        elif artifact_type == 'dust':
            # Small dust particles
            num_dust = np.random.randint(2, 6)
            for _ in range(num_dust):
                dx = np.random.randint(5, 15)
                dy = np.random.randint(5, 15)
                dox = np.random.randint(0, IMAGE_SIZE - dx)
                doy = np.random.randint(0, IMAGE_SIZE - dy)
                
                # Dust appears as slightly brighter spots
                dust_region = img[doy:doy+dy, dox:dox+dx]
                dust_region = dust_region * 0.7 + np.random.uniform(150, 200)
                img[doy:doy+dy, dox:dox+dx] = np.clip(dust_region, 0, 255)
                
                full_mask[doy:doy+dy, dox:dox+dx] = True
        
        else:  # reflection
            # Bright reflection spots
            num_reflections = np.random.randint(1, 3)
            for _ in range(num_reflections):
                ref_x = np.random.randint(20, IMAGE_SIZE - 20)
                ref_y = np.random.randint(20, IMAGE_SIZE - 20)
                ref_size = np.random.randint(5, 15)
                
                dy = np.arange(IMAGE_SIZE) - ref_y
                dx = np.arange(IMAGE_SIZE) - ref_x
                xx, yy = np.meshgrid(dx, dy, indexing='ij')
                distance = np.sqrt(xx ** 2 + yy ** 2)
                
                ref_mask = np.exp(-(distance ** 2) / (2 * ref_size ** 2))
                img += ref_mask.T * np.random.uniform(50, 100)
                
                full_mask |= (ref_mask.T > 0.1)
    
    else:  # ambiguous
        # Ambiguous cases: very small holes, extreme noise, partial occlusion
        ambiguity_type = np.random.choice(['tiny_hole', 'extreme_noise', 'partial_occlusion'])
        
        if ambiguity_type == 'tiny_hole':
            # Very small hole that's hard to classify
            cx = np.random.randint(20, IMAGE_SIZE - 20)
            cy = np.random.randint(20, IMAGE_SIZE - 20)
            
            mean_radius = np.random.uniform(2, 5)
            std_radius = 1
            irregularity = 0.5
            
            img, hole_mask = generate_bullet_hole(img, (cx, cy),
                                                 (mean_radius, std_radius, irregularity),
                                                 'hard')
            
            cy_start = max(0, cy - int(mean_radius) - 1)
            cy_end = min(IMAGE_SIZE, cy + int(mean_radius) + 2)
            cx_start = max(0, cx - int(mean_radius) - 1)
            cx_end = min(IMAGE_SIZE, cx + int(mean_radius) + 2)
            
            h, w = hole_mask.shape
            full_mask[cy_start:cy_end, cx_start:cx_end] |= hole_mask[
                :cy_end-cy_start, :cx_end-cx_start
            ]
        
        elif ambiguity_type == 'extreme_noise':
            # Normal hole but with extreme noise
            cx = np.random.randint(30, IMAGE_SIZE - 30)
            cy = np.random.randint(30, IMAGE_SIZE - 30)
            
            mean_radius = np.random.uniform(10, 18)
            std_radius = 3
            irregularity = 0.3
            
            img, hole_mask = generate_bullet_hole(img, (cx, cy),
                                                 (mean_radius, std_radius, irregularity),
                                                 'medium')
            
            cy_start = max(0, cy - int(mean_radius) - 1)
            cy_end = min(IMAGE_SIZE, cy + int(mean_radius) + 2)
            cx_start = max(0, cx - int(mean_radius) - 1)
            cx_end = min(IMAGE_SIZE, cx + int(mean_radius) + 2)
            
            h, w = hole_mask.shape
            full_mask[cy_start:cy_end, cx_start:cx_end] |= hole_mask[
                :cy_end-cy_start, :cx_end-cx_start
            ]
        
        else:  # partial occlusion
            # Hole partially obscured by noise or artifacts
            cx = np.random.randint(30, IMAGE_SIZE - 30)
            cy = np.random.randint(30, IMAGE_SIZE - 30)
            
            mean_radius = np.random.uniform(12, 20)
            std_radius = 3
            irregularity = 0.4
            
            img, hole_mask = generate_bullet_hole(img, (cx, cy),
                                                 (mean_radius, std_radius, irregularity),
                                                 'medium')
            
            cy_start = max(0, cy - int(mean_radius) - 1)
            cy_end = min(IMAGE_SIZE, cy + int(mean_radius) + 2)
            cx_start = max(0, cx - int(mean_radius) - 1)
            cx_end = min(IMAGE_SIZE, cx + int(mean_radius) + 2)
            
            h, w = hole_mask.shape
            full_mask[cy_start:cy_end, cx_start:cx_end] |= hole_mask[
                :cy_end-cy_start, :cx_end-cx_start
            ]
    
    # ---- Apply IR degradations ----
    noise_level = {'easy': 0.2, 'medium': 0.5, 'hard': 0.8}[difficulty_level]
    img = apply_ir_degradations(img, difficulty_level)
    
    # ---- Apply illumination variance ----
    illum_variance = {'easy': np.random.uniform(*IRPhysicsConfig.ILLUM_VAR_EASY),
                     'medium': np.random.uniform(*IRPhysicsConfig.ILLUM_VAR_MEDIUM),
                     'hard': np.random.uniform(*IRPhysicsConfig.ILLUM_VAR_HARD)}[difficulty_level]
    img = apply_illumination_variance(img, illum_variance)
    
    # ---- Compute blur level ----
    blur_level = {'easy': np.random.uniform(*IRPhysicsConfig.BLUR_RADIUS_EASY) / 5,
                 'medium': np.random.uniform(*IRPhysicsConfig.BLUR_RADIUS_MEDIUM) / 5,
                 'hard': np.random.uniform(*IRPhysicsConfig.BLUR_RADIUS_HARD) / 5}[difficulty_level]
    
    # ---- Normalize image to 0-255 ----
    img = np.clip(img, 0, 255).astype(np.uint8)
    
    # ---- Compute features ----
    features = compute_features(full_mask, img.astype(np.float32))
    
    # ---- Metadata ----
    metadata = {
        'sample_id': sample_id,
        'label': label_type,
        'difficulty_level': difficulty_level,
        'noise_level': noise_level,
        'blur_level': blur_level,
        'illumination_variance': illum_variance,
    }
    
    return img, label_type, features, metadata


# ============================================================================
# Main Dataset Generation
# ============================================================================

def generate_dataset():
    """Generate complete synthetic dataset"""
    
    # Create directories
    DATASET_DIR.mkdir(exist_ok=True)
    IMAGES_DIR.mkdir(exist_ok=True)
    
    print(f"Generating {DATASET_SIZE} synthetic samples...")
    print(f"Output directory: {DATASET_DIR.absolute()}")
    
    # Determine label distribution
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
        
        # Generate sample
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
    
    # Save metadata to CSV
    df = pd.DataFrame(all_data)
    csv_path = DATASET_DIR / "annotations.csv"
    df.to_csv(csv_path, index=False)
    
    print(f"\n? Dataset generation complete!")
    print(f"  Images: {IMAGES_DIR.absolute()}")
    print(f"  Annotations: {csv_path.absolute()}")
    print(f"\nDataset Statistics:")
    print(f"  Total samples: {len(df)}")
    print(f"\n  Label distribution:")
    print(df['label'].value_counts())
    print(f"\n  Difficulty distribution:")
    print(df['difficulty_level'].value_counts())
    print(f"\n  Feature statistics:")
    print(df[['area', 'perimeter', 'circularity', 'eccentricity', 'aspect_ratio',
             'mean_intensity', 'contrast', 'entropy']].describe())


if __name__ == '__main__':
    generate_dataset()
