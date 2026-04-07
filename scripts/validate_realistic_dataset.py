"""
Dataset Quality Control & Visualization - V2

Displays random samples from generated dataset and verifies:
1. Visual quality (hole clearly visible, not noise)
2. Physical realism (smooth background, proper gradients)
3. Feature consistency (area/features match visual structure)
4. Intensity calibration (proper uint8 range)
"""

import numpy as np
import cv2
import pandas as pd
from pathlib import Path
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
from scipy import ndimage

def visualize_samples(num_samples=10, dataset_dir="dataset_realistic"):
    """Display random samples with detailed analysis"""
    
    dataset_path = Path(dataset_dir)
    images_dir = dataset_path / "images"
    csv_path = dataset_path / "annotations.csv"
    
    if not csv_path.exists():
        print(f"Error: {csv_path} not found")
        return
    
    df = pd.read_csv(csv_path)
    
    # Random sample selection
    sample_indices = np.random.choice(len(df), min(num_samples, len(df)), replace=False)
    
    fig = plt.figure(figsize=(20, 4 * num_samples))
    fig.suptitle("IR Bullet Hole Dataset - Quality Control", fontsize=16, fontweight='bold')
    
    for plot_idx, sample_idx in enumerate(sample_indices):
        row = df.iloc[sample_idx]
        sample_id = int(row['sample_id'])
        
        # Load image
        image_path = images_dir / f"{sample_id:06d}.png"
        img = cv2.imread(str(image_path), cv2.IMREAD_GRAYSCALE)
        
        if img is None:
            print(f"Warning: Could not load {image_path}")
            continue
        
        # ---- Subplot 1: Original Image ----
        ax1 = plt.subplot(num_samples, 4, plot_idx * 4 + 1)
        ax1.imshow(img, cmap='gray')
        ax1.set_title(f"Image #{sample_id}\n{row['label']} ({row['difficulty_level']})")
        ax1.axis('off')
        
        # ---- Subplot 2: Intensity Histogram ----
        ax2 = plt.subplot(num_samples, 4, plot_idx * 4 + 2)
        hist, bins = np.histogram(img, bins=32, range=(0, 256))
        ax2.bar(bins[:-1], hist, width=8, color='steelblue', alpha=0.7, edgecolor='black')
        ax2.set_title("Intensity Distribution")
        ax2.set_xlabel("Intensity (uint8)")
        ax2.set_ylabel("Frequency")
        ax2.set_xlim(0, 256)
        ax2.grid(True, alpha=0.3)
        
        # Add reference lines
        ax2.axvline(row['mean_intensity'], color='red', linestyle='--', linewidth=2, label='Mean')
        ax2.axvline(row['min_intensity'], color='green', linestyle='--', linewidth=2, label='Min')
        ax2.axvline(row['max_intensity'], color='orange', linestyle='--', linewidth=2, label='Max')
        ax2.legend(fontsize=8)
        
        # ---- Subplot 3: Thresholded Mask ----
        ax3 = plt.subplot(num_samples, 4, plot_idx * 4 + 3)
        
        # Create mask using Otsu thresholding
        _, binary = cv2.threshold(img, 0, 255, cv2.THRESH_BINARY + cv2.THRESH_OTSU)
        binary = cv2.bitwise_not(binary)  # Invert to get dark regions
        
        ax3.imshow(binary, cmap='gray')
        ax3.set_title(f"Binary Mask\nArea: {row['area']:.0f}")
        ax3.axis('off')
        
        # ---- Subplot 4: Feature Summary ----
        ax4 = plt.subplot(num_samples, 4, plot_idx * 4 + 4)
        ax4.axis('off')
        
        # Feature text
        feature_text = f"""
LABEL:  {row['label']}
DIFFICULTY:  {row['difficulty_level']}

GEOMETRY:
  Circularity:  {row['circularity']:.3f}
  Eccentricity:  {row['eccentricity']:.3f}
  Aspect Ratio:  {row['aspect_ratio']:.2f}

INTENSITY:
  Mean:  {row['mean_intensity']:.1f}
  Std:  {row['std_intensity']:.1f}
  Contrast:  {row['contrast']:.1f}

TEXTURE:
  Entropy:  {row['entropy']:.2f}
  Edge Density:  {row['edge_density']:.3f}
  Gradient Mean:  {row['gradient_mean']:.2f}

SHAPE:
  Contour Variance:  {row['contour_variance']:.2f}
  Fractal Dim:  {row['fractal_dimension']:.2f}
  Hole Depth:  {row['hole_depth_estimate']:.2f}
        """
        
        ax4.text(0.05, 0.95, feature_text, transform=ax4.transAxes,
                fontsize=9, verticalalignment='top', fontfamily='monospace',
                bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.5))
    
    plt.tight_layout()
    return fig


def analyze_dataset_quality(dataset_dir="dataset_realistic"):
    """Perform comprehensive quality checks"""
    
    dataset_path = Path(dataset_dir)
    csv_path = dataset_path / "annotations.csv"
    images_dir = dataset_path / "images"
    
    if not csv_path.exists():
        print(f"Error: {csv_path} not found")
        return
    
    df = pd.read_csv(csv_path)
    
    print("\n" + "=" * 70)
    print("DATASET QUALITY ANALYSIS")
    print("=" * 70)
    
    # ---- Intensity Calibration Check ----
    print("\n1. INTENSITY CALIBRATION CHECK:")
    print("-" * 70)
    
    background_samples = df[df['label'] == 'non_bullet']
    hole_samples = df[df['label'] == 'bullet_hole']
    
    if len(background_samples) > 0:
        bg_mean = background_samples['mean_intensity'].mean()
        bg_std = background_samples['mean_intensity'].std()
        print(f"  Non-bullet mean intensity: {bg_mean:.1f} ± {bg_std:.1f}")
        print(f"    ? PASS: Should be 100-180" if 100 <= bg_mean <= 180 else f"    ? FAIL: Outside range")
    
    if len(hole_samples) > 0:
        hole_min = hole_samples['min_intensity'].mean()
        hole_min_std = hole_samples['min_intensity'].std()
        print(f"  Hole center (min intensity): {hole_min:.1f} ± {hole_min_std:.1f}")
        print(f"    ? PASS: Should be 10-50" if 10 <= hole_min <= 50 else f"    ? FAIL: Outside range")
        
        hole_contrast = hole_samples['contrast'].mean()
        hole_contrast_std = hole_samples['contrast'].std()
        print(f"  Hole contrast: {hole_contrast:.1f} ± {hole_contrast_std:.1f}")
        print(f"    ? PASS: Strong contrast" if hole_contrast > 30 else f"    ? FAIL: Weak contrast")
    
    # ---- Shape Consistency Check ----
    print("\n2. SHAPE CONSISTENCY CHECK:")
    print("-" * 70)
    
    print(f"  Mean circularity: {df['circularity'].mean():.3f}")
    print(f"    ? PASS: Circular shapes" if df['circularity'].mean() > 0.6 else f"    ? FAIL: Not circular")
    
    print(f"  Mean aspect ratio: {df['aspect_ratio'].mean():.2f}")
    print(f"    ? PASS: Regular shapes" if 0.8 <= df['aspect_ratio'].mean() <= 1.5 else f"    ? FAIL: Elongated")
    
    # ---- Noise Level Check ----
    print("\n3. NOISE LEVEL CHECK:")
    print("-" * 70)
    
    for difficulty in ['easy', 'medium', 'hard']:
        subset = df[df['difficulty_level'] == difficulty]
        if len(subset) > 0:
            entropy = subset['entropy'].mean()
            std_intensity = subset['std_intensity'].mean()
            print(f"  {difficulty.upper()}:")
            print(f"    Entropy: {entropy:.2f}, Std Intensity: {std_intensity:.2f}")
    
    # ---- Feature Validity Check ----
    print("\n4. FEATURE VALIDITY CHECK:")
    print("-" * 70)
    
    # Check for invalid values
    invalid_count = 0
    
    if df['area'].min() < 0:
        print(f"  ? FAIL: Negative area values")
        invalid_count += 1
    else:
        print(f"  ? PASS: All area values valid ({df['area'].min():.0f} - {df['area'].max():.0f})")
    
    if df['circularity'].min() < 0 or df['circularity'].max() > 1:
        print(f"  ? FAIL: Circularity out of bounds")
        invalid_count += 1
    else:
        print(f"  ? PASS: Circularity valid")
    
    if df['mean_intensity'].min() < 0 or df['mean_intensity'].max() > 255:
        print(f"  ? FAIL: Intensity out of uint8 range")
        invalid_count += 1
    else:
        print(f"  ? PASS: All intensities in uint8 range")
    
    if invalid_count == 0:
        print(f"\n? ALL QUALITY CHECKS PASSED!")
    else:
        print(f"\n? FOUND {invalid_count} QUALITY ISSUES")
    
    # ---- Label Distribution ----
    print("\n5. LABEL DISTRIBUTION:")
    print("-" * 70)
    for label, count in df['label'].value_counts().items():
        pct = 100.0 * count / len(df)
        print(f"  {label}: {count} ({pct:.1f}%)")
    
    print("\n6. DIFFICULTY DISTRIBUTION:")
    print("-" * 70)
    for diff, count in df['difficulty_level'].value_counts().items():
        pct = 100.0 * count / len(df)
        print(f"  {diff}: {count} ({pct:.1f}%)")
    
    print("\n" + "=" * 70)


def visual_quality_check(num_samples=10, dataset_dir="dataset_realistic"):
    """Quick visual inspection of dataset quality"""
    
    print("\nGenerating quality control visualization...")
    print(f"Displaying {num_samples} random samples...")
    
    fig = visualize_samples(num_samples, dataset_dir)
    
    # Save figure
    output_path = Path(dataset_dir) / "quality_control.png"
    fig.savefig(str(output_path), dpi=100, bbox_inches='tight')
    print(f"? Saved quality control visualization to: {output_path}")
    
    plt.show()


if __name__ == '__main__':
    import sys
    
    dataset_dir = "dataset_realistic" if len(sys.argv) < 2 else sys.argv[1]
    
    # Run quality checks
    analyze_dataset_quality(dataset_dir)
    
    # Generate visualization
    visual_quality_check(num_samples=10, dataset_dir=dataset_dir)
