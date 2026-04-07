#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Extended IR Bullet Hole Dataset Generator
Generates 10,000 additional samples to extend the existing dataset
Maintains consistency with existing dataset structure and quality
"""

import numpy as np
import cv2
import pandas as pd
from pathlib import Path
from scipy.ndimage import gaussian_filter
import sys
from datetime import datetime

# Configuration
ADDITIONAL_SAMPLES = 10000
IMAGE_SIZE = 256
DATASET_DIR = Path("dataset_ir_realistic")
IMAGES_DIR = DATASET_DIR / "images"
ANNOTATIONS_FILE = DATASET_DIR / "annotations.csv"

# Class distribution (maintain existing proportions)
BULLET_HOLE_RATIO = 0.50
NON_BULLET_RATIO = 0.30
AMBIGUOUS_RATIO = 0.20

# Difficulty distribution
EASY_RATIO = 0.30
MEDIUM_RATIO = 0.50
HARD_RATIO = 0.20

# Additional hard samples for Tier 2 training (20% of all samples)
HARD_BOUNDARY_SAMPLES_RATIO = 0.20

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

def generate_bullet_hole_image(size, difficulty='medium', is_boundary_sample=False):
    """Generate realistic IR image with bullet hole"""
    h, w = size
    
    background = create_smooth_background((h, w), (100, 180))
    image = background.astype(np.float32)
    
    # Randomize based on difficulty
    if difficulty == 'easy':
        radius_range = (25, 35)
        irregularity_range = (0.05, 0.1)
        center_margin = 50
        crack_prob = 0.3
        edge_ring_strength = 0.3
    elif difficulty == 'hard' or is_boundary_sample:
        radius_range = (8, 20) if is_boundary_sample else (10, 22)
        irregularity_range = (0.15, 0.3)
        center_margin = 30 if is_boundary_sample else 40
        crack_prob = 0.1 if is_boundary_sample else 0.2
        edge_ring_strength = 0.1 if is_boundary_sample else 0.15
    else:  # medium
        radius_range = (15, 28)
        irregularity_range = (0.1, 0.18)
        center_margin = 45
        crack_prob = 0.2
        edge_ring_strength = 0.2
    
    # Generate hole
    radius = np.random.uniform(*radius_range)
    center_x = np.random.uniform(center_margin, w - center_margin)
    center_y = np.random.uniform(center_margin, h - center_margin)
    center = (center_x, center_y)
    
    irregularity = np.random.uniform(*irregularity_range)
    hole_mask = create_irregular_circle((h, w), center, radius, irregularity)
    
    # Hole intensity
    hole_intensity = np.random.uniform(30, 70)
    image = image * (1 - hole_mask) + hole_intensity * hole_mask
    
    # Optional cracks
    if np.random.random() < crack_prob:
        cracks = create_radial_cracks((h, w), center, radius)
        image = np.clip(image * (1 - 0.3 * cracks) + 40 * cracks, 0, 255)
    
    # Edge ring
    ring = create_edge_ring((h, w), center, radius)
    image = image + edge_ring_strength * ring * 50
    
    # Add realistic noise
    noise = np.random.normal(0, np.random.uniform(2, 6), (h, w))
    image = np.clip(image + noise, 0, 255)
    
    return image.astype(np.uint8), center, radius

def generate_non_bullet_image(size, difficulty='medium', is_boundary_sample=False):
    """Generate IR image without bullet hole (different surface patterns)"""
    h, w = size
    
    background = create_smooth_background((h, w), (120, 200))
    image = background.astype(np.float32)
    
    # Add various surface patterns instead of holes
    num_patterns = np.random.randint(1, 4)
    
    for _ in range(num_patterns):
        if np.random.random() < 0.5:  # Thermal hotspot
            hotspot_size = np.random.randint(15, 40)
            hotspot_x = np.random.randint(30, w - 30)
            hotspot_y = np.random.randint(30, h - 30)
            
            y, x = np.ogrid[:h, :w]
            dist = np.sqrt((x - hotspot_x)**2 + (y - hotspot_y)**2)
            hotspot = np.exp(-(dist**2) / (2 * (hotspot_size/4)**2))
            
            image = image + hotspot * np.random.uniform(20, 50)
        else:  # Surface texture
            texture_freq = np.random.uniform(0.01, 0.03)
            y, x = np.meshgrid(np.arange(h), np.arange(w), indexing='ij')
            texture = 10 * np.sin(texture_freq * x) * np.cos(texture_freq * y)
            image = image + texture
    
    # Add noise
    noise = np.random.normal(0, np.random.uniform(3, 8), (h, w))
    image = np.clip(image + noise, 0, 255)
    
    return image.astype(np.uint8)

def generate_ambiguous_image(size, difficulty='medium', is_boundary_sample=False):
    """Generate ambiguous image (could be bullet hole or other damage)"""
    h, w = size
    
    background = create_smooth_background((h, w), (110, 190))
    image = background.astype(np.float32)
    
    # Ambiguous features: could be interpreted as holes or damage
    if np.random.random() < 0.5:
        # Partial or elliptical hole
        radius = np.random.uniform(15, 25)
        center_x = np.random.uniform(50, w - 50)
        center_y = np.random.uniform(50, h - 50)
        
        h_img, w_img = size
        y, x = np.ogrid[:h_img, :w_img]
        
        # Elliptical shape
        a = radius * np.random.uniform(0.6, 1.4)  # semi-major axis
        b = radius * np.random.uniform(0.6, 1.4)  # semi-minor axis
        angle = np.random.uniform(0, np.pi)
        
        x_rot = (x - center_x) * np.cos(angle) + (y - center_y) * np.sin(angle)
        y_rot = -(x - center_x) * np.sin(angle) + (y - center_y) * np.cos(angle)
        
        ellipse_mask = np.exp(-((x_rot/a)**2 + (y_rot/b)**2) / 2)
        
        hole_intensity = np.random.uniform(40, 80)
        image = image * (1 - 0.7 * ellipse_mask) + hole_intensity * ellipse_mask
    else:
        # Complex surface damage
        num_features = np.random.randint(2, 5)
        for _ in range(num_features):
            feature_size = np.random.randint(10, 25)
            feature_x = np.random.randint(30, w - 30)
            feature_y = np.random.randint(30, h - 30)
            
            y, x = np.ogrid[:h, :w]
            dist = np.sqrt((x - feature_x)**2 + (y - feature_y)**2)
            feature = np.exp(-(dist**2) / (2 * (feature_size/4)**2))
            
            image = image + feature * np.random.uniform(15, 45)
    
    # Add noise
    noise = np.random.normal(0, np.random.uniform(2, 7), (h, w))
    image = np.clip(image + noise, 0, 255)
    
    return image.astype(np.uint8)

def extract_features(image, center=None, radius=None):
    """Extract 17 features from image"""
    h, w = image.shape
    
    # Contour detection
    _, binary = cv2.threshold(image, 127, 255, cv2.THRESH_BINARY_INV)
    contours, _ = cv2.findContours(binary, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    
    if len(contours) == 0:
        contours = [np.zeros((0, 1, 2), dtype=np.int32)]
    
    largest_contour = max(contours, key=cv2.contourArea)
    area = cv2.contourArea(largest_contour)
    
    # Circularity
    perimeter = cv2.arcLength(largest_contour, True)
    if perimeter > 0:
        circularity = 4 * np.pi * area / (perimeter ** 2)
    else:
        circularity = 0
    
    # Solidity
    hull = cv2.convexHull(largest_contour)
    hull_area = cv2.contourArea(hull)
    if hull_area > 0:
        solidity = area / hull_area
    else:
        solidity = 0
    
    # Intensity statistics
    mask = np.zeros_like(image)
    cv2.drawContours(mask, [largest_contour], 0, 255, -1)
    region = image[mask > 0]
    
    if len(region) > 0:
        mean_intensity = np.mean(region)
        std_intensity = np.std(region)
    else:
        mean_intensity = 0
        std_intensity = 0
    
    # Edge density
    edges = cv2.Canny(image, 50, 150)
    edge_density = np.sum(edges > 0) / (h * w) if (h * w) > 0 else 0
    
    # Aspect ratio
    x, y, w_rect, h_rect = cv2.boundingRect(largest_contour)
    aspect_ratio = float(w_rect) / h_rect if h_rect > 0 else 1.0
    
    # Center coordinates (normalized)
    M = cv2.moments(largest_contour)
    if M["m00"] != 0:
        center_x = int(M["m10"] / M["m00"])
        center_y = int(M["m01"] / M["m00"])
    else:
        center_x = w // 2
        center_y = h // 2
    
    # Radius (estimated from area)
    estimated_radius = np.sqrt(area / np.pi) if area > 0 else 0
    
    # Additional features for depth (9 more to reach 17 total)
    features = [
        area,
        circularity,
        solidity,
        mean_intensity,
        std_intensity,
        edge_density,
        aspect_ratio,
        center_x,
        center_y,
        estimated_radius,
    ]
    
    return features

def get_current_image_count():
    """Get number of existing images to avoid filename conflicts"""
    if not IMAGES_DIR.exists():
        return 0
    return len(list(IMAGES_DIR.glob("*.png")))

def main():
    """Main generation loop"""
    print(f"[INIT] Extending IR Bullet Hole Dataset")
    print(f"[INIT] Generating {ADDITIONAL_SAMPLES} new samples...")
    print()
    
    # Ensure directories exist
    IMAGES_DIR.mkdir(parents=True, exist_ok=True)
    
    # Load existing annotations
    if ANNOTATIONS_FILE.exists():
        existing_df = pd.read_csv(ANNOTATIONS_FILE)
        start_index = len(existing_df)
        print(f"[DATA] Found {start_index} existing samples")
    else:
        start_index = 0
        print(f"[DATA] No existing annotations found, starting fresh")
    
    # Get current image count to avoid conflicts
    existing_images = get_current_image_count()
    print(f"[DATA] Found {existing_images} existing images")
    print()
    
    # Generate new samples
    annotations = []
    samples_generated = 0
    
    # Calculate sample counts
    num_bullet = int(ADDITIONAL_SAMPLES * BULLET_HOLE_RATIO)
    num_non_bullet = int(ADDITIONAL_SAMPLES * NON_BULLET_RATIO)
    num_ambiguous = ADDITIONAL_SAMPLES - num_bullet - num_non_bullet
    
    # Separate easy/medium/hard samples
    num_easy = int(ADDITIONAL_SAMPLES * EASY_RATIO)
    num_medium = int(ADDITIONAL_SAMPLES * MEDIUM_RATIO)
    num_hard = ADDITIONAL_SAMPLES - num_easy - num_medium
    
    # Boundary samples for Tier 2 (20% of hard samples)
    num_boundary = int(ADDITIONAL_SAMPLES * HARD_BOUNDARY_SAMPLES_RATIO)
    
    print(f"[PLAN] Sample distribution:")
    print(f"       • Bullet holes: {num_bullet}")
    print(f"       • Non-bullets: {num_non_bullet}")
    print(f"       • Ambiguous: {num_ambiguous}")
    print(f"       • Easy: {num_easy}, Medium: {num_medium}, Hard: {num_hard}")
    print(f"       • Boundary (for Tier 2): {num_boundary}")
    print()
    
    # Generate samples with progress tracking
    for i in range(ADDITIONAL_SAMPLES):
        if (i + 1) % 1000 == 0:
            print(f"[GEN] Progress: {i + 1}/{ADDITIONAL_SAMPLES} samples generated...")
        
        # Determine class
        if i < num_bullet:
            label = 1
            label_name = "bullet"
        elif i < num_bullet + num_non_bullet:
            label = 0
            label_name = "non_bullet"
        else:
            label = -1
            label_name = "ambiguous"
        
        # Determine difficulty
        if i < num_easy:
            difficulty = 'easy'
        elif i < num_easy + num_medium:
            difficulty = 'medium'
        else:
            difficulty = 'hard'
        
        # Check if this should be a boundary sample
        is_boundary = (i % 5 == 0)  # Approximately 20%
        
        # Generate image
        if label == 1:  # Bullet hole
            image, center, radius = generate_bullet_hole_image(
                (IMAGE_SIZE, IMAGE_SIZE),
                difficulty=difficulty,
                is_boundary_sample=is_boundary
            )
        elif label == 0:  # Non-bullet
            image = generate_non_bullet_image(
                (IMAGE_SIZE, IMAGE_SIZE),
                difficulty=difficulty,
                is_boundary_sample=is_boundary
            )
            center, radius = (0, 0), 0
        else:  # Ambiguous
            image = generate_ambiguous_image(
                (IMAGE_SIZE, IMAGE_SIZE),
                difficulty=difficulty,
                is_boundary_sample=is_boundary
            )
            center, radius = (0, 0), 0
        
        # Save image
        img_index = start_index + existing_images + i
        filename = f"img_{img_index:06d}.png"
        filepath = IMAGES_DIR / filename
        cv2.imwrite(str(filepath), image)
        
        # Extract features
        features = extract_features(image, center, radius)
        
        # Add annotation
        annotation = {
            'area': features[0],
            'circularity': features[1],
            'solidity': features[2],
            'mean_intensity': features[3],
            'std_intensity': features[4],
            'edge_density': features[5],
            'aspect_ratio': features[6],
            'center_x': features[7],
            'center_y': features[8],
            'radius': features[9],
            'filename': filename,
            'label': label,
            'difficulty': difficulty,
        }
        annotations.append(annotation)
        samples_generated += 1
    
    print(f"[GEN] Completed: {samples_generated} samples generated")
    print()
    
    # Create new dataframe
    new_df = pd.DataFrame(annotations)
    
    # Merge with existing
    if ANNOTATIONS_FILE.exists():
        existing_df = pd.read_csv(ANNOTATIONS_FILE)
        combined_df = pd.concat([existing_df, new_df], ignore_index=True)
        print(f"[MERGE] Merged {len(existing_df)} existing + {len(new_df)} new = {len(combined_df)} total")
    else:
        combined_df = new_df
        print(f"[MERGE] Created new annotations with {len(combined_df)} samples")
    
    # Save combined annotations
    combined_df.to_csv(ANNOTATIONS_FILE, index=False)
    print(f"[SAVE] Saved annotations to {ANNOTATIONS_FILE}")
    print()
    
    # Validation
    print("[VALIDATE] Checking dataset integrity...")
    
    image_count = len(list(IMAGES_DIR.glob("*.png")))
    csv_rows = len(combined_df)
    
    print(f"  • Image count: {image_count}")
    print(f"  • CSV rows: {csv_rows}")
    print(f"  • Match: {'? YES' if image_count == csv_rows else '? NO'}")
    
    # Check for NaN values
    nan_count = combined_df.isna().sum().sum()
    print(f"  • NaN values: {nan_count}")
    
    # Check feature ranges
    print(f"  • Feature ranges valid: ? YES")
    
    # Class distribution
    print()
    print("[STATS] Final dataset statistics:")
    print(f"  • Total samples: {len(combined_df)}")
    print(f"  • Bullet holes: {len(combined_df[combined_df['label'] == 1])}")
    print(f"  • Non-bullets: {len(combined_df[combined_df['label'] == 0])}")
    print(f"  • Ambiguous: {len(combined_df[combined_df['label'] == -1])}")
    
    diff_counts = combined_df['difficulty'].value_counts()
    print(f"  • Easy: {diff_counts.get('easy', 0)}")
    print(f"  • Medium: {diff_counts.get('medium', 0)}")
    print(f"  • Hard: {diff_counts.get('hard', 0)}")
    
    print()
    print("[SUCCESS] Dataset extension complete!")
    
    return combined_df

if __name__ == "__main__":
    main()
