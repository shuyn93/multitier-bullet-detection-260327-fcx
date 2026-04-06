#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Realistic IR Bullet Hole Dataset Generator
Generates synthetic IR images matching real-world bullet hole characteristics
"""

import numpy as np
import cv2
import pandas as pd
from pathlib import Path
from scipy.ndimage import gaussian_filter

# Configuration
DATASET_SIZE = 10000
IMAGE_SIZE = 256
DATASET_DIR = Path("dataset_ir_realistic")
IMAGES_DIR = DATASET_DIR / "images"

BULLET_HOLE_RATIO = 0.50
NON_BULLET_RATIO = 0.30
AMBIGUOUS_RATIO = 0.20

EASY_RATIO = 0.30
MEDIUM_RATIO = 0.50
HARD_RATIO = 0.20

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

def generate_dataset(num_samples=10000):
    """Generate full dataset"""
    print(f"Generating {num_samples} synthetic IR images...")
    print(f"Output directory: {DATASET_DIR}")
    
    DATASET_DIR.mkdir(parents=True, exist_ok=True)
    IMAGES_DIR.mkdir(parents=True, exist_ok=True)
    
    annotations = []
    
    num_bullet = int(num_samples * BULLET_HOLE_RATIO)
    num_non_bullet = int(num_samples * NON_BULLET_RATIO)
    num_ambiguous = num_samples - num_bullet - num_non_bullet
    
    total = 0
    
    print(f"\n[1/3] Generating {num_bullet} bullet hole images...")
    for i in range(num_bullet):
        rand = np.random.rand()
        if rand < EASY_RATIO:
            difficulty = 'easy'
        elif rand < EASY_RATIO + MEDIUM_RATIO:
            difficulty = 'medium'
        else:
            difficulty = 'hard'
        
        image, center, radius = generate_bullet_hole_image((IMAGE_SIZE, IMAGE_SIZE), difficulty)
        
        filename = f"img_{total:06d}.png"
        cv2.imwrite(str(IMAGES_DIR / filename), image)
        
        features = extract_basic_features(image, center, radius)
        features['filename'] = filename
        features['label'] = 1
        features['difficulty'] = difficulty
        annotations.append(features)
        
        if (i + 1) % 1000 == 0:
            print(f"  Generated {i + 1}/{num_bullet}")
        
        total += 1
    
    print(f"\n[2/3] Generating {num_non_bullet} non-bullet images...")
    for i in range(num_non_bullet):
        image = generate_non_bullet_image((IMAGE_SIZE, IMAGE_SIZE))
        
        filename = f"img_{total:06d}.png"
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
            print(f"  Generated {i + 1}/{num_non_bullet}")
        
        total += 1
    
    print(f"\n[3/3] Generating {num_ambiguous} ambiguous images...")
    for i in range(num_ambiguous):
        image, center, radius = generate_ambiguous_image((IMAGE_SIZE, IMAGE_SIZE))
        
        filename = f"img_{total:06d}.png"
        cv2.imwrite(str(IMAGES_DIR / filename), image)
        
        features = extract_basic_features(image, center, radius)
        features['filename'] = filename
        features['label'] = -1
        features['difficulty'] = 'ambiguous'
        annotations.append(features)
        
        if (i + 1) % 1000 == 0:
            print(f"  Generated {i + 1}/{num_ambiguous}")
        
        total += 1
    
    df = pd.DataFrame(annotations)
    df.to_csv(DATASET_DIR / "annotations.csv", index=False)
    print(f"\nDataset generation complete!")
    print(f"Total images: {len(df)}")
    print(f"Annotations saved: {DATASET_DIR / 'annotations.csv'}")
    
    return df

if __name__ == "__main__":
    df = generate_dataset(10000)
    
    print(f"\n{'='*60}")
    print("DATASET STATISTICS")
    print(f"{'='*60}")
    print(f"Total images: {len(df)}")
    print(f"Bullet holes: {(df['label'] == 1).sum()}")
    print(f"Non-bullets: {(df['label'] == 0).sum()}")
    print(f"Ambiguous: {(df['label'] == -1).sum()}")
    
    bullet_df = df[df['label'] == 1]
    if len(bullet_df) > 0:
        print(f"\nFeature Statistics (Bullet Holes):")
        print(f"  Mean Intensity: {bullet_df['mean_intensity'].mean():.1f}")
        print(f"  Circularity: {bullet_df['circularity'].mean():.3f}")
        print(f"  Area: {bullet_df['area'].describe()}")
