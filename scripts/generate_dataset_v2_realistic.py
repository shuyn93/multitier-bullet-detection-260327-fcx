"""
Synthetic Dataset Generator for Bullet Hole Detection System - V2 REALISTIC

RE-DESIGNED to generate physically plausible IR bullet hole images:

Key improvements:
1. Smooth background using Perlin-like noise (low-frequency)
2. Realistic bullet hole with dark center + radial gradient
3. Thermal contrast ring at edges
4. Radial crack simulation
5. Proper intensity calibration (uint8 [0-255])
6. Quality control with visualization

Characteristics:
- Background: 100-180 (smooth gray)
- Hole center: 10-50 (dark)
- Edge ring: 180-220 (bright thermal gradient)
- Noise: Gaussian only (sensor noise)

Output:
- /dataset_realistic/images/{sample_id}.png
- /dataset_realistic/annotations.csv
"""

import numpy as np
import cv2
import pandas as pd
from pathlib import Path
from scipy import ndimage
from scipy.ndimage import gaussian_filter
from skimage import measure
import warnings
warnings.filterwarnings('ignore')

# ============================================================================
# Configuration
# ============================================================================

DATASET_SIZE = 10000
IMAGE_SIZE = 256
DATASET_DIR = Path("dataset_realistic")
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
# Perlin-like Noise Generation (Low-Frequency Background)
# ============================================================================

def generate_perlin_noise(shape, scale=50):
    """
    Generate Perlin-like noise using interpolated random gradients.
    
    Args:
        shape: (height, width)
        scale: Grid scale (larger = smoother, lower frequency)
    
    Returns:
        Smooth noise array in [0, 1]
    """
    h, w = shape
    
    # Create coarse random grid
    grid_h = (h // scale) + 2
    grid_w = (w // scale) + 2
    
    # Random gradients
    gradients = np.random.uniform(-1, 1, (grid_h, grid_w))
    
    # Interpolate to full resolution
    x = np.linspace(0, grid_w - 1, w)
    y = np.linspace(0, grid_h - 1, h)
    
    # Use scipy interpolation
    from scipy.interpolate import RegularGridInterpolator
    
    points = (np.arange(grid_h), np.arange(grid_w))
    interp_func = RegularGridInterpolator(points, gradients, bounds_error=False, 
                                         fill_value=0, method='cubic')
    
    yy, xx = np.meshgrid(y, x, indexing='ij')
    coords = np.stack([yy, xx], axis=-1)
    
    noise = interp_func(coords)
    
    # Normalize to [0, 1]
    noise = (noise - noise.min()) / (noise.max() - noise.min() + 1e-6)
    
    return noise


def generate_smooth_background(difficulty_level):
    """
    Generate smooth, realistic IR background.
    
    Based on physical IR imaging:
    - Thermal uniformity with small variations
    - Low-frequency content (smooth gradients)
    - Sensor noise only (added later)
    
    Intensity range: 100-180 (uint8)
    """
    
    # Base thermal level
    base_intensity = np.random.randint(120, 160)
    
    # Create base
    background = np.ones((IMAGE_SIZE, IMAGE_SIZE), dtype=np.float32) * base_intensity
    
    # Add low-frequency thermal gradients (smooth variations)
    
    # Gradient 1: Diagonal thermal drift
    gradient_scale = np.random.uniform(0.05, 0.15)
    y, x = np.meshgrid(np.arange(IMAGE_SIZE), np.arange(IMAGE_SIZE), indexing='ij')
    
    thermal_gradient = gradient_scale * 30 * (
        0.5 * np.sin(2 * np.pi * x / IMAGE_SIZE) + 
        0.3 * np.cos(2 * np.pi * y / IMAGE_SIZE) +
        0.2 * np.sin(np.pi * (x + y) / (2 * IMAGE_SIZE))
    )
    
    background += thermal_gradient
    
    # Add smooth blobs using Perlin-like noise
    num_blobs = np.random.randint(2, 4)
    blob_noise = generate_perlin_noise((IMAGE_SIZE, IMAGE_SIZE), scale=60)
    
    for _ in range(num_blobs):
        blob_intensity = np.random.uniform(-8, 8)
        background += blob_noise * blob_intensity
    
    # Slight vignetting (natural for thermal cameras)
    cy, cx = IMAGE_SIZE / 2, IMAGE_SIZE / 2
    distance = np.sqrt((x - cy) ** 2 + (y - cx) ** 2)
    max_distance = np.sqrt(cy ** 2 + cx ** 2)
    vignette = 1.0 - 0.05 * (distance / max_distance) ** 2
    background *= vignette
    
    # Clip to reasonable range
    background = np.clip(background, 100, 180)
    
    return background


def generate_bullet_hole_physics(center, radius, background_intensity, 
                                  material='metal', impact_angle=0):
    """
    Generate physically-realistic bullet hole using thermal physics.
    
    Simulates:
    1. Dark central depression (thermal mass cooling)
    2. Radial gradient (smooth transition)
    3. Edge thermal ring (material deformation heating)
    4. Material-dependent characteristics
    
    Args:
        center: (cy, cx) hole center
        radius: Hole radius (pixels)
        background_intensity: Background thermal level
        material: 'metal', 'wood', 'fabric'
        impact_angle: Impact angle in radians (0 = perpendicular)
    
    Returns:
        hole_image (IMAGE_SIZE x IMAGE_SIZE), hole_mask
    """
    
    hole_image = np.zeros((IMAGE_SIZE, IMAGE_SIZE), dtype=np.float32)
    hole_mask = np.zeros((IMAGE_SIZE, IMAGE_SIZE), dtype=bool)
    
    cy, cx = int(center[0]), int(center[1])
    
    # Create coordinate grids
    y, x = np.meshgrid(np.arange(IMAGE_SIZE), np.arange(IMAGE_SIZE), indexing='ij')
    
    # Distance from hole center
    dx = x - cx
    dy = y - cy
    distance = np.sqrt(dx ** 2 + dy ** 2)
    
    # Apply elliptical distortion if angled impact
    if impact_angle != 0:
        aspect_ratio = 1.0 / (1.0 + 0.3 * np.sin(impact_angle))
        distance_ellipse = np.sqrt((dx ** 2) / (aspect_ratio ** 2) + dy ** 2)
    else:
        distance_ellipse = distance
    
    # ---- HOLE CENTER (Dark region) ----
    hole_mask = distance <= radius
    
    # Central depression intensity (cooler than background)
    if material == 'metal':
        # Sharp, cold depression
        hole_center_intensity = np.random.uniform(15, 40)
        falloff_rate = 3.0
    elif material == 'wood':
        # Rougher, less sharp
        hole_center_intensity = np.random.uniform(35, 60)
        falloff_rate = 2.0
    else:  # fabric
        # Very blurred hole
        hole_center_intensity = np.random.uniform(50, 80)
        falloff_rate = 1.0
    
    # Radial gradient from center (dark) to edge (less dark)
    normalized_distance = np.clip(distance_ellipse / radius, 0, 1)
    
    # Smooth falloff using exponential + polynomial blend
    gradient_inner = np.exp(-falloff_rate * (1 - normalized_distance) ** 2)
    gradient_intensity = hole_center_intensity + gradient_inner * (background_intensity - hole_center_intensity)
    
    hole_image[hole_mask] = gradient_intensity[hole_mask]
    
    # ---- EDGE RING (Thermal contrast) ----
    edge_width = 2 + np.random.randint(0, 3)
    edge_mask = (distance > radius) & (distance <= radius + edge_width)
    
    # Edge is heated (bright in IR thermal)
    if material == 'metal':
        edge_intensity = np.random.uniform(190, 220)
    elif material == 'wood':
        edge_intensity = np.random.uniform(170, 200)
    else:  # fabric
        edge_intensity = np.random.uniform(150, 180)
    
    # Smooth edge using Gaussian falloff
    edge_distance = (distance - radius) / (edge_width + 1)
    edge_distance = np.clip(edge_distance, 0, 1)
    edge_falloff = np.exp(-2 * edge_distance ** 2)
    
    hole_image[edge_mask] = edge_intensity * edge_falloff[edge_mask]
    
    # Add slight irregularity to hole boundary
    irregularity_scale = np.random.uniform(0.1, 0.3)
    irregularity = generate_perlin_noise((IMAGE_SIZE, IMAGE_SIZE), scale=40)
    irregularity = (irregularity - 0.5) * irregularity_scale * radius
    
    boundary_mask = (distance > radius - 1) & (distance < radius + edge_width + 2)
    hole_image[boundary_mask] *= (1 + irregularity[boundary_mask] * 0.05)
    
    # ---- RADIAL CRACKS ----
    num_cracks = np.random.randint(2, 6)
    
    for _ in range(num_cracks):
        crack_angle = np.random.uniform(0, 2 * np.pi)
        crack_length = radius + np.random.randint(5, 20)
        crack_width = 1
        
        # Crack as radial line
        crack_angles = np.arctan2(dy, dx)
        angle_diff = np.abs(crack_angles - crack_angle)
        angle_diff = np.minimum(angle_diff, 2 * np.pi - angle_diff)
        
        # Thin radial line mask
        crack_mask = (angle_diff < 0.1) & (distance >= radius * 0.7) & (distance <= crack_length)
        
        # Crack intensity (darker than background)
        crack_intensity = background_intensity * np.random.uniform(0.6, 0.8)
        hole_image[crack_mask] = crack_intensity
    
    # Smooth everything slightly
    hole_image = gaussian_filter(hole_image, sigma=0.5)
    
    return hole_image, hole_mask


def add_sensor_noise(image, difficulty_level):
    """
    Add realistic IR sensor noise.
    
    Types:
    - Gaussian thermal noise (main)
    - Occasional hot/cold pixels
    - Minimal banding
    """
    
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
    
    # Gaussian thermal noise
    thermal_noise = np.random.normal(0, thermal_std, image.shape)
    noisy_image += thermal_noise
    
    # Hot/cold pixels
    num_anomalies = int(IMAGE_SIZE * IMAGE_SIZE * hot_pixel_ratio)
    anomaly_coords = np.random.choice(IMAGE_SIZE * IMAGE_SIZE, num_anomalies, replace=False)
    anomaly_y = anomaly_coords // IMAGE_SIZE
    anomaly_x = anomaly_coords % IMAGE_SIZE
    
    for ay, ax in zip(anomaly_y, anomaly_x):
        if np.random.random() < 0.5:
            noisy_image[ay, ax] = 255  # Hot pixel
        else:
            noisy_image[ay, ax] = 0    # Cold pixel (dead)
    
    # Occasional banding artifacts
    if np.random.random() < banding_prob:
        if np.random.random() < 0.5:
            # Horizontal band
            band_y = np.random.randint(0, IMAGE_SIZE)
            band_height = np.random.randint(1, 3)
            band_intensity = np.random.uniform(-3, 3)
            noisy_image[band_y:band_y+band_height, :] += band_intensity
        else:
            # Vertical band
            band_x = np.random.randint(0, IMAGE_SIZE)
            band_width = np.random.randint(1, 3)
            band_intensity = np.random.uniform(-3, 3)
            noisy_image[:, band_x:band_x+band_width] += band_intensity
    
    return noisy_image


# ============================================================================
# Feature Computation
# ============================================================================

def compute_features(binary_mask, intensity_image):
    """
    Compute 17 features from binary mask and intensity image.
    """
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
    
    # Geometry
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
    
    # Intensity
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
    
    # Texture
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
    
    # Shape
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
    
    fractal_dim = estimate_fractal_dimension(largest_mask)
    features['fractal_dimension'] = float(fractal_dim)
    
    if area > 0 and features['mean_intensity'] < 128:
        depth_estimate = (128 - features['mean_intensity']) / 128.0
    else:
        depth_estimate = 0.0
    features['hole_depth_estimate'] = float(np.clip(depth_estimate, 0, 1))
    
    return features


def estimate_fractal_dimension(binary_mask, min_size=2, max_size=64):
    """Estimate fractal dimension using box-counting"""
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
        log_sizes = np.log(sizes)
        log_counts = np.log(counts)
        coeffs = np.polyfit(log_sizes, log_counts, 1)
        fractal_dim = -coeffs[0]
    else:
        fractal_dim = 2.0
    
    return np.clip(fractal_dim, 1.0, 3.0)


# ============================================================================
# Non-Bullet Generation
# ============================================================================

def generate_non_bullet_artifact(background):
    """Generate non-bullet artifacts: scratches, dust, reflections"""
    
    img = background.copy()
    mask = np.zeros((IMAGE_SIZE, IMAGE_SIZE), dtype=bool)
    
    artifact_type = np.random.choice(['scratch', 'dust', 'reflection'])
    
    if artifact_type == 'scratch':
        # Thin linear scratch
        x1, y1 = np.random.randint(10, IMAGE_SIZE - 10, 2)
        x2 = x1 + np.random.randint(-80, 80)
        y2 = y1 + np.random.randint(-80, 80)
        x2 = np.clip(x2, 0, IMAGE_SIZE - 1)
        y2 = np.clip(y2, 0, IMAGE_SIZE - 1)
        
        # Draw bright scratch (lower intensity)
        scratch_intensity = background[y1, x1] * 0.6
        cv2.line(img, (x1, y1), (x2, y2), int(scratch_intensity), thickness=1)
        
        # Create line mask
        if x2 != x1:
            y_interp = np.interp(np.arange(min(x1, x2), max(x1, x2) + 1),
                                [x1, x2], [y1, y2]).astype(int)
            for x, y in zip(np.arange(min(x1, x2), max(x1, x2) + 1), y_interp):
                if 0 <= x < IMAGE_SIZE and 0 <= y < IMAGE_SIZE:
                    mask[y, x] = True
    
    elif artifact_type == 'dust':
        # Small dust particles (bright spots)
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
        # Bright reflection spot
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


# ============================================================================
# Sample Generation
# ============================================================================

def generate_synthetic_sample(sample_id, label_type):
    """Generate a complete synthetic sample with realistic IR physics"""
    
    # Difficulty
    difficulty_rand = np.random.random()
    if difficulty_rand < EASY_RATIO:
        difficulty_level = 'easy'
    elif difficulty_rand < EASY_RATIO + MEDIUM_RATIO:
        difficulty_level = 'medium'
    else:
        difficulty_level = 'hard'
    
    # Generate smooth background
    background = generate_smooth_background(difficulty_level)
    
    img = background.copy()
    full_mask = np.zeros((IMAGE_SIZE, IMAGE_SIZE), dtype=bool)
    
    # ---- Generate content based on label ----
    
    if label_type == 'bullet_hole':
        # Generate 1-3 bullet holes
        num_holes = np.random.choice([1, 2, 3], p=[0.7, 0.25, 0.05])
        
        for _ in range(num_holes):
            # Hole size based on difficulty
            if difficulty_level == 'easy':
                radius = np.random.randint(15, 25)
                material = np.random.choice(['metal', 'wood'], p=[0.7, 0.3])
            elif difficulty_level == 'medium':
                radius = np.random.randint(10, 18)
                material = np.random.choice(['metal', 'wood', 'fabric'], p=[0.5, 0.3, 0.2])
            else:  # hard
                radius = np.random.randint(5, 12)
                material = np.random.choice(['metal', 'wood', 'fabric'], p=[0.3, 0.4, 0.3])
            
            # Position
            margin = radius + 5
            cy = np.random.randint(margin, IMAGE_SIZE - margin)
            cx = np.random.randint(margin, IMAGE_SIZE - margin)
            
            # Impact angle (perpendicular = 0, up to 30 degrees)
            impact_angle = np.random.uniform(0, np.pi / 6) if np.random.random() < 0.3 else 0
            
            # Generate hole
            hole_img, hole_mask = generate_bullet_hole_physics(
                (cy, cx), radius, background[cy, cx],
                material=material, impact_angle=impact_angle
            )
            
            # Blend onto image
            img += hole_img
            full_mask |= hole_mask
    
    elif label_type == 'non_bullet':
        img, artifact_mask = generate_non_bullet_artifact(background)
        full_mask = artifact_mask
    
    else:  # ambiguous
        ambiguity_type = np.random.choice(['tiny_hole', 'blurry_hole', 'artifact_confusion'])
        
        if ambiguity_type == 'tiny_hole':
            # Very small, hard-to-detect hole
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
            # Normal hole but heavily blurred (detection ambiguity)
            radius = np.random.randint(12, 18)
            cy = np.random.randint(30, IMAGE_SIZE - 30)
            cx = np.random.randint(30, IMAGE_SIZE - 30)
            
            hole_img, hole_mask = generate_bullet_hole_physics(
                (cy, cx), radius, background[cy, cx],
                material='fabric', impact_angle=0
            )
            
            # Heavy blur
            hole_img = gaussian_filter(hole_img, sigma=3.0)
            img += hole_img
            full_mask = hole_mask
        
        else:  # artifact confusion
            # Artifact that might look like hole
            img, artifact_mask = generate_non_bullet_artifact(background)
            full_mask = artifact_mask
    
    # ---- Apply sensor noise ----
    img = add_sensor_noise(img, difficulty_level)
    
    # ---- Normalize to uint8 ----
    img = np.clip(img, 0, 255).astype(np.uint8)
    
    # ---- Compute features ----
    features = compute_features(full_mask, img.astype(np.float32))
    
    # ---- Metadata ----
    metadata = {
        'sample_id': sample_id,
        'label': label_type,
        'difficulty_level': difficulty_level,
    }
    
    return img, label_type, features, metadata


# ============================================================================
# Main Dataset Generation
# ============================================================================

def generate_dataset():
    """Generate complete realistic synthetic dataset"""
    
    # Create directories
    DATASET_DIR.mkdir(exist_ok=True)
    IMAGES_DIR.mkdir(exist_ok=True)
    
    print("=" * 70)
    print("REALISTIC IR BULLET HOLE DATASET GENERATION - V2")
    print("=" * 70)
    print(f"\nGenerating {DATASET_SIZE} physically-plausible synthetic samples...")
    print(f"Output directory: {DATASET_DIR.absolute()}\n")
    
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
            print(f"  Progress: {sample_id + 1:5d} / {DATASET_SIZE}")
        
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
        }
        
        all_data.append(row)
    
    # Save metadata to CSV
    df = pd.DataFrame(all_data)
    csv_path = DATASET_DIR / "annotations.csv"
    df.to_csv(csv_path, index=False)
    
    print(f"\n? Dataset generation complete!")
    print(f"  Images: {IMAGES_DIR.absolute()}")
    print(f"  Annotations: {csv_path.absolute()}")
    
    print(f"\n" + "=" * 70)
    print("DATASET STATISTICS")
    print("=" * 70)
    
    print(f"\nTotal samples: {len(df)}")
    print(f"\nLabel distribution:")
    print(df['label'].value_counts())
    print(f"\nDifficulty distribution:")
    print(df['difficulty_level'].value_counts())
    
    print(f"\nFeature statistics:")
    print(df[['area', 'perimeter', 'circularity', 'eccentricity', 'aspect_ratio',
             'mean_intensity', 'contrast', 'entropy']].describe())
    
    print(f"\nIntensity calibration:")
    print(f"  Background range: {df['mean_intensity'].quantile(0.25):.1f} - {df['mean_intensity'].quantile(0.75):.1f}")
    print(f"  Hole center (min_intensity): {df['min_intensity'].mean():.1f} ± {df['min_intensity'].std():.1f}")
    print(f"  Contrast: {df['contrast'].mean():.1f} ± {df['contrast'].std():.1f}")
    
    return df


if __name__ == '__main__':
    df = generate_dataset()
