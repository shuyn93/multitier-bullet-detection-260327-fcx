"""
Model Training and Evaluation Pipeline

Provides a complete training framework for bullet hole detection models.
Includes:
- Multiple model architectures (Naive Bayes, GMM, SVM, MLP, Ensemble)
- Cross-validation
- Hyperparameter tuning
- Performance metrics
- Model persistence
"""

import numpy as np
import pandas as pd
from pathlib import Path
import pickle
import json
from datetime import datetime
from typing import Dict, Tuple
import warnings
warnings.filterwarnings('ignore')

# ============================================================================
# Model Factory
# ============================================================================

class ModelFactory:
    """Factory for creating and configuring models"""
    
    @staticmethod
    def create_naive_bayes():
        """Create Naive Bayes classifier"""
        from sklearn.naive_bayes import GaussianNB
        return GaussianNB()
    
    @staticmethod
    def create_gmm(n_components: int = 3):
        """Create Gaussian Mixture Model"""
        from sklearn.mixture import GaussianMixture
        return GaussianMixture(
            n_components=n_components,
            covariance_type='full',
            random_state=42,
            n_init=10
        )
    
    @staticmethod
    def create_svm():
        """Create Support Vector Machine"""
        from sklearn.svm import SVC
        return SVC(
            kernel='rbf',
            C=1.0,
            gamma='scale',
            probability=True,
            random_state=42
        )
    
    @staticmethod
    def create_random_forest():
        """Create Random Forest classifier"""
        from sklearn.ensemble import RandomForestClassifier
        return RandomForestClassifier(
            n_estimators=100,
            max_depth=15,
            min_samples_split=5,
            min_samples_leaf=2,
            random_state=42,
            n_jobs=-1
        )
    
    @staticmethod
    def create_mlp():
        """Create Multi-Layer Perceptron"""
        from sklearn.neural_network import MLPClassifier
        return MLPClassifier(
            hidden_layer_sizes=(128, 64, 32),
            activation='relu',
            solver='adam',
            batch_size=32,
            learning_rate_init=0.001,
            max_iter=500,
            early_stopping=True,
            validation_fraction=0.1,
            n_iter_no_change=20,
            random_state=42,
            verbose=False
        )
    
    @staticmethod
    def create_gradient_boosting():
        """Create Gradient Boosting classifier"""
        from sklearn.ensemble import GradientBoostingClassifier
        return GradientBoostingClassifier(
            n_estimators=100,
            learning_rate=0.1,
            max_depth=5,
            min_samples_split=5,
            subsample=0.8,
            random_state=42
        )
    
    @staticmethod
    def create_ensemble():
        """Create ensemble of multiple models"""
        from sklearn.ensemble import VotingClassifier
        
        models = [
            ('nb', ModelFactory.create_naive_bayes()),
            ('svm', ModelFactory.create_svm()),
            ('rf', ModelFactory.create_random_forest()),
        ]
        
        return VotingClassifier(
            estimators=models,
            voting='soft'
        )


# ============================================================================
# Training Pipeline
# ============================================================================

class ModelTrainer:
    """Training and evaluation pipeline"""
    
    def __init__(self, dataset, output_dir: str = "model_results"):
        self.dataset = dataset
        self.output_dir = Path(output_dir)
        self.output_dir.mkdir(exist_ok=True)
        
        self.models = {}
        self.results = {}
        self.split = None
    
    def prepare_data(self, test_size: float = 0.2, val_size: float = 0.1):
        """Prepare train/val/test split"""
        print("Preparing data split...")
        self.split = self.dataset.get_split(
            test_size=test_size,
            val_size=val_size,
            stratify=True
        )
        
        print(f"  Training samples: {len(self.split['X_train'])}")
        print(f"  Validation samples: {len(self.split['X_val'])}")
        print(f"  Test samples: {len(self.split['X_test'])}")
    
    def train_model(self, model_name: str, model) -> Dict:
        """Train a single model"""
        print(f"\nTraining {model_name}...")
        
        import time
        from sklearn.metrics import (
            accuracy_score, precision_score, recall_score, f1_score,
            confusion_matrix, classification_report, roc_auc_score
        )
        
        start_time = time.time()
        
        # Train
        model.fit(self.split['X_train'], self.split['y_train'])
        train_time = time.time() - start_time
        
        # Predict
        y_pred_train = model.predict(self.split['X_train'])
        y_pred_val = model.predict(self.split['X_val'])
        y_pred_test = model.predict(self.split['X_test'])
        
        # Metrics
        results = {
            'model_name': model_name,
            'train_time': train_time,
            'train_accuracy': accuracy_score(self.split['y_train'], y_pred_train),
            'val_accuracy': accuracy_score(self.split['y_val'], y_pred_val),
            'test_accuracy': accuracy_score(self.split['y_test'], y_pred_test),
            'test_precision': precision_score(self.split['y_test'], y_pred_test, average='weighted', zero_division=0),
            'test_recall': recall_score(self.split['y_test'], y_pred_test, average='weighted', zero_division=0),
            'test_f1': f1_score(self.split['y_test'], y_pred_test, average='weighted', zero_division=0),
            'confusion_matrix': confusion_matrix(self.split['y_test'], y_pred_test).tolist(),
            'classification_report': classification_report(
                self.split['y_test'], y_pred_test,
                target_names=list(self.dataset.idx_to_label.values()),
                output_dict=True
            )
        }
        
        # Try to get AUC (only for binary/multiclass with probability)
        try:
            if hasattr(model, 'predict_proba'):
                y_prob = model.predict_proba(self.split['X_test'])
                if len(np.unique(self.split['y_test'])) == 2:
                    results['test_auc'] = roc_auc_score(self.split['y_test'], y_prob[:, 1])
                else:
                    results['test_auc'] = roc_auc_score(
                        self.split['y_test'], y_prob, multi_class='ovr', average='weighted'
                    )
        except:
            results['test_auc'] = None
        
        self.models[model_name] = model
        self.results[model_name] = results
        
        # Print summary
        print(f"  Train acc: {results['train_accuracy']:.4f}")
        print(f"  Val acc:   {results['val_accuracy']:.4f}")
        print(f"  Test acc:  {results['test_accuracy']:.4f}")
        print(f"  Test F1:   {results['test_f1']:.4f}")
        print(f"  Time:      {train_time:.2f}s")
        
        return results
    
    def train_all_models(self):
        """Train all available models"""
        models = {
            'Naive Bayes': ModelFactory.create_naive_bayes(),
            'SVM': ModelFactory.create_svm(),
            'Random Forest': ModelFactory.create_random_forest(),
            'Gradient Boosting': ModelFactory.create_gradient_boosting(),
            'MLP': ModelFactory.create_mlp(),
            'Ensemble': ModelFactory.create_ensemble(),
        }
        
        for model_name, model in models.items():
            try:
                self.train_model(model_name, model)
            except Exception as e:
                print(f"  ? Error training {model_name}: {e}")
    
    def save_models(self):
        """Save trained models to disk"""
        for model_name, model in self.models.items():
            model_path = self.output_dir / f"{model_name.lower().replace(' ', '_')}.pkl"
            with open(model_path, 'wb') as f:
                pickle.dump(model, f)
            print(f"? Saved {model_name} to {model_path}")
    
    def save_results(self):
        """Save results to JSON"""
        results_path = self.output_dir / "training_results.json"
        
        # Convert non-serializable objects
        results_export = {}
        for model_name, results in self.results.items():
            results_export[model_name] = {
                k: (v.tolist() if isinstance(v, np.ndarray) else v)
                for k, v in results.items() if k != 'classification_report'
            }
            results_export[model_name]['classification_report'] = results['classification_report']
        
        with open(results_path, 'w') as f:
            json.dump(results_export, f, indent=2, default=str)
        
        print(f"? Saved results to {results_path}")
    
    def generate_report(self):
        """Generate comprehensive report"""
        report_path = self.output_dir / "training_report.txt"
        
        with open(report_path, 'w') as f:
            f.write("=" * 80 + "\n")
            f.write("BULLET HOLE DETECTION MODEL TRAINING REPORT\n")
            f.write("=" * 80 + "\n\n")
            
            f.write(f"Generated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n\n")
            
            # Dataset info
            f.write("DATASET INFORMATION\n")
            f.write("-" * 80 + "\n")
            f.write(f"Total samples: {len(self.dataset.annotations)}\n")
            f.write(f"Features: {len(self.dataset.feature_cols)}\n")
            f.write(f"Training samples: {len(self.split['X_train'])}\n")
            f.write(f"Validation samples: {len(self.split['X_val'])}\n")
            f.write(f"Test samples: {len(self.split['X_test'])}\n\n")
            
            # Model results
            f.write("MODEL PERFORMANCE\n")
            f.write("-" * 80 + "\n")
            
            # Sort by test accuracy
            sorted_results = sorted(
                self.results.items(),
                key=lambda x: x[1]['test_accuracy'],
                reverse=True
            )
            
            for model_name, results in sorted_results:
                f.write(f"\n{model_name}\n")
                f.write("  Training accuracy:  {:.4f}\n".format(results['train_accuracy']))
                f.write("  Validation accuracy: {:.4f}\n".format(results['val_accuracy']))
                f.write("  Test accuracy:       {:.4f}\n".format(results['test_accuracy']))
                f.write("  Test Precision:      {:.4f}\n".format(results['test_precision']))
                f.write("  Test Recall:         {:.4f}\n".format(results['test_recall']))
                f.write("  Test F1:             {:.4f}\n".format(results['test_f1']))
                if results['test_auc'] is not None:
                    f.write("  Test AUC:            {:.4f}\n".format(results['test_auc']))
                f.write("  Training time:       {:.2f}s\n".format(results['train_time']))
            
            # Best model
            best_model = sorted_results[0]
            f.write("\n" + "=" * 80 + "\n")
            f.write(f"BEST MODEL: {best_model[0]}\n")
            f.write(f"Test Accuracy: {best_model[1]['test_accuracy']:.4f}\n")
            f.write("=" * 80 + "\n")
        
        print(f"? Saved report to {report_path}")
    
    def print_summary(self):
        """Print training summary"""
        print("\n" + "=" * 80)
        print("TRAINING SUMMARY")
        print("=" * 80)
        
        sorted_results = sorted(
            self.results.items(),
            key=lambda x: x[1]['test_accuracy'],
            reverse=True
        )
        
        print(f"\n{'Model':<25} {'Train Acc':<12} {'Val Acc':<12} {'Test Acc':<12} {'F1 Score':<12}")
        print("-" * 73)
        
        for model_name, results in sorted_results:
            print(
                f"{model_name:<25} "
                f"{results['train_accuracy']:<12.4f} "
                f"{results['val_accuracy']:<12.4f} "
                f"{results['test_accuracy']:<12.4f} "
                f"{results['test_f1']:<12.4f}"
            )
        
        print("\n" + "=" * 80)


# ============================================================================
# Hyperparameter Tuning
# ============================================================================

def tune_svm_parameters(dataset, split: Dict):
    """Tune SVM hyperparameters"""
    from sklearn.model_selection import GridSearchCV
    from sklearn.svm import SVC
    
    print("Tuning SVM parameters...")
    
    param_grid = {
        'C': [0.1, 1, 10],
        'gamma': ['scale', 'auto', 0.001, 0.01],
        'kernel': ['rbf', 'poly']
    }
    
    svm = SVC()
    grid_search = GridSearchCV(svm, param_grid, cv=5, n_jobs=-1, verbose=1)
    grid_search.fit(split['X_train'], split['y_train'])
    
    print(f"Best parameters: {grid_search.best_params_}")
    print(f"Best CV score: {grid_search.best_score_:.4f}")
    
    return grid_search.best_estimator_


def tune_mlp_parameters(dataset, split: Dict):
    """Tune MLP hyperparameters"""
    from sklearn.model_selection import GridSearchCV
    from sklearn.neural_network import MLPClassifier
    
    print("Tuning MLP parameters...")
    
    param_grid = {
        'hidden_layer_sizes': [(64, 32), (128, 64, 32), (256, 128, 64)],
        'learning_rate_init': [0.001, 0.01, 0.1],
        'alpha': [0.0001, 0.001],
    }
    
    mlp = MLPClassifier(max_iter=500, early_stopping=True, random_state=42)
    grid_search = GridSearchCV(mlp, param_grid, cv=5, n_jobs=-1, verbose=1)
    grid_search.fit(split['X_train'], split['y_train'])
    
    print(f"Best parameters: {grid_search.best_params_}")
    print(f"Best CV score: {grid_search.best_score_:.4f}")
    
    return grid_search.best_estimator_


# ============================================================================
# Cross-Validation Analysis
# ============================================================================

def cross_validate_models(dataset, split: Dict, n_splits: int = 5):
    """Perform k-fold cross-validation"""
    from sklearn.model_selection import cross_val_score
    
    models = {
        'Naive Bayes': ModelFactory.create_naive_bayes(),
        'SVM': ModelFactory.create_svm(),
        'Random Forest': ModelFactory.create_random_forest(),
        'MLP': ModelFactory.create_mlp(),
    }
    
    results = {}
    
    print(f"Running {n_splits}-fold cross-validation...\n")
    
    for model_name, model in models.items():
        scores = cross_val_score(
            model, split['X_train'], split['y_train'],
            cv=n_splits, scoring='accuracy', n_jobs=-1
        )
        
        results[model_name] = {
            'scores': scores,
            'mean': scores.mean(),
            'std': scores.std(),
        }
        
        print(f"{model_name}:")
        print(f"  Fold scores: {[f'{s:.4f}' for s in scores]}")
        print(f"  Mean: {scores.mean():.4f} (+/- {scores.std():.4f})\n")
    
    return results


# ============================================================================
# Main
# ============================================================================

def main():
    """Main training pipeline"""
    from dataset_utils import BulletHoleDataset
    
    print("=" * 80)
    print("BULLET HOLE DETECTION - MODEL TRAINING PIPELINE")
    print("=" * 80)
    
    # Load dataset
    print("\nLoading dataset...")
    dataset = BulletHoleDataset()
    
    # Initialize trainer
    trainer = ModelTrainer(dataset)
    
    # Prepare data
    trainer.prepare_data(test_size=0.2, val_size=0.1)
    
    # Train all models
    trainer.train_all_models()
    
    # Save models and results
    trainer.save_models()
    trainer.save_results()
    trainer.generate_report()
    trainer.print_summary()
    
    # Cross-validation
    cv_results = cross_validate_models(dataset, trainer.split, n_splits=5)
    
    print("\n? Training pipeline complete!")
    print(f"Results saved to: {trainer.output_dir.absolute()}")


if __name__ == '__main__':
    main()
