#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Multi-Tier Bullet Hole Detection System - Training Pipeline
Trains Tier 1 (classical models), Tier 2 (neural network), and Tier 3 (physics validation)
"""

import numpy as np
import pandas as pd
import cv2
from pathlib import Path
import json
import pickle
import time
from sklearn.model_selection import train_test_split
from sklearn.preprocessing import StandardScaler
from sklearn.naive_bayes import GaussianNB
from sklearn.mixture import GaussianMixture
from sklearn.ensemble import RandomForestClassifier
from sklearn.metrics import accuracy_score, precision_score, recall_score, f1_score, confusion_matrix
import warnings
warnings.filterwarnings('ignore')

# For neural networks
try:
    import tensorflow as tf
    from tensorflow import keras
    from tensorflow.keras import layers
    TENSORFLOW_AVAILABLE = True
except ImportError:
    TENSORFLOW_AVAILABLE = False
    print("Warning: TensorFlow not available. Will skip Tier 2 training.")

# ============ Configuration ============
DATASET_DIR = Path("dataset_ir_realistic")
IMAGES_DIR = DATASET_DIR / "images"
ANNOTATIONS_FILE = DATASET_DIR / "annotations.csv"

MODELS_DIR = Path("models")
TIER1_DIR = MODELS_DIR / "tier1"
TIER2_DIR = MODELS_DIR / "tier2"
RESULTS_DIR = Path("results")

RESULTS_DIR.mkdir(parents=True, exist_ok=True)
TIER1_DIR.mkdir(parents=True, exist_ok=True)
TIER2_DIR.mkdir(parents=True, exist_ok=True)

# Random seed for reproducibility
RANDOM_SEED = 42
np.random.seed(RANDOM_SEED)
tf.random.set_seed(RANDOM_SEED) if TENSORFLOW_AVAILABLE else None

# Feature columns
FEATURE_COLS = [
    'area', 'circularity', 'solidity', 'mean_intensity', 'std_intensity',
    'edge_density', 'aspect_ratio', 'center_x', 'center_y', 'radius'
]

# ============ Step 1: DATA LOADING ============

def load_dataset():
    """Load annotations and prepare feature matrix"""
    print("\n" + "="*70)
    print("STEP 1: LOADING DATASET")
    print("="*70)
    
    if not ANNOTATIONS_FILE.exists():
        print(f"ERROR: {ANNOTATIONS_FILE} not found!")
        print("Please generate the dataset using scripts/generate_ir_dataset_v4.py")
        return None, None, None
    
    df = pd.read_csv(ANNOTATIONS_FILE)
    print(f"Loaded {len(df)} samples")
    
    # Extract features
    X = df[FEATURE_COLS].values.astype(np.float32)
    
    # Map labels: 1 -> 0 (bullet), 0 -> 1 (non-bullet), -1 -> 2 (ambiguous)
    label_map = {1: 0, 0: 1, -1: 2}
    y = df['label'].map(label_map).values
    
    print(f"\nFeature matrix shape: {X.shape}")
    print(f"Label distribution:")
    unique, counts = np.unique(y, return_counts=True)
    for label_id, count in zip(unique, counts):
        label_name = ['Bullet Hole', 'Non-Bullet', 'Ambiguous'][label_id]
        print(f"  {label_name}: {count} ({count/len(y)*100:.1f}%)")
    
    return X, y, df

def normalize_features(X_train, X_val, X_test):
    """Normalize features using StandardScaler"""
    print("\nNormalizing features...")
    scaler = StandardScaler()
    X_train_scaled = scaler.fit_transform(X_train)
    X_val_scaled = scaler.transform(X_val)
    X_test_scaled = scaler.transform(X_test)
    
    # Save scaler
    with open(TIER1_DIR / "scaler.pkl", "wb") as f:
        pickle.dump(scaler, f)
    
    print(f"Scaler saved to {TIER1_DIR / 'scaler.pkl'}")
    
    return X_train_scaled, X_val_scaled, X_test_scaled, scaler

# ============ Step 2: TRAIN/TEST SPLIT ============

def split_dataset(X, y):
    """Stratified train/val/test split"""
    print("\n" + "="*70)
    print("STEP 2: TRAIN/VALIDATION/TEST SPLIT")
    print("="*70)
    
    # 70% train, 30% temp
    X_train, X_temp, y_train, y_temp = train_test_split(
        X, y, test_size=0.30, random_state=RANDOM_SEED, stratify=y
    )
    
    # 50-50 split of temp into val and test
    X_val, X_test, y_val, y_test = train_test_split(
        X_temp, y_temp, test_size=0.50, random_state=RANDOM_SEED, stratify=y_temp
    )
    
    print(f"Training set: {len(X_train)} samples ({len(X_train)/len(X)*100:.1f}%)")
    print(f"Validation set: {len(X_val)} samples ({len(X_val)/len(X)*100:.1f}%)")
    print(f"Test set: {len(X_test)} samples ({len(X_test)/len(X)*100:.1f}%)")
    
    return X_train, X_val, X_test, y_train, y_val, y_test

# ============ Step 3: TIER 1 TRAINING ============

def train_tier1(X_train, X_val, y_train, y_val, X_train_scaled, X_val_scaled):
    """Train Tier 1 models: Naive Bayes, GMM, Random Forest"""
    print("\n" + "="*70)
    print("STEP 3: TIER 1 TRAINING (Fast Classical Models)")
    print("="*70)
    
    models = {}
    
    # 1. Gaussian Naive Bayes
    print("\n[1/3] Training Gaussian Naive Bayes...")
    start = time.time()
    nb_model = GaussianNB()
    nb_model.fit(X_train_scaled, y_train)
    nb_time = time.time() - start
    
    y_pred_nb = nb_model.predict(X_val_scaled)
    nb_acc = accuracy_score(y_val, y_pred_nb)
    nb_recall = recall_score(y_val, y_pred_nb, average='weighted', zero_division=0)
    
    print(f"  Training time: {nb_time:.3f}s")
    print(f"  Validation accuracy: {nb_acc:.4f}")
    print(f"  Validation recall: {nb_recall:.4f}")
    
    models['naive_bayes'] = {
        'model': nb_model,
        'accuracy': nb_acc,
        'recall': nb_recall,
        'time': nb_time
    }
    
    # 2. Gaussian Mixture Model (unsupervised + label mapping)
    print("\n[2/3] Training Gaussian Mixture Model...")
    start = time.time()
    gmm_model = GaussianMixture(n_components=3, random_state=RANDOM_SEED)
    gmm_model.fit(X_train_scaled)
    gmm_time = time.time() - start
    
    # Map GMM clusters to labels
    gmm_clusters = gmm_model.predict(X_train_scaled)
    cluster_to_label = np.zeros(3, dtype=int)
    for cluster_id in range(3):
        mask = gmm_clusters == cluster_id
        if mask.sum() > 0:
            most_common_label = np.bincount(y_train[mask]).argmax()
            cluster_to_label[cluster_id] = most_common_label
    
    y_pred_gmm = cluster_to_label[gmm_model.predict(X_val_scaled)]
    gmm_acc = accuracy_score(y_val, y_pred_gmm)
    gmm_recall = recall_score(y_val, y_pred_gmm, average='weighted', zero_division=0)
    
    print(f"  Training time: {gmm_time:.3f}s")
    print(f"  Validation accuracy: {gmm_acc:.4f}")
    print(f"  Validation recall: {gmm_recall:.4f}")
    
    models['gmm'] = {
        'model': gmm_model,
        'cluster_to_label': cluster_to_label,
        'accuracy': gmm_acc,
        'recall': gmm_recall,
        'time': gmm_time
    }
    
    # 3. Random Forest
    print("\n[3/3] Training Random Forest...")
    start = time.time()
    rf_model = RandomForestClassifier(
        n_estimators=100, max_depth=10, random_state=RANDOM_SEED, n_jobs=-1
    )
    rf_model.fit(X_train_scaled, y_train)
    rf_time = time.time() - start
    
    y_pred_rf = rf_model.predict(X_val_scaled)
    rf_acc = accuracy_score(y_val, y_pred_rf)
    rf_recall = recall_score(y_val, y_pred_rf, average='weighted', zero_division=0)
    
    print(f"  Training time: {rf_time:.3f}s")
    print(f"  Validation accuracy: {rf_acc:.4f}")
    print(f"  Validation recall: {rf_recall:.4f}")
    
    models['random_forest'] = {
        'model': rf_model,
        'accuracy': rf_acc,
        'recall': rf_recall,
        'time': rf_time
    }
    
    # Save models
    for name, model_info in models.items():
        model_path = TIER1_DIR / f"{name}.pkl"
        with open(model_path, "wb") as f:
            pickle.dump(model_info, f)
        print(f"Saved {name} to {model_path}")
    
    return models

def tier1_ensemble_predict(models, X):
    """Ensemble prediction from Tier 1 models"""
    nb_pred = models['naive_bayes']['model'].predict(X)
    
    gmm_clusters = models['gmm']['model'].predict(X)
    gmm_pred = models['gmm']['cluster_to_label'][gmm_clusters]
    
    rf_pred = models['random_forest']['model'].predict(X)
    
    # Soft voting: average predictions
    predictions = np.stack([nb_pred, gmm_pred, rf_pred], axis=1)
    ensemble_pred = np.argmax(np.bincount(predictions, axis=0), axis=1)
    
    # Get confidences
    confidences = np.max(np.bincount(predictions, axis=0) / 3.0, axis=1)
    
    return ensemble_pred, confidences

# ============ Step 4: TIER 2 TRAINING ============

def build_tier2_model(input_dim=10):
    """Build MLP for Tier 2 refinement"""
    if not TENSORFLOW_AVAILABLE:
        return None
    
    model = keras.Sequential([
        layers.Dense(64, activation='relu', input_dim=input_dim),
        layers.Dropout(0.2),
        layers.Dense(32, activation='relu'),
        layers.Dropout(0.2),
        layers.Dense(3, activation='softmax')
    ])
    
    model.compile(
        optimizer=keras.optimizers.Adam(learning_rate=0.001),
        loss='sparse_categorical_crossentropy',
        metrics=['accuracy']
    )
    
    return model

def train_tier2(X_train, X_val, X_test, y_train, y_val, y_test):
    """Train Tier 2 MLP on validation samples"""
    if not TENSORFLOW_AVAILABLE:
        print("\nWARNING: TensorFlow not available. Skipping Tier 2 training.")
        return None
    
    print("\n" + "="*70)
    print("STEP 4: TIER 2 TRAINING (Neural Network Refinement)")
    print("="*70)
    
    print("\nBuilding MLP architecture...")
    model = build_tier2_model(input_dim=X_train.shape[1])
    model.summary()
    
    print("\nTraining Tier 2 model...")
    history = model.fit(
        X_train, y_train,
        validation_data=(X_val, y_val),
        epochs=100,
        batch_size=32,
        verbose=0,
        callbacks=[
            keras.callbacks.EarlyStopping(monitor='val_loss', patience=10, restore_best_weights=True)
        ]
    )
    
    print(f"Training complete. Final validation accuracy: {history.history['val_accuracy'][-1]:.4f}")
    
    # Evaluate on test set
    test_loss, test_acc = model.evaluate(X_test, y_test, verbose=0)
    print(f"Test set accuracy: {test_acc:.4f}")
    
    # Save model
    model.save(TIER2_DIR / "tier2_model.h5")
    print(f"Saved Tier 2 model to {TIER2_DIR / 'tier2_model.h5'}")
    
    return model

# ============ Step 5: TIER 3 (Physics Validation) ============

def tier3_validate(predictions, features_df):
    """Physics-based validation rules"""
    print("\n" + "="*70)
    print("STEP 5: TIER 3 PHYSICS VALIDATION")
    print("="*70)
    
    # Rule-based validation
    valid_mask = np.ones(len(predictions), dtype=bool)
    
    # Rule 1: Circularity threshold
    invalid_circularity = features_df['circularity'].values < 0.5
    valid_mask &= ~invalid_circularity
    
    # Rule 2: Hole center must be within image bounds
    invalid_position = (features_df['center_x'].values < 20) | (features_df['center_x'].values > 236) | \
                      (features_df['center_y'].values < 20) | (features_df['center_y'].values > 236)
    valid_mask &= ~invalid_position
    
    # Rule 3: Realistic size
    invalid_size = (features_df['radius'].values < 5) | (features_df['radius'].values > 80)
    valid_mask &= ~invalid_size
    
    print(f"Valid predictions: {valid_mask.sum()}/{len(predictions)}")
    print(f"Rejected by Tier 3: {(~valid_mask).sum()}")
    
    return valid_mask

# ============ Step 6: EVALUATION ============

def evaluate_pipeline(y_true, y_pred, tier_name="Pipeline"):
    """Comprehensive evaluation metrics"""
    acc = accuracy_score(y_true, y_pred)
    prec = precision_score(y_true, y_pred, average='weighted', zero_division=0)
    recall = recall_score(y_true, y_pred, average='weighted', zero_division=0)
    f1 = f1_score(y_true, y_pred, average='weighted', zero_division=0)
    
    cm = confusion_matrix(y_true, y_pred)
    
    print(f"\n{tier_name} Evaluation:")
    print(f"  Accuracy:  {acc:.4f}")
    print(f"  Precision: {prec:.4f}")
    print(f"  Recall:    {recall:.4f}")
    print(f"  F1-Score:  {f1:.4f}")
    print(f"\nConfusion Matrix:")
    print(cm)
    
    # Per-class metrics
    labels = ['Bullet Hole', 'Non-Bullet', 'Ambiguous']
    print(f"\nPer-class metrics:")
    for i, label in enumerate(labels):
        tp = cm[i, i]
        fp = cm[:, i].sum() - tp
        fn = cm[i, :].sum() - tp
        precision = tp / (tp + fp) if (tp + fp) > 0 else 0
        recall = tp / (tp + fn) if (tp + fn) > 0 else 0
        f1 = 2 * precision * recall / (precision + recall) if (precision + recall) > 0 else 0
        print(f"  {label}: Prec={precision:.4f}, Rec={recall:.4f}, F1={f1:.4f}")
    
    return {'accuracy': acc, 'precision': prec, 'recall': recall, 'f1': f1, 'confusion_matrix': cm}

# ============ MAIN TRAINING PIPELINE ============

def main():
    print("\n" + "="*70)
    print("MULTI-TIER BULLET HOLE DETECTION SYSTEM - TRAINING PIPELINE")
    print("="*70)
    
    # Step 1: Load dataset
    X, y, df = load_dataset()
    if X is None:
        return
    
    # Step 2: Train/Val/Test split
    X_train, X_val, X_test, y_train, y_val, y_test = split_dataset(X, y)
    
    # Normalize
    X_train_scaled, X_val_scaled, X_test_scaled, scaler = normalize_features(X_train, X_val, X_test)
    
    # Step 3: Train Tier 1
    models_tier1 = train_tier1(X_train, X_val, y_train, y_val, X_train_scaled, X_val_scaled)
    
    # Tier 1 predictions on test set
    print("\n" + "="*70)
    print("TIER 1 EVALUATION")
    print("="*70)
    
    y_pred_tier1, conf_tier1 = tier1_ensemble_predict(models_tier1, X_test_scaled)
    metrics_tier1 = evaluate_pipeline(y_test, y_pred_tier1, "Tier 1")
    
    # Step 4: Train Tier 2
    model_tier2 = train_tier2(X_train_scaled, X_val_scaled, X_test_scaled, y_train, y_val, y_test)
    
    # Tier 2 predictions
    if model_tier2:
        print("\n" + "="*70)
        print("TIER 2 EVALUATION")
        print("="*70)
        
        y_pred_tier2 = np.argmax(model_tier2.predict(X_test_scaled, verbose=0), axis=1)
        metrics_tier2 = evaluate_pipeline(y_test, y_pred_tier2, "Tier 2")
    
    # Step 5: Tier 3 Physics Validation
    print("\n" + "="*70)
    print("FULL PIPELINE EVALUATION")
    print("="*70)
    
    test_df = df.iloc[X_test.shape[0]:]  # This is simplified; in production, track indices properly
    valid_mask = tier3_validate(y_pred_tier1, df)
    
    # Step 6: Save results
    print("\n" + "="*70)
    print("SAVING RESULTS")
    print("="*70)
    
    results = {
        'tier1_metrics': metrics_tier1,
        'dataset_split': {
            'train': len(X_train),
            'validation': len(X_val),
            'test': len(X_test)
        }
    }
    
    if model_tier2:
        results['tier2_metrics'] = metrics_tier2
    
    with open(RESULTS_DIR / "training_report.json", "w") as f:
        json.dump(results, f, indent=2, default=str)
    
    print(f"\nTraining report saved to {RESULTS_DIR / 'training_report.json'}")
    print(f"Models saved to {MODELS_DIR}")
    print("\n" + "="*70)
    print("TRAINING COMPLETE")
    print("="*70 + "\n")

if __name__ == "__main__":
    main()
