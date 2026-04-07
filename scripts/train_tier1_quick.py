#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Multi-Tier Bullet Hole Detection - Quick Training Script
Tests with available libraries (no TensorFlow required for initial test)
"""

import numpy as np
import pandas as pd
from pathlib import Path
import json
import pickle
import time
from sklearn.model_selection import train_test_split
from sklearn.preprocessing import StandardScaler
from sklearn.naive_bayes import GaussianNB
from sklearn.mixture import GaussianMixture
from sklearn.ensemble import RandomForestClassifier, GradientBoostingClassifier
from sklearn.metrics import accuracy_score, precision_score, recall_score, f1_score, confusion_matrix, classification_report
import warnings
warnings.filterwarnings('ignore')

DATASET_DIR = Path("dataset_ir_realistic")
ANNOTATIONS_FILE = DATASET_DIR / "annotations.csv"
MODELS_DIR = Path("models")
RESULTS_DIR = Path("results")

MODELS_DIR.mkdir(parents=True, exist_ok=True)
RESULTS_DIR.mkdir(parents=True, exist_ok=True)

RANDOM_SEED = 42
np.random.seed(RANDOM_SEED)

FEATURE_COLS = [
    'area', 'circularity', 'solidity', 'mean_intensity', 'std_intensity',
    'edge_density', 'aspect_ratio', 'center_x', 'center_y', 'radius'
]

print("\n" + "="*80)
print("MULTI-TIER BULLET HOLE DETECTION SYSTEM - TRAINING")
print("="*80)

# Load dataset
print("\n[STEP 1] Loading dataset...")
if not ANNOTATIONS_FILE.exists():
    print(f"ERROR: Dataset not found at {ANNOTATIONS_FILE}")
    exit(1)

df = pd.read_csv(ANNOTATIONS_FILE)
print(f"Loaded {len(df)} samples")

X = df[FEATURE_COLS].values.astype(np.float32)
label_map = {1: 0, 0: 1, -1: 2}
y = df['label'].map(label_map).values

print(f"Features shape: {X.shape}")
print(f"Label distribution:")
for label_id in range(3):
    count = (y == label_id).sum()
    label_names = ['Bullet Hole', 'Non-Bullet', 'Ambiguous']
    print(f"  {label_names[label_id]}: {count} ({count/len(y)*100:.1f}%)")

# Train/Val/Test split
print("\n[STEP 2] Creating train/val/test split...")
X_train, X_temp, y_train, y_temp = train_test_split(X, y, test_size=0.30, random_state=RANDOM_SEED, stratify=y)
X_val, X_test, y_val, y_test = train_test_split(X_temp, y_temp, test_size=0.50, random_state=RANDOM_SEED, stratify=y_temp)

print(f"Train: {len(X_train)} ({len(X_train)/len(X)*100:.1f}%)")
print(f"Val:   {len(X_val)} ({len(X_val)/len(X)*100:.1f}%)")
print(f"Test:  {len(X_test)} ({len(X_test)/len(X)*100:.1f}%)")

# Normalize
print("\n[STEP 3] Normalizing features...")
scaler = StandardScaler()
X_train_scaled = scaler.fit_transform(X_train)
X_val_scaled = scaler.transform(X_val)
X_test_scaled = scaler.transform(X_test)

with open(MODELS_DIR / "scaler.pkl", "wb") as f:
    pickle.dump(scaler, f)

# Train Tier 1 models
print("\n" + "="*80)
print("[STEP 4] TIER 1 TRAINING - Classical Models")
print("="*80)

models = {}
timings = {}

# 1. Naive Bayes
print("\n[1/3] Gaussian Naive Bayes...")
start = time.time()
nb_model = GaussianNB()
nb_model.fit(X_train_scaled, y_train)
nb_time = time.time() - start

y_pred_nb_val = nb_model.predict(X_val_scaled)
nb_acc_val = accuracy_score(y_val, y_pred_nb_val)

y_pred_nb_test = nb_model.predict(X_test_scaled)
nb_acc_test = accuracy_score(y_test, y_pred_nb_test)
nb_recall_test = recall_score(y_test, y_pred_nb_test, average='weighted', zero_division=0)

print(f"  Train time: {nb_time:.3f}s")
print(f"  Val accuracy: {nb_acc_val:.4f}")
print(f"  Test accuracy: {nb_acc_test:.4f}, Recall: {nb_recall_test:.4f}")

models['naive_bayes'] = nb_model
timings['naive_bayes'] = nb_time

# 2. Gaussian Mixture Model
print("\n[2/3] Gaussian Mixture Model...")
start = time.time()
gmm_model = GaussianMixture(n_components=3, random_state=RANDOM_SEED)
gmm_model.fit(X_train_scaled)
gmm_time = time.time() - start

gmm_clusters = gmm_model.predict(X_train_scaled)
cluster_to_label = np.zeros(3, dtype=int)
for cluster_id in range(3):
    mask = gmm_clusters == cluster_id
    if mask.sum() > 0:
        cluster_to_label[cluster_id] = np.bincount(y_train[mask]).argmax()

y_pred_gmm_test = cluster_to_label[gmm_model.predict(X_test_scaled)]
gmm_acc_test = accuracy_score(y_test, y_pred_gmm_test)
gmm_recall_test = recall_score(y_test, y_pred_gmm_test, average='weighted', zero_division=0)

print(f"  Train time: {gmm_time:.3f}s")
print(f"  Test accuracy: {gmm_acc_test:.4f}, Recall: {gmm_recall_test:.4f}")

models['gmm'] = gmm_model
models['gmm_cluster_map'] = cluster_to_label
timings['gmm'] = gmm_time

# 3. Random Forest
print("\n[3/3] Random Forest...")
start = time.time()
rf_model = RandomForestClassifier(n_estimators=100, max_depth=12, random_state=RANDOM_SEED, n_jobs=-1)
rf_model.fit(X_train_scaled, y_train)
rf_time = time.time() - start

y_pred_rf_test = rf_model.predict(X_test_scaled)
rf_acc_test = accuracy_score(y_test, y_pred_rf_test)
rf_recall_test = recall_score(y_test, y_pred_rf_test, average='weighted', zero_division=0)

print(f"  Train time: {rf_time:.3f}s")
print(f"  Test accuracy: {rf_acc_test:.4f}, Recall: {rf_recall_test:.4f}")

models['random_forest'] = rf_model
timings['random_forest'] = rf_time

# Tier 1 Ensemble
print("\n" + "="*80)
print("[STEP 5] TIER 1 ENSEMBLE PREDICTION")
print("="*80)

def ensemble_predict(X):
    nb_pred = models['naive_bayes'].predict(X)
    gmm_pred = models['gmm_cluster_map'][models['gmm'].predict(X)]
    rf_pred = models['random_forest'].predict(X)
    predictions = np.stack([nb_pred, gmm_pred, rf_pred], axis=1)
    ensemble_pred = np.argmax(np.bincount(predictions, axis=0), axis=1)
    return ensemble_pred

y_pred_ensemble_test = ensemble_predict(X_test_scaled)
ensemble_acc_test = accuracy_score(y_test, y_pred_ensemble_test)
ensemble_recall_test = recall_score(y_test, y_pred_ensemble_test, average='weighted', zero_division=0)

print(f"Ensemble Test Accuracy: {ensemble_acc_test:.4f}")
print(f"Ensemble Test Recall: {ensemble_recall_test:.4f}")

# Detailed evaluation
print("\nDetailed Metrics:")
cm = confusion_matrix(y_test, y_pred_ensemble_test)
print("Confusion Matrix:")
print(cm)

label_names = ['Bullet Hole', 'Non-Bullet', 'Ambiguous']
print("\nPer-class Performance:")
for i, label in enumerate(label_names):
    tp = cm[i, i]
    fp = cm[:, i].sum() - tp
    fn = cm[i, :].sum() - tp
    precision = tp / (tp + fp) if (tp + fp) > 0 else 0
    recall = tp / (tp + fn) if (tp + fn) > 0 else 0
    f1 = 2 * precision * recall / (precision + recall) if (precision + recall) > 0 else 0
    print(f"  {label:15s}: Prec={precision:.4f}, Rec={recall:.4f}, F1={f1:.4f}")

# Save models
print("\n" + "="*80)
print("[STEP 6] SAVING MODELS")
print("="*80)

with open(MODELS_DIR / "tier1_ensemble.pkl", "wb") as f:
    pickle.dump(models, f)
print(f"Saved models to {MODELS_DIR / 'tier1_ensemble.pkl'}")

# Save results
results = {
    'dataset_info': {
        'total_samples': len(df),
        'train_samples': len(X_train),
        'val_samples': len(X_val),
        'test_samples': len(X_test)
    },
    'tier1_results': {
        'naive_bayes': {'accuracy': float(nb_acc_test), 'recall': float(nb_recall_test)},
        'gmm': {'accuracy': float(gmm_acc_test), 'recall': float(gmm_recall_test)},
        'random_forest': {'accuracy': float(rf_acc_test), 'recall': float(rf_recall_test)},
        'ensemble': {'accuracy': float(ensemble_acc_test), 'recall': float(ensemble_recall_test)}
    },
    'timings_ms': {k: v*1000 for k, v in timings.items()}
}

with open(RESULTS_DIR / "training_report.json", "w") as f:
    json.dump(results, f, indent=2)

print(f"Results saved to {RESULTS_DIR / 'training_report.json'}")

print("\n" + "="*80)
print("TRAINING COMPLETE")
print("="*80 + "\n")
