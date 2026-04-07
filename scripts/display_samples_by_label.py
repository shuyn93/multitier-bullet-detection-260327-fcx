#!/usr/bin/env python3
"""
Display multiple random images from the bullet hole dataset
Shows samples from each label class
"""

import pandas as pd
import numpy as np
from pathlib import Path
import matplotlib.pyplot as plt
from PIL import Image

def display_samples_by_label():
    """Display random samples, one from each label class"""
    
    dataset_path = Path(r'C:\Users\Admin\source\repos\bullet_hole_detection_system\scripts\dataset')
    csv_path = dataset_path / 'annotations.csv'
    images_dir = dataset_path / 'images'
    output_dir = Path(r'C:\Users\Admin\source\repos\bullet_hole_detection_system\output')
    output_dir.mkdir(exist_ok=True)
    
    # Load annotations
    df = pd.read_csv(csv_path)
    
    # Get unique labels
    labels = sorted(df['label'].unique())
    
    print(f"\nFound {len(labels)} label classes: {labels}\n")
    
    # Create figure with subplots for each label
    fig, axes = plt.subplots(1, len(labels), figsize=(18, 6))
    if len(labels) == 1:
        axes = [axes]
    
    for idx, label in enumerate(labels):
        # Get random sample with this label
        label_df = df[df['label'] == label]
        sample_idx = np.random.choice(len(label_df))
        sample = label_df.iloc[sample_idx]
        
        # Get image path
        sample_id = str(int(sample['sample_id'])).zfill(6)
        image_path = images_dir / f"{sample_id}.png"
        
        if not image_path.exists():
            print(f"Warning: Image not found at {image_path}")
            continue
        
        # Load image
        img = Image.open(image_path)
        
        # Display in subplot
        ax = axes[idx]
        ax.imshow(img, cmap='gray')
        
        # Title with label
        ax.set_title(f"{label.upper()}\n(ID: {sample_id})", 
                    fontsize=14, fontweight='bold', pad=10)
        ax.axis('off')
        
        # Print details
        print(f"\n{label.upper()} Sample")
        print(f"  ID: {sample_id}")
        print(f"  Area: {float(sample['area']):.2f}")
        print(f"  Circularity: {float(sample['circularity']):.4f}")
        print(f"  Mean Intensity: {float(sample['mean_intensity']):.2f}")
        print(f"  Difficulty: {sample['difficulty_level']}")
        print(f"  Noise Level: {float(sample['noise_level']):.4f}")
    
    plt.tight_layout()
    
    # Save figure
    output_path = output_dir / "samples_by_label.png"
    plt.savefig(output_path, dpi=100, bbox_inches='tight')
    print(f"\n\nComposite image saved to: {output_path}\n")
    
    plt.close()

if __name__ == '__main__':
    display_samples_by_label()
