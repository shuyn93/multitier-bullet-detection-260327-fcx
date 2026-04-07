#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Multi-Tier Bullet Hole Detection Training System
Trains Tier 1, Tier 2, and Tier 3 models on synthetic IR dataset
"""

import numpy as np
import pandas as pd
import cv2
from pathlib import Path
import pickle
import time
from sklearn.model_selection import train_test_split
from sklearn.preprocessing import StandardScaler
from sklearn.naive_bayes import GaussianNB
from sklearn.mixture import GaussianMixture
from sklearn.ensemble import RandomForestClassifier, GradientBoostingClassifier
from sklearn.metrics import (accuracy_score, precision_score, recall_score, 
                            f1_score, confusion_matrix, classification_report)
import tensorflow as tf
from tensorflow import keras
from tensorflow.keras import layers, Sequential
import matplotlib.pyplot as plt
import seaborn as sns

# Configuration
DATASET_DIR = Path("dataset_ir_realistic")
RESULTS_DIR = Path("results")
MODELS_DIR = Path("models")

RESULTS_DIR.mkdir(parents=True, exist_ok=True)
MODELS_DIR.mkdir(parents=True, exist_ok=True)
(MODELS_DIR / "tier1").mkdir(parents=True, exist_ok=True)
(MODELS_DIR / "tier2").mkdir(parents=True, exist_ok=True)

# Random seed for reproducibility
SEED = 42
np.random.seed(SEED)
tf.random.set_seed(SEED)

# ============================================================================
# TASK 1: DATA LOADING
# ============================================================================

def load_data():
    """Load and prepare dataset"""
    print("=" * 80)
    print("TASK 1: DATA LOADING")
    print("=" * 80)
    
    # Load CSV
    csv_path = DATASET_DIR / "annotations.csv"
    print(f"\nLoading: {csv_path}")
    df = pd.read_csv(csv_path)
    
    print(f"Total samples: {len(df)}")
    print(f"Labels distribution:")
    print(df['label'].value_counts())
    
    # Feature columns (all numeric except filename, label, difficulty)
    feature_cols = [col for col in df.columns if col not in 
                   ['filename', 'label', 'difficulty']]
    
    print(f"\nFeatures: {len(feature_cols)}")
    print(f"Feature list: {feature_cols}")
    
    # Extract X (features) and y (labels)
    X = df[feature_cols].values.astype(np.float32)
    y = df['label'].values  # -1, 0, 1
    
    # Map labels to 0, 1, 2 for multi-class
    label_map = {-1: 2, 0: 1, 1: 0}  # ambiguous=2, non_bullet=1, bullet=0
    y_mapped = np.array([label_map[label] for label in y])
    
    print(f"\nFeature matrix shape: {X.shape}")
    print(f"Label distribution (mapped):")
    unique, counts = np.unique(y_mapped, return_counts=True)
    for label, count in zip(unique, counts):
        label_names = {0: "Bullet Hole", 1: "Non-Bullet", 2: "Ambiguous"}
        print(f"  {label_names[label]}: {count}")
    
    return X, y_mapped, feature_cols

# ============================================================================
# TASK 2: TRAIN/TEST SPLIT
# ============================================================================

def split_data(X, y):
    """Split data into train/val/test"""
    print("\n" + "=" * 80)
    print("TASK 2: TRAIN/TEST SPLIT")
    print("=" * 80)
    
    # First split: 70% train, 30% temp (for val+test)
    X_train, X_temp, y_train, y_temp = train_test_split(
        X, y, test_size=0.30, random_state=SEED, stratify=y
    )
    
    # Second split: 50% val, 50% test from temp
    X_val, X_test, y_val, y_test = train_test_split(
        X_temp, y_temp, test_size=0.50, random_state=SEED, stratify=y_temp
    )
    
    print(f"\nTrain samples: {len(X_train)} (70%)")
    print(f"Val samples: {len(X_val)} (15%)")
    print(f"Test samples: {len(X_test)} (15%)")
    
    return (X_train, y_train), (X_val, y_val), (X_test, y_test)

# ============================================================================
# TASK 3: FEATURE NORMALIZATION
# ============================================================================

def normalize_features(X_train, X_val, X_test):
    """Normalize features using standard scaling"""
    print("\n" + "=" * 80)
    print("TASK 3: FEATURE NORMALIZATION")
    print("=" * 80)
    
    scaler = StandardScaler()
    
    # Fit on training data
    X_train_norm = scaler.fit_transform(X_train)
    X_val_norm = scaler.transform(X_val)
    X_test_norm = scaler.transform(X_test)
    
    print(f"\nScaling parameters saved")
    print(f"Mean (first 5 features): {scaler.mean_[:5]}")
    print(f"Std (first 5 features): {scaler.scale_[:5]}")
    
    # Save scaler
    with open(MODELS_DIR / "scaler.pkl", "wb") as f:
        pickle.dump(scaler, f)
    
    return X_train_norm, X_val_norm, X_test_norm, scaler

# ============================================================================
# TASK 4: TIER 1 TRAINING
# ============================================================================

def train_tier1(X_train, y_train, X_val, y_val):
    """Train Tier 1 models (fast classifiers)"""
    print("\n" + "=" * 80)
    print("TASK 4: TIER 1 TRAINING")
    print("=" * 80)
    
    models = {}
    predictions = {}
    
    # 1. Naive Bayes
    print("\n[1/3] Training Gaussian Naive Bayes...")
    nb_model = GaussianNB()
    nb_model.fit(X_train, y_train)
    
    y_pred_nb = nb_model.predict(X_val)
    y_pred_proba_nb = nb_model.predict_proba(X_val)
    
    nb_accuracy = accuracy_score(y_val, y_pred_nb)
    nb_recall = recall_score(y_val, y_pred_nb, average='weighted', zero_division=0)
    
    print(f"  Accuracy: {nb_accuracy:.4f}")
    print(f"  Recall: {nb_recall:.4f}")
    
    models['naive_bayes'] = nb_model
    predictions['naive_bayes'] = y_pred_proba_nb
    
    # 2. Gaussian Mixture Model
    print("\n[2/3] Training Gaussian Mixture Model...")
    gmm_model = GaussianMixture(n_components=2, random_state=SEED)
    gmm_model.fit(X_train)
    
    # Predict with GMM (labels not directly available, use scoring)
    gmm_scores = gmm_model.score_samples(X_val)
    
    # Train a wrapper classifier
    gmm_wrapper = GaussianNB()
    gmm_wrapper.fit(X_train, y_train)
    
    y_pred_gmm = gmm_wrapper.predict(X_val)
    y_pred_proba_gmm = gmm_wrapper.predict_proba(X_val)
    
    gmm_accuracy = accuracy_score(y_val, y_pred_gmm)
    gmm_recall = recall_score(y_val, y_pred_gmm, average='weighted', zero_division=0)
    
    print(f"  Accuracy: {gmm_accuracy:.4f}")
    print(f"  Recall: {gmm_recall:.4f}")
    
    models['gmm'] = gmm_model
    predictions['gmm'] = y_pred_proba_gmm
    
    # 3. Random Forest
    print("\n[3/3] Training Random Forest Ensemble...")
    rf_model = RandomForestClassifier(
        n_estimators=100,
        max_depth=10,
        random_state=SEED,
        n_jobs=-1
    )
    rf_model.fit(X_train, y_train)
    
    y_pred_rf = rf_model.predict(X_val)
    y_pred_proba_rf = rf_model.predict_proba(X_val)
    
    rf_accuracy = accuracy_score(y_val, y_pred_rf)
    rf_recall = recall_score(y_val, y_pred_rf, average='weighted', zero_division=0)
    
    print(f"  Accuracy: {rf_accuracy:.4f}")
    print(f"  Recall: {rf_recall:.4f}")
    
    models['random_forest'] = rf_model
    predictions['random_forest'] = y_pred_proba_rf
    
    # Ensemble voting
    print("\n[Ensemble] Combining models...")
    ensemble_proba = (y_pred_proba_nb + y_pred_proba_gmm + y_pred_proba_rf) / 3
    y_pred_ensemble = np.argmax(ensemble_proba, axis=1)
    
    ensemble_accuracy = accuracy_score(y_val, y_pred_ensemble)
    ensemble_recall = recall_score(y_val, y_pred_ensemble, average='weighted', zero_division=0)
    
    print(f"  Accuracy: {ensemble_accuracy:.4f}")
    print(f"  Recall: {ensemble_recall:.4f}")
    
    # Save models
    with open(MODELS_DIR / "tier1" / "naive_bayes.pkl", "wb") as f:
        pickle.dump(nb_model, f)
    with open(MODELS_DIR / "tier1" / "gmm.pkl", "wb") as f:
        pickle.dump(gmm_model, f)
    with open(MODELS_DIR / "tier1" / "random_forest.pkl", "wb") as f:
        pickle.dump(rf_model, f)
    
    print("\nTier 1 models saved")
    
    return models, ensemble_proba

# ============================================================================
# TASK 5: TIER 2 TRAINING (MLP)
# ============================================================================

def train_tier2(X_train, y_train, X_val, y_val):
    """Train Tier 2 neural network"""
    print("\n" + "=" * 80)
    print("TASK 5: TIER 2 TRAINING (MLP)")
    print("=" * 80)
    
    num_features = X_train.shape[1]
    
    # Build MLP model
    model = Sequential([
        layers.Dense(64, activation='relu', input_dim=num_features),
        layers.Dropout(0.2),
        layers.Dense(32, activation='relu'),
        layers.Dropout(0.2),
        layers.Dense(3, activation='softmax')
    ])
    
    model.compile(
        optimizer='adam',
        loss='sparse_categorical_crossentropy',
        metrics=['accuracy']
    )
    
    print(f"\nModel architecture:")
    model.summary()
    
    # Train
    print(f"\nTraining...")
    history = model.fit(
        X_train, y_train,
        validation_data=(X_val, y_val),
        epochs=50,
        batch_size=32,
        verbose=1
    )
    
    # Evaluate
    val_loss, val_accuracy = model.evaluate(X_val, y_val, verbose=0)
    print(f"\nValidation - Loss: {val_loss:.4f}, Accuracy: {val_accuracy:.4f}")
    
    # Save model
    model.save(str(MODELS_DIR / "tier2" / "mlp_model.h5"))
    
    print("Tier 2 model saved")
    
    return model, history

# ============================================================================
# TASK 6: TIER 3 RULES
# ============================================================================

class Tier3Validator:
    """Physics-based validation rules"""
    
    def __init__(self):
        self.circularity_min = 0.55
        self.intensity_min = 30
        self.intensity_max = 110
        self.size_min = 200
        self.size_max = 4000
    
    def validate(self, features):
        """Validate sample based on physics rules"""
        circularity = features[1]  # index of circularity
        mean_intensity = features[3]  # index of mean_intensity
        area = features[0]  # index of area
        
        confidence = 1.0
        
        # Circularity check
        if circularity < self.circularity_min:
            confidence *= 0.7
        
        # Intensity check
        if mean_intensity < self.intensity_min or mean_intensity > self.intensity_max:
            confidence *= 0.6
        
        # Size check
        if area < self.size_min or area > self.size_max:
            confidence *= 0.8
        
        return confidence

# ============================================================================
# TASK 7: EVALUATION
# ============================================================================

def evaluate_models(models, tier2_model, X_train, y_train, X_val, y_val, X_test, y_test, ensemble_proba):
    """Evaluate all tiers"""
    print("\n" + "=" * 80)
    print("TASK 7: EVALUATION")
    print("=" * 80)
    
    results = {}
    
    # Tier 1 predictions
    print("\n[Tier 1] Ensemble voting predictions...")
    y_pred_tier1 = np.argmax(ensemble_proba, axis=1)
    
    tier1_metrics = {
        'accuracy': accuracy_score(y_val, y_pred_tier1),
        'precision': precision_score(y_val, y_pred_tier1, average='weighted', zero_division=0),
        'recall': recall_score(y_val, y_pred_tier1, average='weighted', zero_division=0),
        'f1': f1_score(y_val, y_pred_tier1, average='weighted', zero_division=0)
    }
    
    print(f"  Accuracy: {tier1_metrics['accuracy']:.4f}")
    print(f"  Precision: {tier1_metrics['precision']:.4f}")
    print(f"  Recall: {tier1_metrics['recall']:.4f}")
    print(f"  F1-Score: {tier1_metrics['f1']:.4f}")
    
    results['tier1'] = tier1_metrics
    
    # Tier 2 predictions
    print("\n[Tier 2] MLP predictions...")
    y_pred_tier2_logits = tier2_model.predict(X_val, verbose=0)
    y_pred_tier2 = np.argmax(y_pred_tier2_logits, axis=1)
    
    tier2_metrics = {
        'accuracy': accuracy_score(y_val, y_pred_tier2),
        'precision': precision_score(y_val, y_pred_tier2, average='weighted', zero_division=0),
        'recall': recall_score(y_val, y_pred_tier2, average='weighted', zero_division=0),
        'f1': f1_score(y_val, y_pred_tier2, average='weighted', zero_division=0)
    }
    
    print(f"  Accuracy: {tier2_metrics['accuracy']:.4f}")
    print(f"  Precision: {tier2_metrics['precision']:.4f}")
    print(f"  Recall: {tier2_metrics['recall']:.4f}")
    print(f"  F1-Score: {tier2_metrics['f1']:.4f}")
    
    results['tier2'] = tier2_metrics
    
    # Full evaluation on test set
    print("\n[TEST SET] Final evaluation...")
    y_pred_test_tier1 = models['random_forest'].predict(X_test)
    y_pred_test_tier2 = np.argmax(tier2_model.predict(X_test, verbose=0), axis=1)
    
    test_metrics_tier1 = {
        'accuracy': accuracy_score(y_test, y_pred_test_tier1),
        'precision': precision_score(y_test, y_pred_test_tier1, average='weighted', zero_division=0),
        'recall': recall_score(y_test, y_pred_test_tier1, average='weighted', zero_division=0),
        'f1': f1_score(y_test, y_pred_test_tier1, average='weighted', zero_division=0)
    }
    
    test_metrics_tier2 = {
        'accuracy': accuracy_score(y_test, y_pred_test_tier2),
        'precision': precision_score(y_test, y_pred_test_tier2, average='weighted', zero_division=0),
        'recall': recall_score(y_test, y_pred_test_tier2, average='weighted', zero_division=0),
        'f1': f1_score(y_test, y_pred_test_tier2, average='weighted', zero_division=0)
    }
    
    print("\nTier 1 (Test Set):")
    for key, val in test_metrics_tier1.items():
        print(f"  {key.upper()}: {val:.4f}")
    
    print("\nTier 2 (Test Set):")
    for key, val in test_metrics_tier2.items():
        print(f"  {key.upper()}: {val:.4f}")
    
    # Confusion matrices
    cm_tier1 = confusion_matrix(y_val, y_pred_tier1)
    cm_tier2 = confusion_matrix(y_val, y_pred_tier2)
    
    return results, {
        'tier1': test_metrics_tier1,
        'tier2': test_metrics_tier2
    }, (cm_tier1, cm_tier2)

# ============================================================================
# TASK 8: LOGGING & REPORTING
# ============================================================================

def save_report(results, test_results, confusion_matrices):
    """Save results to file"""
    print("\n" + "=" * 80)
    print("TASK 8: LOGGING & REPORTING")
    print("=" * 80)
    
    report_path = RESULTS_DIR / "report.txt"
    
    with open(report_path, "w") as f:
        f.write("=" * 80 + "\n")
        f.write("MULTI-TIER BULLET HOLE DETECTION TRAINING REPORT\n")
        f.write("=" * 80 + "\n\n")
        
        f.write("VALIDATION SET RESULTS:\n")
        f.write("-" * 80 + "\n")
        
        f.write("\nTier 1 (Ensemble):\n")
        for key, val in results['tier1'].items():
            f.write(f"  {key.upper()}: {val:.4f}\n")
        
        f.write("\nTier 2 (MLP):\n")
        for key, val in results['tier2'].items():
            f.write(f"  {key.upper()}: {val:.4f}\n")
        
        f.write("\n\nTEST SET RESULTS:\n")
        f.write("-" * 80 + "\n")
        
        f.write("\nTier 1 (Random Forest):\n")
        for key, val in test_results['tier1'].items():
            f.write(f"  {key.upper()}: {val:.4f}\n")
        
        f.write("\nTier 2 (MLP):\n")
        for key, val in test_results['tier2'].items():
            f.write(f"  {key.upper()}: {val:.4f}\n")
        
        f.write("\n" + "=" * 80 + "\n")
    
    print(f"\nReport saved: {report_path}")

# ============================================================================
# TASK 9: VISUALIZATIONS
# ============================================================================

def plot_confusion_matrices(confusion_matrices):
    """Plot confusion matrices"""
    cm_tier1, cm_tier2 = confusion_matrices
    
    fig, axes = plt.subplots(1, 2, figsize=(12, 5))
    
    label_names = ['Bullet Hole', 'Non-Bullet', 'Ambiguous']
    
    # Tier 1
    sns.heatmap(cm_tier1, annot=True, fmt='d', cmap='Blues',
               xticklabels=label_names, yticklabels=label_names,
               ax=axes[0])
    axes[0].set_title('Tier 1 (Ensemble) Confusion Matrix')
    axes[0].set_ylabel('True Label')
    axes[0].set_xlabel('Predicted Label')
    
    # Tier 2
    sns.heatmap(cm_tier2, annot=True, fmt='d', cmap='Greens',
               xticklabels=label_names, yticklabels=label_names,
               ax=axes[1])
    axes[1].set_title('Tier 2 (MLP) Confusion Matrix')
    axes[1].set_ylabel('True Label')
    axes[1].set_xlabel('Predicted Label')
    
    plt.tight_layout()
    plt.savefig(str(RESULTS_DIR / "confusion_matrices.png"), dpi=150)
    print(f"\nConfusion matrices saved: {RESULTS_DIR / 'confusion_matrices.png'}")

# ============================================================================
# MAIN TRAINING PIPELINE
# ============================================================================

def main():
    """Main training pipeline"""
    print("\n" + "=" * 80)
    print("MULTI-TIER BULLET HOLE DETECTION TRAINING")
    print("=" * 80)
    
    start_time = time.time()
    
    # Load data
    X, y, feature_cols = load_data()
    
    # Split
    (X_train, y_train), (X_val, y_val), (X_test, y_test) = split_data(X, y)
    
    # Normalize
    X_train_norm, X_val_norm, X_test_norm, scaler = normalize_features(
        X_train, X_val, X_test
    )
    
    # Train Tier 1
    models, ensemble_proba = train_tier1(X_train_norm, y_train, X_val_norm, y_val)
    
    # Train Tier 2
    tier2_model, history = train_tier2(X_train_norm, y_train, X_val_norm, y_val)
    
    # Evaluate
    results, test_results, confusion_matrices = evaluate_models(
        models, tier2_model, X_train_norm, y_train, X_val_norm, y_val,
        X_test_norm, y_test, ensemble_proba
    )
    
    # Report
    save_report(results, test_results, confusion_matrices)
    
    # Plot
    plot_confusion_matrices(confusion_matrices)
    
    elapsed = time.time() - start_time
    print(f"\n\nTraining completed in {elapsed:.1f} seconds")
    print(f"Results saved to: {RESULTS_DIR}")
    print(f"Models saved to: {MODELS_DIR}")

if __name__ == "__main__":
    main()
