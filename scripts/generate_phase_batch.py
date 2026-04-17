"""
Optimized Batch Generator - Phase 1: Generate first 5000 samples (1250 per class)
Can be run multiple times to extend dataset
"""

import numpy as np
import cv2
import os
import csv
from pathlib import Path
from scipy import ndimage
import warnings
import time

warnings.filterwarnings('ignore')

class OptimizedBatchGenerator:
    def __init__(self, output_dir, batch_size=1000):
        self.output_dir = Path(output_dir)
        self.images_dir = self.output_dir / 'images'
        self.batch_size = batch_size
        self.image_height = 256
        self.image_width = 256
        
        self.feature_ranges = {
            'area': (10, 5000),
            'circularity': (0, 1),
            'solidity': (0.3, 1),
            'elongation': (0, 1),
            'radial_symmetry': (0, 1),
            'radial_gradient_consistency': (0, 1),
            'snr': (0, 100),
            'entropy': (0, 8),
            'ring_energy': (0, 1),
            'sharpness': (0, 1),
            'laplacian_density': (0, 1),
            'phase_coherence': (0, 1),
            'mean_intensity': (0, 255),
            'intensity_contrast': (0, 255),
            'variance': (0, 10000),
            'edge_density': (0, 1),
            'local_uniformity': (0, 1)
        }
        
        self.annotations = []
        self.images_dir.mkdir(parents=True, exist_ok=True)
    
    def normalize_feature(self, value, feature_name):
        """Normalize feature to [0, 1]"""
        if feature_name not in self.feature_ranges:
            return np.clip(value, 0, 1)
        min_val, max_val = self.feature_ranges[feature_name]
        if max_val == min_val:
            return 0.5
        normalized = (value - min_val) / (max_val - min_val)
        return np.clip(normalized, 0, 1)
    
    def create_ir_background(self, intensity_range=(50, 120)):
        """Create non-uniform IR background"""
        bg = np.random.uniform(intensity_range[0], intensity_range[1], 
                              (self.image_height, self.image_width))
        for octave in range(3):
            freq = 2 ** octave
            noise = np.random.uniform(-1, 1, 
                                     (self.image_height // freq, self.image_width // freq))
            noise = cv2.resize(noise.astype(np.float32), 
                             (self.image_width, self.image_height),
                             interpolation=cv2.INTER_LINEAR)
            amplitude = 10 * (0.5 ** octave)
            bg += noise * amplitude
        bg = cv2.GaussianBlur(bg.astype(np.float32), (31, 31), 10)
        return np.clip(bg, intensity_range[0], intensity_range[1])
    
    def create_bright_spot(self, center, radius, intensity, fade_type='gaussian'):
        """Create bright IR spot"""
        spot = np.zeros((self.image_height, self.image_width), dtype=np.float32)
        y, x = np.ogrid[:self.image_height, :self.image_width]
        dist = np.sqrt((x - center[0])**2 + (y - center[1])**2)
        
        if fade_type == 'gaussian':
            mask = np.exp(-(dist**2) / (2 * radius**2))
        else:
            mask = np.maximum(0, 1 - (dist / radius)**2)
        
        return intensity * mask
    
    def generate_bullet_hole(self, bg):
        """Single clear bullet hole"""
        cx = np.random.randint(50, self.image_width - 50)
        cy = np.random.randint(50, self.image_height - 50)
        radius = np.random.uniform(8, 35)
        intensity = np.random.uniform(200, 255)
        spot = self.create_bright_spot((cx, cy), radius, intensity)
        spot = cv2.GaussianBlur(spot, (5, 5), 1)
        image = bg + spot
        return np.clip(image, 0, 255).astype(np.uint8)
    
    def generate_overlapping_bullet_holes(self, bg):
        """Multiple overlapping holes"""
        image = bg.copy()
        num_holes = np.random.randint(2, 5)
        for _ in range(num_holes):
            cx = np.random.randint(40, self.image_width - 40)
            cy = np.random.randint(40, self.image_height - 40)
            radius = np.random.uniform(6, 25)
            intensity = np.random.uniform(180, 255)
            spot = self.create_bright_spot((cx, cy), radius, intensity)
            spot = cv2.GaussianBlur(spot, (3, 3), 0.5)
            image += spot
        return np.clip(image, 0, 255).astype(np.uint8)
    
    def generate_unclear(self, bg):
        """Low contrast hole"""
        cx = np.random.randint(50, self.image_width - 50)
        cy = np.random.randint(50, self.image_height - 50)
        radius = np.random.uniform(5, 20)
        intensity = np.random.uniform(120, 180)
        spot = self.create_bright_spot((cx, cy), radius, intensity)
        kernel_size = np.random.randint(3, 8) | 1
        spot = cv2.GaussianBlur(spot, (kernel_size, kernel_size), 1.5)
        noise = np.random.normal(0, 10, spot.shape)
        image = bg + spot + noise
        return np.clip(image, 0, 255).astype(np.uint8)
    
    def generate_non_bullet(self, bg):
        """Random artifacts"""
        image = bg.copy()
        artifact_type = np.random.choice(['noise', 'reflection', 'artifact'])
        
        if artifact_type == 'noise':
            noise = np.random.normal(0, 15, image.shape)
            image = image + noise
        elif artifact_type == 'reflection':
            num_streaks = np.random.randint(1, 3)
            for _ in range(num_streaks):
                x1, y1 = np.random.randint(0, self.image_width, 2)
                x2, y2 = np.random.randint(0, self.image_width, 2)
                cv2.line(image, (x1, y1), (x2, y2), np.random.randint(100, 200), 2)
        else:
            num_artifacts = np.random.randint(2, 6)
            for _ in range(num_artifacts):
                cx = np.random.randint(20, self.image_width - 20)
                cy = np.random.randint(20, self.image_height - 20)
                radius = np.random.randint(3, 15)
                intensity = np.random.randint(80, 200)
                spot = self.create_bright_spot((cx, cy), radius, intensity, fade_type='quadratic')
                image = image + spot
        
        return np.clip(image, 0, 255).astype(np.uint8)
    
    def extract_17_features(self, image):
        """Extract 17 features"""
        img_float = image.astype(np.float32) / 255.0
        threshold = np.mean(image) + np.std(image) * 0.5
        binary = (image > threshold).astype(np.uint8)
        contours, _ = cv2.findContours(binary, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        
        features = {}
        
        # Default values
        features['area'] = 0
        features['circularity'] = 0
        features['solidity'] = 0
        features['elongation'] = 0
        features['radial_symmetry'] = 0
        features['radial_gradient_consistency'] = 0
        
        if len(contours) > 0:
            cnt = max(contours, key=cv2.contourArea)
            area = cv2.contourArea(cnt)
            perimeter = cv2.arcLength(cnt, True)
            
            features['area'] = self.normalize_feature(area, 'area')
            features['circularity'] = 4 * np.pi * area / (perimeter ** 2) if perimeter > 0 else 0
            
            hull = cv2.convexHull(cnt)
            hull_area = cv2.contourArea(hull)
            features['solidity'] = area / hull_area if hull_area > 0 else 0
            
            if area > 0:
                rect = cv2.minAreaRect(cnt)
                box = cv2.boxPoints(rect)
                box = np.asarray(box, dtype=np.int32)
                edges = [np.linalg.norm(box[i] - box[(i+1)%4]) for i in range(4)]
                edges.sort()
                features['elongation'] = np.clip(edges[0] / edges[1] if edges[1] > 0 else 0, 0, 1)
            
            # Radial symmetry
            if area > 10:
                M = cv2.moments(cnt)
                if M['m00'] > 0:
                    cx = int(M['m10'] / M['m00'])
                    cy = int(M['m01'] / M['m00'])
                    radial_profile = []
                    for angle in np.linspace(0, 2*np.pi, 16):
                        max_r = min(50, min(self.image_width - cx, self.image_height - cy))
                        for r in range(1, int(max_r)):
                            x = int(cx + r * np.cos(angle))
                            y = int(cy + r * np.sin(angle))
                            if 0 <= x < self.image_width and 0 <= y < self.image_height:
                                if image[y, x] > threshold:
                                    radial_profile.append(r)
                                    break
                    if radial_profile:
                        features['radial_symmetry'] = 1.0 / (1.0 + np.std(radial_profile) / max(1, np.mean(radial_profile)))
        
        # Gradient
        gradient_x = cv2.Sobel(image, cv2.CV_32F, 1, 0, ksize=3)
        gradient_y = cv2.Sobel(image, cv2.CV_32F, 0, 1, ksize=3)
        gradient_magnitude = np.sqrt(gradient_x**2 + gradient_y**2)
        features['radial_gradient_consistency'] = self.normalize_feature(
            np.std(gradient_magnitude) if gradient_magnitude.size > 0 else 0,
            'radial_gradient_consistency'
        )
        
        # Energy & Texture
        mean_intensity = np.mean(image)
        std_intensity = np.std(image)
        features['snr'] = self.normalize_feature(mean_intensity / (std_intensity + 1e-6) * 10, 'snr')
        
        hist, _ = np.histogram(image, bins=256, range=(0, 256))
        hist = hist / hist.sum()
        entropy = -np.sum(hist[hist > 0] * np.log2(hist[hist > 0]))
        features['entropy'] = self.normalize_feature(entropy, 'entropy')
        
        edges = cv2.Canny(image, 50, 150)
        features['ring_energy'] = self.normalize_feature(np.sum(edges) / 255, 'ring_energy')
        
        # Frequency & Phase
        laplacian = cv2.Laplacian(image, cv2.CV_32F)
        features['sharpness'] = self.normalize_feature(np.var(laplacian), 'sharpness')
        features['laplacian_density'] = self.normalize_feature(
            np.sum(np.abs(laplacian) > 10) / image.size, 'laplacian_density'
        )
        
        fft = np.fft.fft2(img_float)
        fft_shift = np.fft.fftshift(fft)
        phase = np.angle(fft_shift)
        phase_coherence = np.abs(np.mean(np.exp(1j * phase)))
        features['phase_coherence'] = self.normalize_feature(phase_coherence, 'phase_coherence')
        
        # Additional
        features['mean_intensity'] = self.normalize_feature(mean_intensity, 'mean_intensity')
        features['intensity_contrast'] = self.normalize_feature(
            np.max(image) - np.min(image), 'intensity_contrast'
        )
        features['variance'] = self.normalize_feature(np.var(image), 'variance')
        
        edges = cv2.Canny(image, 50, 150)
        features['edge_density'] = self.normalize_feature(
            np.sum(edges) / 255 / image.size, 'edge_density'
        )
        
        local_var = ndimage.generic_filter(image.astype(np.float32), np.var, size=9)
        features['local_uniformity'] = self.normalize_feature(
            1.0 / (1.0 + np.mean(local_var) / 100), 'local_uniformity'
        )
        
        # Map to f1-f17
        feature_order = [
            'area', 'circularity', 'solidity', 'elongation',
            'radial_symmetry', 'radial_gradient_consistency',
            'snr', 'entropy', 'ring_energy',
            'sharpness', 'laplacian_density', 'phase_coherence',
            'mean_intensity', 'intensity_contrast', 'variance', 'edge_density', 'local_uniformity'
        ]
        
        ordered_features = {}
        for i, feature_name in enumerate(feature_order, 1):
            ordered_features[f'f{i}'] = np.clip(features.get(feature_name, 0.0), 0, 1)
        
        return ordered_features
    
    def generate_batch(self, class_label, batch_size, class_name, generator_func):
        """Generate one batch"""
        import sys
        batch_annotations = []
        batch_start = time.time()
        
        for i in range(batch_size):
            if (i + 1) % 10 == 0 or i == batch_size - 1:
                elapsed = time.time() - batch_start
                pct = 100.0 * (i + 1) / batch_size
                speed = (i + 1) / elapsed if elapsed > 0 else 0
                print(f"      {i+1}/{batch_size} ({pct:.0f}%) - {elapsed:.1f}s - {speed:.1f} img/s", end='\r', flush=True)
                sys.stdout.flush()
            
            bg = self.create_ir_background()
            image = generator_func(bg)
            features = self.extract_17_features(image)
            
            image_id = f"{class_label:06d}"
            cv2.imwrite(str(self.images_dir / f"{image_id}.png"), image)
            
            annotation = [image_id, class_name]
            annotation.extend([features[f'f{j}'] for j in range(1, 18)])
            batch_annotations.append(annotation)
            
            class_label += 1
        
        elapsed = time.time() - batch_start
        speed = batch_size / elapsed if elapsed > 0 else 0
        print(f"      ? {batch_size} samples ({elapsed:.1f}s, {elapsed/batch_size:.3f}s/img, {speed:.1f} img/s)     ")
        sys.stdout.flush()
        return class_label, batch_annotations
    
    def generate_full_dataset(self, num_samples_per_class=12500):
        """Generate full dataset (50,000 total samples)"""
        import sys
        print("\n" + "="*70)
        print("FULL DATASET GENERATION - BATCH PROCESSING")
        print(f"Total samples: {num_samples_per_class * 4:,}")
        print(f"Batch size: {self.batch_size:,}")
        print(f"Classes: 4")
        print("="*70)
        sys.stdout.flush()
        
        all_annotations = []
        class_label = 0
        
        generators = {
            'bullet_hole': self.generate_bullet_hole,
            'overlapping_bullet_holes': self.generate_overlapping_bullet_holes,
            'unclear': self.generate_unclear,
            'non_bullet': self.generate_non_bullet
        }
        
        overall_start = time.time()
        class_idx = 0
        total_classes = len(generators)
        
        for class_name, generator_func in generators.items():
            class_idx += 1
            print(f"\n[CLASS {class_idx}/{total_classes}] {class_name.upper()}")
            print(f"  Samples per class: {num_samples_per_class:,}")
            num_batches = (num_samples_per_class + self.batch_size - 1) // self.batch_size
            print(f"  Number of batches: {num_batches}")
            sys.stdout.flush()
            
            for batch_idx in range(num_batches):
                batch_samples = min(self.batch_size, 
                                  num_samples_per_class - batch_idx * self.batch_size)
                print(f"  [Batch {batch_idx + 1}/{num_batches}] ", end='', flush=True)
                sys.stdout.flush()
                
                class_label, batch_annot = self.generate_batch(
                    class_label, batch_samples, class_name, generator_func
                )
                all_annotations.extend(batch_annot)
                
                # Print status update
                total_so_far = len(all_annotations)
                pct_overall = 100.0 * total_so_far / (num_samples_per_class * 4)
                print(f"  Total: {total_so_far:,} ({pct_overall:.1f}%)")
                sys.stdout.flush()
        
        # Save annotations CSV
        csv_path = self.output_dir / 'annotations.csv'
        print(f"\n{'='*70}")
        print(f"Saving {len(all_annotations):,} annotations to {csv_path}...")
        sys.stdout.flush()
        
        with open(csv_path, 'w', newline='') as f:
            writer = csv.writer(f)
            header = ['image_name', 'class_label'] + [f'f{i}' for i in range(1, 18)]
            writer.writerow(header)
            writer.writerows(all_annotations)
        
        total_samples = len(list(self.images_dir.glob('*.png')))
        overall_time = time.time() - overall_start
        
        print(f"{'='*70}")
        print(f"? FULL DATASET GENERATION COMPLETE!")
        print(f"{'='*70}")
        print(f"Total images: {total_samples:,}")
        print(f"Total time: {overall_time/60:.1f} minutes ({overall_time:.0f}s)")
        print(f"Average speed: {overall_time/total_samples:.3f}s per image")
        print(f"Images dir: {self.images_dir}")
        print(f"Annotations: {csv_path}")
        print(f"{'='*70}\n")
        sys.stdout.flush()
        
        return total_samples
        print(f"{'='*70}\n")
        
        return total_samples


if __name__ == '__main__':
    output_directory = Path(__file__).parent.parent / 'data' / 'datasets' / 'dataset_ir_final'
    
    generator = OptimizedBatchGenerator(
        output_dir=output_directory,
        batch_size=100  # Batch size: 100 images
    )
    
    # Generate full dataset: 50,000 samples (12,500 per class)
    total = generator.generate_full_dataset(num_samples_per_class=12500)
