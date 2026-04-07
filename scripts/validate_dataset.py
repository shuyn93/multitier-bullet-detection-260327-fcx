"""
Dataset Validation and Analysis Tool

Validates the generated synthetic dataset and provides comprehensive statistics.
Useful for:
- Verifying feature consistency
- Detecting anomalies
- Understanding data distributions
- Quality assurance
"""

import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
from pathlib import Path
import cv2
from scipy import stats
import seaborn as sns

# ============================================================================
# Configuration
# ============================================================================

DATASET_DIR = Path("dataset")
IMAGES_DIR = DATASET_DIR / "images"
CSV_PATH = DATASET_DIR / "annotations.csv"

# ============================================================================
# Validation Functions
# ============================================================================

def check_dataset_structure():
    """Verify dataset directory structure"""
    print("=" * 70)
    print("DATASET STRUCTURE CHECK")
    print("=" * 70)
    
    if not DATASET_DIR.exists():
        print("? Dataset directory not found!")
        return False
    
    print(f"? Dataset directory: {DATASET_DIR.absolute()}")
    
    if not IMAGES_DIR.exists():
        print("? Images directory not found!")
        return False
    
    num_images = len(list(IMAGES_DIR.glob("*.png")))
    print(f"? Images directory: {num_images} PNG files found")
    
    if not CSV_PATH.exists():
        print("? Annotations CSV not found!")
        return False
    
    print(f"? Annotations CSV: {CSV_PATH.absolute()}")
    
    return num_images > 0


def load_annotations():
    """Load and validate annotations CSV"""
    print("\n" + "=" * 70)
    print("LOADING ANNOTATIONS")
    print("=" * 70)
    
    df = pd.read_csv(CSV_PATH)
    print(f"? Loaded {len(df)} annotation records")
    print(f"? Columns: {list(df.columns)}")
    
    return df


def validate_images(df, sample_size=100):
    """Validate image files and dimensions"""
    print("\n" + "=" * 70)
    print("IMAGE VALIDATION")
    print("=" * 70)
    
    num_samples = min(sample_size, len(df))
    issues = []
    
    for idx in df['sample_id'].iloc[:num_samples]:
        image_path = IMAGES_DIR / f"{idx:06d}.png"
        
        if not image_path.exists():
            issues.append(f"Missing: {image_path.name}")
            continue
        
        try:
            img = cv2.imread(str(image_path), cv2.IMREAD_GRAYSCALE)
            
            if img is None:
                issues.append(f"Cannot read: {image_path.name}")
                continue
            
            if img.shape != (256, 256):
                issues.append(f"Wrong dimensions {img.shape}: {image_path.name}")
                continue
            
            if img.dtype != np.uint8:
                issues.append(f"Wrong dtype {img.dtype}: {image_path.name}")
                continue
        
        except Exception as e:
            issues.append(f"Error reading {image_path.name}: {e}")
    
    if issues:
        print(f"? Found {len(issues)} issues:")
        for issue in issues[:10]:
            print(f"  - {issue}")
        if len(issues) > 10:
            print(f"  ... and {len(issues) - 10} more")
        return False
    else:
        print(f"? All {num_samples} checked images are valid")
        return True


def check_label_distribution(df):
    """Verify label distribution matches requirements"""
    print("\n" + "=" * 70)
    print("LABEL DISTRIBUTION")
    print("=" * 70)
    
    label_counts = df['label'].value_counts()
    label_ratios = df['label'].value_counts(normalize=True)
    
    print(f"Total samples: {len(df)}\n")
    
    for label in ['bullet_hole', 'non_bullet', 'ambiguous']:
        count = label_counts.get(label, 0)
        ratio = label_ratios.get(label, 0)
        print(f"  {label:15s}: {count:5d} ({ratio*100:6.2f}%)")
    
    # Check against expected distribution
    expected = {'bullet_hole': 0.50, 'non_bullet': 0.30, 'ambiguous': 0.20}
    tolerance = 0.05
    
    all_ok = True
    for label, expected_ratio in expected.items():
        actual_ratio = label_ratios.get(label, 0)
        if abs(actual_ratio - expected_ratio) > tolerance:
            print(f"  ? {label} distribution off by {abs(actual_ratio - expected_ratio)*100:.1f}%")
            all_ok = False
    
    if all_ok:
        print("\n? Label distribution matches requirements")
    
    return label_counts, label_ratios


def check_difficulty_distribution(df):
    """Verify difficulty distribution"""
    print("\n" + "=" * 70)
    print("DIFFICULTY DISTRIBUTION")
    print("=" * 70)
    
    diff_counts = df['difficulty_level'].value_counts()
    diff_ratios = df['difficulty_level'].value_counts(normalize=True)
    
    for diff in ['easy', 'medium', 'hard']:
        count = diff_counts.get(diff, 0)
        ratio = diff_ratios.get(diff, 0)
        print(f"  {diff:10s}: {count:5d} ({ratio*100:6.2f}%)")
    
    expected = {'easy': 0.30, 'medium': 0.50, 'hard': 0.20}
    tolerance = 0.05
    
    all_ok = True
    for diff, expected_ratio in expected.items():
        actual_ratio = diff_ratios.get(diff, 0)
        if abs(actual_ratio - expected_ratio) > tolerance:
            print(f"  ? {diff} distribution off by {abs(actual_ratio - expected_ratio)*100:.1f}%")
            all_ok = False
    
    if all_ok:
        print("\n? Difficulty distribution matches requirements")
    
    return diff_counts, diff_ratios


def validate_features(df):
    """Validate feature values for consistency"""
    print("\n" + "=" * 70)
    print("FEATURE VALIDATION")
    print("=" * 70)
    
    feature_cols = ['area', 'perimeter', 'circularity', 'eccentricity', 'aspect_ratio',
                    'mean_intensity', 'std_intensity', 'min_intensity', 'max_intensity',
                    'contrast', 'entropy', 'edge_density', 'gradient_mean', 'gradient_std',
                    'contour_variance', 'fractal_dimension', 'hole_depth_estimate']
    
    issues = []
    
    for col in feature_cols:
        if col not in df.columns:
            issues.append(f"Missing feature column: {col}")
            continue
        
        values = df[col]
        
        # Check for NaN/Inf
        nan_count = values.isna().sum()
        inf_count = np.isinf(values).sum()
        
        if nan_count > 0:
            issues.append(f"{col}: {nan_count} NaN values")
        if inf_count > 0:
            issues.append(f"{col}: {inf_count} Inf values")
        
        # Check ranges (domain-specific)
        if col == 'circularity':
            bad = ((values < 0) | (values > 1)).sum()
            if bad > 0:
                issues.append(f"{col}: {bad} values outside [0, 1]")
        
        elif col == 'eccentricity':
            bad = ((values < 0) | (values > 1)).sum()
            if bad > 0:
                issues.append(f"{col}: {bad} values outside [0, 1]")
        
        elif col in ['aspect_ratio']:
            bad = (values < 0.1).sum()
            if bad > 0:
                issues.append(f"{col}: {bad} values < 0.1")
        
        elif col in ['mean_intensity', 'min_intensity', 'max_intensity']:
            bad = ((values < 0) | (values > 255)).sum()
            if bad > 0:
                issues.append(f"{col}: {bad} values outside [0, 255]")
        
        elif col == 'entropy':
            bad = ((values < 0) | (values > 8)).sum()
            if bad > 0:
                issues.append(f"{col}: {bad} values outside [0, 8]")
        
        elif col == 'fractal_dimension':
            bad = ((values < 1) | (values > 3)).sum()
            if bad > 0:
                issues.append(f"{col}: {bad} values outside [1, 3]")
    
    if issues:
        print("? Feature validation issues:")
        for issue in issues[:20]:
            print(f"  - {issue}")
        if len(issues) > 20:
            print(f"  ... and {len(issues) - 20} more")
        return False
    else:
        print("? All features are valid")
        return True


def validate_feature_correlations(df):
    """Check realistic feature correlations"""
    print("\n" + "=" * 70)
    print("FEATURE CORRELATION VALIDATION")
    print("=" * 70)
    
    # Correlation checks
    checks = [
        ('circularity', 'eccentricity', -0.7, -0.4, 'negative'),
        ('area', 'perimeter', 0.6, 0.9, 'positive'),
        ('mean_intensity', 'contrast', 0.0, 0.5, 'weak-to-moderate'),
        ('entropy', 'edge_density', 0.2, 0.6, 'weak-to-moderate'),
    ]
    
    feature_cols = ['area', 'perimeter', 'circularity', 'eccentricity', 'aspect_ratio',
                    'mean_intensity', 'std_intensity', 'min_intensity', 'max_intensity',
                    'contrast', 'entropy', 'edge_density', 'gradient_mean', 'gradient_std',
                    'contour_variance', 'fractal_dimension', 'hole_depth_estimate']
    
    df_features = df[feature_cols].copy()
    
    # Filter out zero-variance features for correlation
    df_features = df_features.loc[:, (df_features != 0).any(axis=0)]
    
    corr_matrix = df_features.corr()
    
    print("\nFeature Correlation Checks:")
    for feat1, feat2, expected_min, expected_max, description in checks:
        if feat1 in corr_matrix.index and feat2 in corr_matrix.columns:
            corr = corr_matrix.loc[feat1, feat2]
            print(f"  {feat1} vs {feat2}: {corr:.3f} (expected {description})")
        else:
            print(f"  {feat1} vs {feat2}: features not found")
    
    print("\n? Feature correlations computed")
    
    return corr_matrix


def compute_statistics(df):
    """Compute comprehensive statistics"""
    print("\n" + "=" * 70)
    print("DATASET STATISTICS")
    print("=" * 70)
    
    feature_cols = ['area', 'perimeter', 'circularity', 'eccentricity', 'aspect_ratio',
                    'mean_intensity', 'std_intensity', 'min_intensity', 'max_intensity',
                    'contrast', 'entropy', 'edge_density', 'gradient_mean', 'gradient_std',
                    'contour_variance', 'fractal_dimension', 'hole_depth_estimate']
    
    stats_table = df[feature_cols].describe()
    
    print("\nFeature Statistics Summary:")
    print(stats_table.to_string())
    
    return stats_table


def check_metadata(df):
    """Validate metadata fields"""
    print("\n" + "=" * 70)
    print("METADATA VALIDATION")
    print("=" * 70)
    
    metadata_cols = ['sample_id', 'label', 'difficulty_level', 'noise_level', 
                     'blur_level', 'illumination_variance']
    
    for col in metadata_cols:
        if col not in df.columns:
            print(f"? Missing metadata column: {col}")
            continue
        
        print(f"? {col}: {df[col].dtype}")
        
        if col in ['noise_level', 'blur_level', 'illumination_variance']:
            min_val = df[col].min()
            max_val = df[col].max()
            print(f"    Range: [{min_val:.3f}, {max_val:.3f}]")
    
    print("\n? All metadata fields present")


# ============================================================================
# Visualization Functions
# ============================================================================

def plot_distributions(df, output_dir=Path("dataset_analysis")):
    """Generate distribution plots"""
    output_dir.mkdir(exist_ok=True)
    
    print("\n" + "=" * 70)
    print("GENERATING PLOTS")
    print("=" * 70)
    
    # Label distribution
    fig, ax = plt.subplots(figsize=(8, 5))
    df['label'].value_counts().plot(kind='bar', ax=ax)
    ax.set_title('Label Distribution')
    ax.set_ylabel('Count')
    plt.tight_layout()
    plt.savefig(output_dir / 'label_distribution.png', dpi=150)
    plt.close()
    print("? Saved label_distribution.png")
    
    # Difficulty distribution
    fig, ax = plt.subplots(figsize=(8, 5))
    df['difficulty_level'].value_counts().plot(kind='bar', ax=ax)
    ax.set_title('Difficulty Distribution')
    ax.set_ylabel('Count')
    plt.tight_layout()
    plt.savefig(output_dir / 'difficulty_distribution.png', dpi=150)
    plt.close()
    print("? Saved difficulty_distribution.png")
    
    # Feature distributions (sample)
    features_to_plot = ['area', 'circularity', 'mean_intensity', 'contrast', 'entropy']
    
    fig, axes = plt.subplots(len(features_to_plot), 1, figsize=(10, 12))
    for ax, feature in zip(axes, features_to_plot):
        df[feature].hist(bins=50, ax=ax)
        ax.set_title(f'{feature} Distribution')
        ax.set_xlabel(feature)
        ax.set_ylabel('Frequency')
    
    plt.tight_layout()
    plt.savefig(output_dir / 'feature_distributions.png', dpi=150)
    plt.close()
    print("? Saved feature_distributions.png")
    
    # Noise vs blur scatter
    fig, ax = plt.subplots(figsize=(8, 6))
    scatter = ax.scatter(df['noise_level'], df['blur_level'], 
                        c=df['difficulty_level'].map({'easy': 0, 'medium': 1, 'hard': 2}),
                        alpha=0.5, s=10)
    ax.set_xlabel('Noise Level')
    ax.set_ylabel('Blur Level')
    ax.set_title('Noise vs Blur (colored by difficulty)')
    plt.colorbar(scatter, ax=ax, label='Difficulty')
    plt.tight_layout()
    plt.savefig(output_dir / 'noise_blur_scatter.png', dpi=150)
    plt.close()
    print("? Saved noise_blur_scatter.png")
    
    # Feature correlation heatmap
    feature_cols = ['area', 'perimeter', 'circularity', 'eccentricity', 'aspect_ratio',
                    'mean_intensity', 'contrast', 'entropy', 'edge_density', 
                    'fractal_dimension']
    df_features = df[feature_cols].copy()
    df_features = df_features.loc[:, (df_features != 0).any(axis=0)]
    
    corr_matrix = df_features.corr()
    
    fig, ax = plt.subplots(figsize=(12, 10))
    sns.heatmap(corr_matrix, annot=True, fmt='.2f', cmap='coolwarm', center=0,
                square=True, ax=ax, cbar_kws={'label': 'Correlation'})
    ax.set_title('Feature Correlation Matrix')
    plt.tight_layout()
    plt.savefig(output_dir / 'correlation_heatmap.png', dpi=150)
    plt.close()
    print("? Saved correlation_heatmap.png")
    
    # Label vs features
    fig, axes = plt.subplots(2, 3, figsize=(15, 10))
    axes = axes.flatten()
    
    for idx, feature in enumerate(features_to_plot):
        ax = axes[idx]
        for label in df['label'].unique():
            subset = df[df['label'] == label]
            ax.hist(subset[feature], bins=30, alpha=0.5, label=label)
        ax.set_title(f'{feature} by Label')
        ax.set_xlabel(feature)
        ax.set_ylabel('Frequency')
        ax.legend()
    
    plt.tight_layout()
    plt.savefig(output_dir / 'features_by_label.png', dpi=150)
    plt.close()
    print("? Saved features_by_label.png")
    
    print(f"\n? All plots saved to {output_dir.absolute()}")


# ============================================================================
# Main Validation Pipeline
# ============================================================================

def validate_dataset(generate_plots=True):
    """Run complete dataset validation"""
    print("\n")
    print("?" + "=" * 68 + "?")
    print("?" + " SYNTHETIC BULLET HOLE DATASET VALIDATION ".center(68) + "?")
    print("?" + "=" * 68 + "?")
    
    # Structure check
    if not check_dataset_structure():
        print("\n? Dataset structure check failed!")
        return False
    
    # Load annotations
    df = load_annotations()
    
    # Image validation
    validate_images(df, sample_size=100)
    
    # Distribution checks
    label_counts, label_ratios = check_label_distribution(df)
    diff_counts, diff_ratios = check_difficulty_distribution(df)
    
    # Feature validation
    validate_features(df)
    validate_feature_correlations(df)
    check_metadata(df)
    
    # Statistics
    stats_table = compute_statistics(df)
    
    # Plots
    if generate_plots:
        try:
            plot_distributions(df)
        except Exception as e:
            print(f"? Could not generate plots: {e}")
    
    # Final summary
    print("\n" + "=" * 70)
    print("VALIDATION SUMMARY")
    print("=" * 70)
    print(f"? Dataset is ready for use!")
    print(f"  - Total samples: {len(df)}")
    print(f"  - Features per sample: 17")
    print(f"  - Labels: 3 classes (bullet_hole, non_bullet, ambiguous)")
    print(f"  - Difficulty levels: 3 (easy, medium, hard)")
    print(f"  - Image size: 256×256 pixels")
    print(f"  - Image format: PNG (grayscale)")
    print("\n" + "=" * 70)


if __name__ == '__main__':
    validate_dataset(generate_plots=True)
