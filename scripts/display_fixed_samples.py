#!/usr/bin/env python3
"""
Display samples from the FIXED dataset
"""

import pandas as pd
import numpy as np
from pathlib import Path
import matplotlib.pyplot as plt
from PIL import Image

def display_fixed_samples():
    """Display samples from fixed dataset"""
    
    dataset_path = Path(r'C:\Users\Admin\source\repos\bullet_hole_detection_system\dataset')
    csv_path = dataset_path / 'annotations.csv'
    images_dir = dataset_path / 'images'
    output_dir = Path(r'C:\Users\Admin\source\repos\bullet_hole_detection_system\output')
    output_dir.mkdir(exist_ok=True)
    
    df = pd.read_csv(csv_path)
    
    # Display one from each label
    labels_to_show = ['bullet_hole', 'non_bullet', 'ambiguous']
    
    fig, axes = plt.subplots(1, 3, figsize=(18, 6))
    
    for idx, label in enumerate(labels_to_show):
        label_df = df[df['label'] == label]
        sample = label_df.iloc[np.random.randint(len(label_df))]
        
        sample_id = str(int(sample['sample_id'])).zfill(6)
        image_path = images_dir / f"{sample_id}.png"
        
        img = Image.open(image_path)
        
        ax = axes[idx]
        ax.imshow(img, cmap='gray')
        ax.set_title(f"{label.upper()}\n(ID: {sample_id})", fontsize=14, fontweight='bold')
        ax.axis('off')
        
        print(f"\n{label.upper()} Sample:")
        print(f"  ID: {sample_id}")
        print(f"  Area: {sample['area']:.1f} px2")
        print(f"  Circularity: {sample['circularity']:.4f}")
        print(f"  Mean Intensity: {sample['mean_intensity']:.2f}")
        print(f"  Std Intensity: {sample['std_intensity']:.2f}")
        print(f"  Contrast: {sample['contrast']:.4f}")
        print(f"  Hole Depth: {sample['hole_depth_estimate']:.4f}")
        print(f"  Difficulty: {sample['difficulty_level']}")
    
    plt.tight_layout()
    output_path = output_dir / "fixed_dataset_samples.png"
    plt.savefig(output_path, dpi=100, bbox_inches='tight')
    print(f"\n\nSamples saved to: {output_path}")
    plt.close()

if __name__ == '__main__':
    display_fixed_samples()
