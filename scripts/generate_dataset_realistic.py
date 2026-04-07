#!/usr/bin/env python3
"""
PHYSICALLY REALISTIC DATASET GENERATOR
Generates synthetic IR images matching real bullet hole characteristics

Real IR Bullet Hole Physics:
1. Smooth background (Perlin noise base)
2. Dark circular/elliptical hole with radial gradient
3. High-contrast edge ring (thermal boundary)
4. Radial cracks emanating from center
5. Realistic IR sensor noise
"""

import numpy as np
import cv2
import pandas as pd
from pathlib import Path
from scipy import ndimage
from scipy.ndimage import gaussian_filter
import warnings
warnings.filterwarnings('ignore')

# Configuration
DATASET_SIZE = 10000
IMAGE_SIZE = 256
DATASET_DIR = Path("dataset_realistic")
IMAGES_DIR = DATASET_DIR / "images"

BULLET_HOLE_RATIO = 0.50
NON_BULLET_RATIO = 0.30
AMBIGUOUS_RATIO = 0.20

EASY_RATIO = 0.30
MEDIUM_RATIO = 0.50
HARD_RATIO = 0.20

class RealisticIRPhysicsConfig:
    """Realistic IR imaging physics parameters"""
    
    # Intensity ranges (uint8: 0-255)
    BACKGROUND_MEAN_EASY = (120, 160)
    BACKGROUND_MEAN_MEDIUM = (110, 150)
    BACKGROUND_MEAN_HARD = (100, 140)
    
    # Hole intensity (much darker)
    HOLE_CENTER_INTENSITY = (10, 40)
    HOLE_EDGE_INTENSITY = (50, 100)
    
    # Sensor noise (realistic IR noise)
    THERMAL_NOISE_STD_EASY = 1.5
    THERMAL_NOISE_STD_MEDIUM = 2.5
    THERMAL_NOISE_STD_HARD = 4.0
    
    # Radial gradient smoothness
    HOLE_GRADIENT_RADIUS_EASY = (15, 25)
    HOLE_GRADIENT_RADIUS_MEDIUM = (8, 18)
    HOLE_GRADIENT_RADIUS_HARD = (4, 12)
    
    # Background smoothness (Perlin noise scale)
    BG_SMOOTHNESS_EASY = 40
    BG_SMOOTHNESS_MEDIUM = 30
    BG_SMOOTHNESS_HARD = 20

def perlin_noise_2d(size, scale=50, octaves=2):
    """
    Generate smooth Perlin-like noise using multiple octaves
    Simulates smooth thermal background variations
    """
    noise = np.zeros((size, size))
    
    for octave in range(octaves):
        freq = 2 ** octave
        amplitude = 1.0 / freq
        
        # Generate random gradient field at frequency
        grad_size = max(2, size // (scale // freq))
        gradients = np.random.randn(grad_size, grad_size)
        
        # Interpolate to image size
        from scipy.interpolate import RectBivariateSpline
        y = np.linspace(0, grad_size - 1, size)
        x = np.linspace(0, grad_size - 1, size)
        
        try:
            f = RectBivariateSpline(np.arange(grad_size), np.arange(grad_size), 
                                   gradients, kx=1, ky=1)
            octave_noise = f(y, x, grid=False)
        except:
            # Fallback if spline fails
            octave_noise = cv2.resize(gradients, (size, size), 
                                     interpolation=cv2.INTER_LINEAR)
        
        noise += octave_noise * amplitude
    
    # Normalize to [-1, 1]
    noise = noise / (octaves * 0.75)
    noise = np.clip(noise, -1, 1)
    
    return noise

def generate_smooth_background(size, background_mean, smoothness_level, difficulty):
    """
    Generate smooth thermal background using low-frequency noise
    Resembles real thermal sensor output
    """
    # Start with base intensity
    img = np.ones((size, size), dtype=np.float32) * background_mean
    
    # Add smooth variations (Perlin-like noise)
    perlin = perlin_noise_2d(size, scale=smoothness_level, octaves=2)
    thermal_variation = perlin * 15  # Small amplitude
    img += thermal_variation
    
    # Add subtle vignetting (realistic sensor effect)
    y, x = np.meshgrid(np.arange(size), np.arange(size), indexing='ij')
    center_y, center_x = size / 2, size / 2
    distance = np.sqrt((x - center_x)**2 + (y - center_y)**2)
    max_distance = np.sqrt(center_x**2 + center_y**2)
    vignette = 1.0 - 0.08 * (distance / max_distance)**2
    img *= vignette
    
    return np.clip(img, 0, 255)

def generate_realistic_bullet_hole(img, center, hole_params, difficulty):
    """
    Generate realistic bullet hole with:
    1. Dark circular center
    2. Radial gradient outward
    3. High-contrast edge ring
    4. Radial cracks
    """
    img_float = img.astype(np.float32)
    center_x, center_y = center
    
    # Extract parameters
    hole_radius, hole_irregularity, impact_angle = hole_params
    
    # Create radial gradient mask
    y, x = np.meshgrid(np.arange(img.shape[0]), np.arange(img.shape[1]), indexing='ij')
    
    # Elliptical distortion from impact angle
    angle_rad = np.radians(impact_angle)
    x_rot = (x - center_x) * np.cos(angle_rad) + (y - center_y) * np.sin(angle_rad)
    y_rot = -(x - center_x) * np.sin(angle_rad) + (y - center_y) * np.cos(angle_rad)
    
    # Aspect ratio from impact angle
    aspect_ratio = 1.0 + 0.3 * abs(np.sin(angle_rad))
    
    # Distance from center (accounting for ellipticity)
    distance = np.sqrt((x_rot / aspect_ratio)**2 + y_rot**2)
    
    # Create the hole as a radial gradient
    hole_center_intensity = np.random.uniform(15, 35)
    hole_edge_intensity = np.random.uniform(80, 120)
    
    # Gradient function: dark at center, brighter outward
    gradient = np.clip(distance / (hole_radius + 1e-6), 0, 1)
    hole_intensity = hole_center_intensity + (hole_edge_intensity - hole_center_intensity) * gradient**1.5
    
    # Create mask for hole region (distance < hole_radius)
    hole_mask = distance < hole_radius
    
    # Apply hole (replace with gradient)
    img_float[hole_mask] = hole_intensity[hole_mask]
    
    # Add edge ring (high contrast boundary)
    edge_ring = (distance > hole_radius * 0.95) & (distance < hole_radius * 1.15)
    edge_intensity = np.random.uniform(180, 220)
    img_float[edge_ring] = edge_intensity
    
    # Add radial cracks (fractures from impact)
    num_cracks = np.random.randint(2, 6) if difficulty in ['medium', 'hard'] else np.random.randint(0, 3)
    
    for _ in range(num_cracks):
        crack_angle = np.random.uniform(0, 2 * np.pi)
        crack_length = hole_radius * np.random.uniform(1.5, 3.0)
        crack_width = np.random.uniform(0.5, 2.0)
        
        # Draw crack as a thin line
        for r in np.linspace(hole_radius, crack_length, int(crack_length)):
            crack_x = int(center_x + r * np.cos(crack_angle))
            crack_y = int(center_y + r * np.sin(crack_angle))
            
            if 0 <= crack_x < img.shape[1] and 0 <= crack_y < img.shape[0]:
                # Thin dark line for crack
                rr, cc = np.ogrid[max(0, crack_y-1):min(img.shape[0], crack_y+2),
                                 max(0, crack_x-1):min(img.shape[1], crack_x+2)]
                crack_intensity = np.random.uniform(20, 60)
                img_float[rr, cc] = np.minimum(img_float[rr, cc], crack_intensity)
    
    return np.clip(img_float, 0, 255), hole_mask

def add_realistic_ir_noise(img, difficulty):
    """
    Add realistic IR sensor noise (NOT dominant)
    """
    if difficulty == 'easy':
        noise_std = 1.5
        salt_pepper_ratio = 0.0001
    elif difficulty == 'medium':
        noise_std = 2.5
        salt_pepper_ratio = 0.0003
    else:  # hard
        noise_std = 4.0
        salt_pepper_ratio = 0.0008
    
    img_float = img.astype(np.float32)
    
    # Gaussian thermal noise
    thermal_noise = np.random.normal(0, noise_std, img.shape)
    img_float += thermal_noise
    
    # Minimal salt-pepper noise (sensor defects)
    if salt_pepper_ratio > 0:
        num_salt_pepper = int(img.size * salt_pepper_ratio)
        coords = np.random.choice(img.size, num_salt_pepper, replace=False)
        coords_y = coords // img.shape[1]
        coords_x = coords % img.shape[1]
        
        for py, px in zip(coords_y, coords_x):
            img_float[py, px] = 255 if np.random.random() < 0.5 else 0
    
    return np.clip(img_float, 0, 255)

def compute_features(binary_mask, intensity_image):
    """Compute 17 features from binary mask and intensity image"""
    features = {}
    
    labeled_mask, num_features = ndimage.label(binary_mask)
    
    if num_features == 0:
        return {k: 0 for k in ['area', 'perimeter', 'circularity', 'eccentricity',
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
        circularity = np.clip(circularity, 0, 1)
    else:
        circularity = 0
    features['circularity'] = float(circularity)
    
    contours, _ = cv2.findContours(largest_mask.astype(np.uint8), 
                                    cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    if len(contours) > 0 and len(contours[0]) >= 5:
        cnt = max(contours, key=cv2.contourArea)
        ellipse = cv2.fitEllipse(cnt)
        (cx, cy), (major, minor), angle = ellipse
        aspect_ratio = max(major, minor) / (min(major, minor) + 1e-6)
        eccentricity = np.sqrt(1 - (min(major, minor) / (max(major, minor) + 1e-6)) ** 2)
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

def generate_realistic_sample(sample_id, label_type, difficulty):
    """
    Generate a realistic IR bullet hole sample
    """
    config = RealisticIRPhysicsConfig()
    
    # Select parameters based on difficulty
    if difficulty == 'easy':
        bg_mean = np.random.uniform(*config.BACKGROUND_MEAN_EASY)
        smoothness = config.BG_SMOOTHNESS_EASY
        hole_radius_range = config.HOLE_GRADIENT_RADIUS_EASY
    elif difficulty == 'medium':
        bg_mean = np.random.uniform(*config.BACKGROUND_MEAN_MEDIUM)
        smoothness = config.BG_SMOOTHNESS_MEDIUM
        hole_radius_range = config.HOLE_GRADIENT_RADIUS_MEDIUM
    else:  # hard
        bg_mean = np.random.uniform(*config.BACKGROUND_MEAN_HARD)
        smoothness = config.BG_SMOOTHNESS_HARD
        hole_radius_range = config.HOLE_GRADIENT_RADIUS_HARD
    
    # Generate smooth background
    img = generate_smooth_background(IMAGE_SIZE, bg_mean, smoothness, difficulty)
    full_mask = np.zeros((IMAGE_SIZE, IMAGE_SIZE), dtype=bool)
    
    if label_type == 'bullet_hole':
        # Generate 1-2 bullet holes
        num_holes = np.random.choice([1, 2], p=[0.8, 0.2])
        
        for _ in range(num_holes):
            center_x = np.random.randint(40, IMAGE_SIZE - 40)
            center_y = np.random.randint(40, IMAGE_SIZE - 40)
            hole_radius = np.random.uniform(*hole_radius_range)
            hole_irregularity = np.random.uniform(0.1, 0.4)
            impact_angle = np.random.uniform(0, 360)
            
            img, hole_mask = generate_realistic_bullet_hole(
                img, (center_x, center_y),
                (hole_radius, hole_irregularity, impact_angle),
                difficulty
            )
            full_mask |= hole_mask
    
    elif label_type == 'non_bullet':
        # Small artifacts (dust, scratches)
        artifact_type = np.random.choice(['dust', 'scratch'])
        
        if artifact_type == 'dust':
            num_dust = np.random.randint(1, 2)
            for _ in range(num_dust):
                dust_x = np.random.randint(30, IMAGE_SIZE - 30)
                dust_y = np.random.randint(30, IMAGE_SIZE - 30)
                dust_size = np.random.randint(3, 8)
                
                y, x = np.ogrid[max(0, dust_y-dust_size):min(IMAGE_SIZE, dust_y+dust_size),
                               max(0, dust_x-dust_size):min(IMAGE_SIZE, dust_x+dust_size)]
                dust_intensity = np.random.uniform(40, 80)
                img[y, x] = np.minimum(img[y, x], dust_intensity)
                full_mask[y, x] = True
        else:  # scratch
            x1 = np.random.randint(20, IMAGE_SIZE - 20)
            y1 = np.random.randint(20, IMAGE_SIZE - 20)
            x2 = x1 + np.random.randint(-60, 60)
            y2 = y1 + np.random.randint(-60, 60)
            x2 = np.clip(x2, 0, IMAGE_SIZE - 1)
            y2 = np.clip(y2, 0, IMAGE_SIZE - 1)
            
            cv2.line(img, (x1, y1), (x2, y2), 60, thickness=1)
    
    else:  # ambiguous
        # Tiny hole or borderline feature
        if np.random.random() < 0.6:
            center_x = np.random.randint(50, IMAGE_SIZE - 50)
            center_y = np.random.randint(50, IMAGE_SIZE - 50)
            hole_radius = np.random.uniform(2, 6)
            hole_irregularity = 0.5
            impact_angle = np.random.uniform(0, 360)
            
            img, hole_mask = generate_realistic_bullet_hole(
                img, (center_x, center_y),
                (hole_radius, hole_irregularity, impact_angle),
                'hard'
            )
            full_mask |= hole_mask
    
    # Add realistic IR noise
    img = add_realistic_ir_noise(img, difficulty)
    
    # Final normalization
    img = np.clip(img, 0, 255).astype(np.uint8)
    
    # Compute features
    features = compute_features(full_mask, img.astype(np.float32))
    
    metadata = {
        'sample_id': sample_id,
        'label': label_type,
        'difficulty_level': difficulty,
        'noise_level': 0.3 if difficulty == 'easy' else (0.5 if difficulty == 'medium' else 0.8),
        'blur_level': 0.2 if difficulty == 'easy' else (0.4 if difficulty == 'medium' else 0.6),
        'illumination_variance': 0.1 if difficulty == 'easy' else (0.25 if difficulty == 'medium' else 0.4),
    }
    
    return img, label_type, features, metadata

def generate_realistic_dataset():
    """Generate complete realistic dataset"""
    
    DATASET_DIR.mkdir(exist_ok=True)
    IMAGES_DIR.mkdir(exist_ok=True)
    
    print(f"Generating REALISTIC IR bullet hole dataset: {DATASET_SIZE} samples")
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
        if (sample_id + 1) % 1000 == 0:
            print(f"  Progress: {sample_id + 1} / {DATASET_SIZE}")
        
        # Assign difficulty
        diff_rand = np.random.random()
        if diff_rand < EASY_RATIO:
            difficulty = 'easy'
        elif diff_rand < EASY_RATIO + MEDIUM_RATIO:
            difficulty = 'medium'
        else:
            difficulty = 'hard'
        
        label = labels[sample_id]
        img, label_out, features, metadata = generate_realistic_sample(sample_id, label, difficulty)
        
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
            'noise_level': metadata['noise_level'],
            'blur_level': metadata['blur_level'],
            'illumination_variance': metadata['illumination_variance'],
        }
        
        all_data.append(row)
    
    df = pd.DataFrame(all_data)
    csv_path = DATASET_DIR / "annotations.csv"
    df.to_csv(csv_path, index=False)
    
    print(f"\nOK: Realistic dataset generation complete!")
    print(f"  Images: {IMAGES_DIR.absolute()}")
    print(f"  Annotations: {csv_path.absolute()}")

if __name__ == '__main__':
    generate_realistic_dataset()
