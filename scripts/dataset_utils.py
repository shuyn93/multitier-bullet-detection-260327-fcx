"""
Dataset Utilities and Example Usage

Provides helper functions for:
- Loading dataset in batches
- Feature normalization
- Train/validation/test splitting
- Data augmentation
- Integration with ML models (Naive Bayes, GMM, MLP)
"""

import numpy as np
import pandas as pd
import cv2
from pathlib import Path
from sklearn.preprocessing import StandardScaler
from sklearn.model_selection import train_test_split
from typing import Tuple, List
import pickle

# ============================================================================
# Dataset Loading
# ============================================================================

class BulletHoleDataset:
    """Dataset loader and preprocessor"""
    
    def __init__(self, dataset_dir: str = "dataset"):
        self.dataset_dir = Path(dataset_dir)
        self.images_dir = self.dataset_dir / "images"
        self.csv_path = self.dataset_dir / "annotations.csv"
        
        # Load annotations
        self.annotations = pd.read_csv(self.csv_path)
        
        # Feature columns
        self.feature_cols = [
            'area', 'perimeter', 'circularity', 'eccentricity', 'aspect_ratio',
            'mean_intensity', 'std_intensity', 'min_intensity', 'max_intensity',
            'contrast', 'entropy', 'edge_density', 'gradient_mean', 'gradient_std',
            'contour_variance', 'fractal_dimension', 'hole_depth_estimate'
        ]
        
        # Label encoding
        self.label_to_idx = {'bullet_hole': 0, 'non_bullet': 1, 'ambiguous': 2}
        self.idx_to_label = {v: k for k, v in self.label_to_idx.items()}
        
        # Scaler
        self.scaler = StandardScaler()
        self._fit_scaler()
    
    def _fit_scaler(self):
        """Fit feature scaler on entire dataset"""
        features = self.annotations[self.feature_cols].values
        self.scaler.fit(features)
    
    def load_image(self, sample_id: int) -> np.ndarray:
        """Load single image by sample_id"""
        image_path = self.images_dir / f"{sample_id:06d}.png"
        img = cv2.imread(str(image_path), cv2.IMREAD_GRAYSCALE)
        if img is None:
            raise FileNotFoundError(f"Image not found: {image_path}")
        return img
    
    def get_sample(self, sample_id: int) -> Tuple[np.ndarray, str, np.ndarray]:
        """Get complete sample: (image, label, normalized_features)"""
        # Load image
        image = self.load_image(sample_id)
        
        # Get label
        row = self.annotations[self.annotations['sample_id'] == sample_id].iloc[0]
        label = row['label']
        
        # Get features
        features = row[self.feature_cols].values.astype(np.float32)
        features_normalized = self.scaler.transform([features])[0]
        
        return image, label, features_normalized
    
    def get_feature_vector(self, sample_id: int, normalize: bool = True) -> np.ndarray:
        """Get feature vector only"""
        row = self.annotations[self.annotations['sample_id'] == sample_id].iloc[0]
        features = row[self.feature_cols].values.astype(np.float32)
        
        if normalize:
            features = self.scaler.transform([features])[0]
        
        return features
    
    def get_batch(self, indices: List[int], normalize: bool = True) -> Tuple[np.ndarray, np.ndarray]:
        """Get batch of features and labels"""
        features_list = []
        labels_list = []
        
        for idx in indices:
            row = self.annotations[self.annotations['sample_id'] == idx].iloc[0]
            features = row[self.feature_cols].values.astype(np.float32)
            
            if normalize:
                features = self.scaler.transform([features])[0]
            
            features_list.append(features)
            labels_list.append(self.label_to_idx[row['label']])
        
        return np.array(features_list), np.array(labels_list)
    
    def get_split(self, test_size: float = 0.2, val_size: float = 0.1, 
                  random_state: int = 42, stratify: bool = True) -> dict:
        """
        Get train/val/test split.
        
        Returns:
            {
                'train_indices': [...],
                'val_indices': [...],
                'test_indices': [...],
                'X_train': (n, 17),
                'y_train': (n,),
                'X_val': (n, 17),
                'y_val': (n,),
                'X_test': (n, 17),
                'y_test': (n,),
            }
        """
        all_indices = np.arange(len(self.annotations))
        labels = self.annotations['label'].values
        
        if stratify:
            # Stratified split
            X_temp, X_test, y_temp, y_test = train_test_split(
                all_indices, labels, test_size=test_size, 
                random_state=random_state, stratify=labels
            )
            
            val_size_adjusted = val_size / (1 - test_size)
            X_train, X_val, y_train, y_val = train_test_split(
                X_temp, y_temp, test_size=val_size_adjusted,
                random_state=random_state, stratify=y_temp
            )
        else:
            X_temp, X_test, y_temp, y_test = train_test_split(
                all_indices, labels, test_size=test_size, random_state=random_state
            )
            
            val_size_adjusted = val_size / (1 - test_size)
            X_train, X_val, y_train, y_val = train_test_split(
                X_temp, y_temp, test_size=val_size_adjusted, random_state=random_state
            )
        
        # Load features
        X_train_feat, y_train_labels = self.get_batch(X_train, normalize=True)
        X_val_feat, y_val_labels = self.get_batch(X_val, normalize=True)
        X_test_feat, y_test_labels = self.get_batch(X_test, normalize=True)
        
        return {
            'train_indices': X_train,
            'val_indices': X_val,
            'test_indices': X_test,
            'X_train': X_train_feat,
            'y_train': y_train_labels,
            'X_val': X_val_feat,
            'y_val': y_val_labels,
            'X_test': X_test_feat,
            'y_test': y_test_labels,
        }
    
    def get_by_difficulty(self, difficulty: str) -> Tuple[np.ndarray, np.ndarray]:
        """Get all samples of a specific difficulty"""
        mask = self.annotations['difficulty_level'] == difficulty
        indices = self.annotations[mask]['sample_id'].values
        
        features, labels = self.get_batch(indices, normalize=True)
        return features, labels
    
    def get_by_label(self, label: str) -> Tuple[np.ndarray, np.ndarray]:
        """Get all samples of a specific label"""
        mask = self.annotations['label'] == label
        indices = self.annotations[mask]['sample_id'].values
        
        features, labels = self.get_batch(indices, normalize=True)
        return features, labels


# ============================================================================
# Feature Analysis
# ============================================================================

def analyze_features_by_label(dataset: BulletHoleDataset) -> pd.DataFrame:
    """Analyze feature statistics grouped by label"""
    analysis = []
    
    for label in ['bullet_hole', 'non_bullet', 'ambiguous']:
        subset = dataset.annotations[dataset.annotations['label'] == label]
        
        for feature_col in dataset.feature_cols:
            stats = {
                'label': label,
                'feature': feature_col,
                'mean': subset[feature_col].mean(),
                'std': subset[feature_col].std(),
                'min': subset[feature_col].min(),
                'max': subset[feature_col].max(),
                'median': subset[feature_col].median(),
            }
            analysis.append(stats)
    
    return pd.DataFrame(analysis)


def analyze_features_by_difficulty(dataset: BulletHoleDataset) -> pd.DataFrame:
    """Analyze feature statistics grouped by difficulty"""
    analysis = []
    
    for difficulty in ['easy', 'medium', 'hard']:
        subset = dataset.annotations[dataset.annotations['difficulty_level'] == difficulty]
        
        for feature_col in dataset.feature_cols:
            stats = {
                'difficulty': difficulty,
                'feature': feature_col,
                'mean': subset[feature_col].mean(),
                'std': subset[feature_col].std(),
                'min': subset[feature_col].min(),
                'max': subset[feature_col].max(),
                'median': subset[feature_col].median(),
            }
            analysis.append(stats)
    
    return pd.DataFrame(analysis)


# ============================================================================
# Data Augmentation
# ============================================================================

def augment_features(features: np.ndarray, noise_std: float = 0.1) -> np.ndarray:
    """
    Augment feature vectors with Gaussian noise.
    
    Args:
        features: (n, 17) feature matrix
        noise_std: Standard deviation of Gaussian noise
    
    Returns:
        Augmented features
    """
    noise = np.random.normal(0, noise_std, features.shape)
    augmented = features + noise
    
    # Clip to reasonable ranges
    augmented = np.clip(augmented, -3, 3)  # Assuming normalized features
    
    return augmented


def augment_image(image: np.ndarray, rotation: bool = True, 
                 noise: bool = True, blur: bool = True) -> np.ndarray:
    """
    Augment image with transformations.
    
    Args:
        image: Single 256x256 grayscale image
        rotation: Apply small rotation
        noise: Add Gaussian noise
        blur: Apply slight blur
    
    Returns:
        Augmented image
    """
    img = image.copy().astype(np.float32)
    
    # Rotation
    if rotation and np.random.random() < 0.5:
        angle = np.random.uniform(-5, 5)
        h, w = img.shape
        M = cv2.getRotationMatrix2D((w/2, h/2), angle, 1.0)
        img = cv2.warpAffine(img, M, (w, h))
    
    # Gaussian noise
    if noise and np.random.random() < 0.5:
        noise_level = np.random.uniform(0.5, 2.0)
        img += np.random.normal(0, noise_level, img.shape)
    
    # Blur
    if blur and np.random.random() < 0.5:
        kernel_size = np.random.choice([3, 5, 7])
        img = cv2.GaussianBlur(img, (kernel_size, kernel_size), 0)
    
    img = np.clip(img, 0, 255).astype(np.uint8)
    
    return img


# ============================================================================
# Example Usage and Model Training
# ============================================================================

def example_loading():
    """Example: Load and inspect dataset"""
    print("=" * 70)
    print("EXAMPLE 1: Loading Dataset")
    print("=" * 70)
    
    dataset = BulletHoleDataset()
    
    print(f"Total samples: {len(dataset.annotations)}")
    print(f"Features: {len(dataset.feature_cols)}")
    print(f"Labels: {list(dataset.label_to_idx.keys())}")
    
    # Load single sample
    sample_id = 0
    image, label, features = dataset.get_sample(sample_id)
    
    print(f"\nSample {sample_id}:")
    print(f"  Image shape: {image.shape}")
    print(f"  Label: {label}")
    print(f"  Features shape: {features.shape}")
    print(f"  First 3 features: {features[:3]}")


def example_train_test_split():
    """Example: Create train/validation/test splits"""
    print("\n" + "=" * 70)
    print("EXAMPLE 2: Train/Validation/Test Split")
    print("=" * 70)
    
    dataset = BulletHoleDataset()
    split = dataset.get_split(test_size=0.2, val_size=0.1)
    
    print(f"Training samples: {len(split['X_train'])}")
    print(f"Validation samples: {len(split['X_val'])}")
    print(f"Test samples: {len(split['X_test'])}")
    
    print(f"\nTraining labels distribution:")
    unique, counts = np.unique(split['y_train'], return_counts=True)
    for label_idx, count in zip(unique, counts):
        print(f"  {dataset.idx_to_label[label_idx]}: {count}")


def example_naive_bayes():
    """Example: Train Naive Bayes classifier"""
    print("\n" + "=" * 70)
    print("EXAMPLE 3: Naive Bayes Classifier")
    print("=" * 70)
    
    from sklearn.naive_bayes import GaussianNB
    from sklearn.metrics import accuracy_score, precision_recall_fscore_support
    
    # Load dataset
    dataset = BulletHoleDataset()
    split = dataset.get_split(test_size=0.2, val_size=0.1)
    
    # Train
    clf = GaussianNB()
    clf.fit(split['X_train'], split['y_train'])
    
    # Evaluate
    y_pred_train = clf.predict(split['X_train'])
    y_pred_val = clf.predict(split['X_val'])
    y_pred_test = clf.predict(split['X_test'])
    
    acc_train = accuracy_score(split['y_train'], y_pred_train)
    acc_val = accuracy_score(split['y_val'], y_pred_val)
    acc_test = accuracy_score(split['y_test'], y_pred_test)
    
    print(f"Training accuracy: {acc_train:.4f}")
    print(f"Validation accuracy: {acc_val:.4f}")
    print(f"Test accuracy: {acc_test:.4f}")
    
    # Per-class metrics
    precision, recall, f1, _ = precision_recall_fscore_support(
        split['y_test'], y_pred_test, average=None
    )
    
    print(f"\nPer-class F1 scores:")
    for label_idx, f1_score in enumerate(f1):
        print(f"  {dataset.idx_to_label[label_idx]}: {f1_score:.4f}")


def example_gmm():
    """Example: Train Gaussian Mixture Model"""
    print("\n" + "=" * 70)
    print("EXAMPLE 4: Gaussian Mixture Model")
    print("=" * 70)
    
    from sklearn.mixture import GaussianMixture
    from sklearn.metrics import accuracy_score, adjusted_rand_score
    
    # Load dataset
    dataset = BulletHoleDataset()
    split = dataset.get_split(test_size=0.2, val_size=0.1)
    
    # Train
    gmm = GaussianMixture(n_components=3, random_state=42, n_init=10)
    gmm.fit(split['X_train'])
    
    # Evaluate
    y_pred_train = gmm.predict(split['X_train'])
    y_pred_val = gmm.predict(split['X_val'])
    y_pred_test = gmm.predict(split['X_test'])
    
    # Note: GMM predictions don't directly map to class labels
    # Using ARI (Adjusted Rand Index) for unsupervised evaluation
    ari_test = adjusted_rand_score(split['y_test'], y_pred_test)
    
    print(f"Test Adjusted Rand Index: {ari_test:.4f}")
    print(f"Model log-likelihood (test): {gmm.score(split['X_test']):.4f}")


def example_mlp():
    """Example: Train simple neural network"""
    print("\n" + "=" * 70)
    print("EXAMPLE 5: Multi-Layer Perceptron")
    print("=" * 70)
    
    try:
        from sklearn.neural_network import MLPClassifier
        from sklearn.metrics import accuracy_score, classification_report
    except ImportError:
        print("Requires scikit-learn with neural_network module")
        return
    
    # Load dataset
    dataset = BulletHoleDataset()
    split = dataset.get_split(test_size=0.2, val_size=0.1)
    
    # Train
    mlp = MLPClassifier(
        hidden_layer_sizes=(64, 32),
        max_iter=1000,
        random_state=42,
        early_stopping=True,
        validation_fraction=0.1,
        verbose=False
    )
    mlp.fit(split['X_train'], split['y_train'])
    
    # Evaluate
    y_pred_train = mlp.predict(split['X_train'])
    y_pred_val = mlp.predict(split['X_val'])
    y_pred_test = mlp.predict(split['X_test'])
    
    acc_train = accuracy_score(split['y_train'], y_pred_train)
    acc_val = accuracy_score(split['y_val'], y_pred_val)
    acc_test = accuracy_score(split['y_test'], y_pred_test)
    
    print(f"Training accuracy: {acc_train:.4f}")
    print(f"Validation accuracy: {acc_val:.4f}")
    print(f"Test accuracy: {acc_test:.4f}")
    
    print(f"\nTest classification report:")
    print(classification_report(
        split['y_test'], y_pred_test,
        target_names=list(dataset.idx_to_label.values())
    ))


def example_feature_analysis():
    """Example: Analyze features"""
    print("\n" + "=" * 70)
    print("EXAMPLE 6: Feature Analysis")
    print("=" * 70)
    
    dataset = BulletHoleDataset()
    
    # By label
    print("\nFeature statistics by label (first 5):")
    analysis_label = analyze_features_by_label(dataset)
    print(analysis_label[analysis_label['label'] == 'bullet_hole'].head())
    
    # By difficulty
    print("\nFeature statistics by difficulty (first 5):")
    analysis_diff = analyze_features_by_difficulty(dataset)
    print(analysis_diff[analysis_diff['difficulty'] == 'hard'].head())


# ============================================================================
# Main
# ============================================================================

if __name__ == '__main__':
    example_loading()
    example_train_test_split()
    example_feature_analysis()
    
    print("\n" + "=" * 70)
    print("Additional examples (uncomment to run):")
    print("=" * 70)
    print("- example_naive_bayes()")
    print("- example_gmm()")
    print("- example_mlp()")
    
    # Uncomment to run:
    # example_naive_bayes()
    # example_gmm()
    # example_mlp()
