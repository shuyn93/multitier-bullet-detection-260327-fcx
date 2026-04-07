#!/usr/bin/env python3
"""
Display a random image from the bullet hole dataset with its label
Saves image to disk and prints details
"""

import pandas as pd
import numpy as np
from pathlib import Path
import matplotlib.pyplot as plt
from PIL import Image

def display_random_sample():
    """Display a random sample image with its label and key features"""
    
    dataset_path = Path(r'C:\Users\Admin\source\repos\bullet_hole_detection_system\scripts\dataset')
    csv_path = dataset_path / 'annotations.csv'
    images_dir = dataset_path / 'images'
    output_dir = Path(r'C:\Users\Admin\source\repos\bullet_hole_detection_system\output')
    output_dir.mkdir(exist_ok=True)
    
    # Load annotations
    df = pd.read_csv(csv_path)
    
    # Select random sample
    idx = np.random.randint(0, len(df))
    sample = df.iloc[idx]
    
    # Get image path
    sample_id = str(int(sample['sample_id'])).zfill(6)
    image_path = images_dir / f"{sample_id}.png"
    
    if not image_path.exists():
        print(f"Error: Image not found at {image_path}")
        return
    
    # Load image
    img = Image.open(image_path)
    
    # Create figure
    fig, ax = plt.subplots(1, 1, figsize=(12, 10))
    
    # Display image
    ax.imshow(img, cmap='gray')
    
    # Create title with label and ID
    title = f"Sample ID: {sample_id}\nLabel: {sample['label'].upper()}"
    ax.set_title(title, fontsize=18, fontweight='bold', pad=20)
    
    # Add text box with key features
    feature_text = (
        f"Area: {float(sample['area']):.2f} px2\n"
        f"Circularity: {float(sample['circularity']):.4f}\n"
        f"Eccentricity: {float(sample['eccentricity']):.4f}\n"
        f"Mean Intensity: {float(sample['mean_intensity']):.2f}\n"
        f"Contrast: {float(sample['contrast']):.4f}\n"
        f"Entropy: {float(sample['entropy']):.4f}\n"
        f"Edge Density: {float(sample['edge_density']):.4f}\n"
        f"Difficulty: {sample['difficulty_level']}\n"
        f"Noise: {float(sample['noise_level']):.4f}\n"
        f"Blur: {float(sample['blur_level']):.4f}"
    )
    
    ax.text(0.02, 0.98, feature_text, transform=ax.transAxes,
            fontsize=11, verticalalignment='top', family='monospace',
            bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.9, pad=1))
    
    # Remove axes
    ax.axis('off')
    
    plt.tight_layout()
    
    # Save figure
    output_path = output_dir / f"sample_{sample_id}_display.png"
    plt.savefig(output_path, dpi=100, bbox_inches='tight')
    print(f"\nImage saved to: {output_path}")
    
    plt.close()
    
    # Print details to console
    print("\n" + "="*70)
    print("RANDOM SAMPLE FROM DATASET")
    print("="*70)
    print(f"\nSample ID: {sample_id}")
    print(f"Label: {sample['label'].upper()}")
    print(f"Image Path: {image_path}")
    print(f"Image Size: {img.size[0]} x {img.size[1]} pixels")
    print(f"\nKey Features:")
    print(f"  Area: {float(sample['area']):.2f} px2")
    print(f"  Perimeter: {float(sample['perimeter']):.2f} px")
    print(f"  Circularity: {float(sample['circularity']):.4f}")
    print(f"  Eccentricity: {float(sample['eccentricity']):.4f}")
    print(f"  Aspect Ratio: {float(sample['aspect_ratio']):.4f}")
    print(f"  Mean Intensity: {float(sample['mean_intensity']):.2f}")
    print(f"  Std Intensity: {float(sample['std_intensity']):.2f}")
    print(f"  Min Intensity: {float(sample['min_intensity']):.2f}")
    print(f"  Max Intensity: {float(sample['max_intensity']):.2f}")
    print(f"  Contrast: {float(sample['contrast']):.4f}")
    print(f"  Entropy: {float(sample['entropy']):.4f}")
    print(f"  Edge Density: {float(sample['edge_density']):.4f}")
    print(f"  Gradient Mean: {float(sample['gradient_mean']):.4f}")
    print(f"  Gradient Std: {float(sample['gradient_std']):.4f}")
    print(f"  Contour Variance: {float(sample['contour_variance']):.4f}")
    print(f"  Fractal Dimension: {float(sample['fractal_dimension']):.4f}")
    print(f"  Hole Depth Estimate: {float(sample['hole_depth_estimate']):.4f}")
    print(f"  Difficulty Level: {sample['difficulty_level']}")
    print(f"  Noise Level: {float(sample['noise_level']):.4f}")
    print(f"  Blur Level: {float(sample['blur_level']):.4f}")
    print(f"  Illumination Variance: {float(sample['illumination_variance']):.4f}")
    print("="*70 + "\n")

if __name__ == '__main__':
    display_random_sample()
