#!/usr/bin/env python3
"""
IR Backlit Bullet Hole Detection - Final Dataset Generator (50,000 samples)
============================================================================

This script generates a comprehensive dataset for IR backlit bullet hole detection:
- 50,000 total samples
- 4 balanced classes (~12,500 each)
- Correct IR physics (bright holes)
- 17 normalized features per sample
- Proper data splits (train/test)

Classes:
1. bullet_hole: Clear, circular, bright IR blob
2. overlapping_bullet_holes: Merged bright blobs from multiple hits
3. unclear: Low contrast, blurred, partially visible
4. non_bullet: Noise, reflections, random bright artifacts

Feature Engineering (17 features, normalized to [0,1]):
A. Geometry (4): area, circularity, solidity, elongation
B. Radial Structure (2): radial_symmetry, radial_gradient_consistency
C. Energy & Texture (3): SNR, entropy, ring_energy
D. Frequency & Phase (3): sharpness, laplacian_density, phase_coherence
E. Additional (5): mean_intensity, intensity_contrast, variance, edge_density, local_uniformity
"""

import numpy as np
import cv2
import os
import csv
from pathlib import Path
from scipy import ndimage, signal
from skimage import exposure, filters, feature
import warnings

warnings.filterwarnings('ignore')

class IRBacklitDatasetGenerator:
    def __init__(self, output_dir, num_samples_per_class=12500):
        self.output_dir = Path(output_dir)
        self.images_dir = self.output_dir / 'images'
        self.test_images_dir = self.output_dir / 'test_images'
        self.num_samples_per_class = num_samples_per_class
        self.image_height = 256
        self.image_width = 256
        
        # Feature normalization limits (for later normalization)
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
        
    def normalize_feature(self, value, feature_name):
        """Normalize feature to [0, 1] range"""
        if feature_name not in self.feature_ranges:
            return np.clip(value, 0, 1)
        
        min_val, max_val = self.feature_ranges[feature_name]
        if max_val == min_val:
            return 0.5
        normalized = (value - min_val) / (max_val - min_val)
        return np.clip(normalized, 0, 1)
    
    def create_ir_background(self, intensity_range=(50, 120)):
        """Create non-uniform IR background with low-frequency variation"""
        # Create base background
        bg = np.random.uniform(intensity_range[0], intensity_range[1], 
                              (self.image_height, self.image_width))
        
        # Add low-frequency variation using Perlin-like noise
        for octave in range(3):
            freq = 2 ** octave
            noise = np.random.uniform(-1, 1, 
                                     (self.image_height // freq, self.image_width // freq))
            noise = cv2.resize(noise.astype(np.float32), 
                             (self.image_width, self.image_height),
                             interpolation=cv2.INTER_LINEAR)
            amplitude = 10 * (0.5 ** octave)
            bg += noise * amplitude
        
        # Apply Gaussian blur to smooth
        bg = cv2.GaussianBlur(bg.astype(np.float32), (31, 31), 10)
        return np.clip(bg, intensity_range[0], intensity_range[1])
    
    def create_bright_spot(self, center, radius, intensity, fade_type='gaussian'):
        """Create a bright IR spot (bullet hole) on background"""
        spot = np.zeros((self.image_height, self.image_width), dtype=np.float32)
        
        y, x = np.ogrid[:self.image_height, :self.image_width]
        dist = np.sqrt((x - center[0])**2 + (y - center[1])**2)
        
        if fade_type == 'gaussian':
            # Gaussian falloff (most realistic for IR)
            mask = np.exp(-(dist**2) / (2 * radius**2))
        elif fade_type == 'quadratic':
            # Quadratic falloff
            mask = np.maximum(0, 1 - (dist / radius)**2)
        elif fade_type == 'linear':
            # Linear falloff
            mask = np.maximum(0, 1 - dist / radius)
        else:
            # Circular/hard edge
            mask = (dist <= radius).astype(np.float32)
        
        spot = intensity * mask
        return spot
    
    def generate_bullet_hole(self, bg):
        """Generate a single, clear bullet hole"""
        # Random position (avoid edges)
        margin = 50
        cx = np.random.randint(margin, self.image_width - margin)
        cy = np.random.randint(margin, self.image_height - margin)
        
        # Bullet hole characteristics
        radius = np.random.uniform(8, 35)  # Varied sizes including small holes (r < 10)
        intensity = np.random.uniform(200, 255)  # BRIGHT (backlit physics)
        
        # Create the spot
        spot = self.create_bright_spot((cx, cy), radius, intensity, fade_type='gaussian')
        
        # Add slight blur for realism
        spot = cv2.GaussianBlur(spot, (5, 5), 1)
        
        # Combine with background
        image = bg + spot
        image = np.clip(image, 0, 255)
        
        return image.astype(np.uint8)
    
    def generate_overlapping_bullet_holes(self, bg):
        """Generate multiple overlapping bullet holes"""
        image = bg.copy()
        num_holes = np.random.randint(2, 5)  # 2-4 overlapping holes
        
        for _ in range(num_holes):
            # Random position
            margin = 40
            cx = np.random.randint(margin, self.image_width - margin)
            cy = np.random.randint(margin, self.image_height - margin)
            
            # Varying sizes and intensities
            radius = np.random.uniform(6, 25)
            intensity = np.random.uniform(180, 255)
            
            spot = self.create_bright_spot((cx, cy), radius, intensity, fade_type='gaussian')
            spot = cv2.GaussianBlur(spot, (3, 3), 0.5)
            image += spot
        
        image = np.clip(image, 0, 255)
        return image.astype(np.uint8)
    
    def generate_unclear(self, bg):
        """Generate unclear/low-contrast bullet hole image"""
        image = bg.copy()
        
        # Low contrast bright spot (harder to detect)
        margin = 50
        cx = np.random.randint(margin, self.image_width - margin)
        cy = np.random.randint(margin, self.image_height - margin)
        
        radius = np.random.uniform(5, 20)
        # Lower intensity (lower contrast vs background)
        intensity = np.random.uniform(120, 180)
        
        spot = self.create_bright_spot((cx, cy), radius, intensity, fade_type='gaussian')
        
        # Add motion blur
        kernel_size = np.random.randint(3, 8)
        if kernel_size % 2 == 0:
            kernel_size += 1
        spot = cv2.GaussianBlur(spot, (kernel_size, kernel_size), 1.5)
        
        # Add stronger noise
        noise = np.random.normal(0, 10, spot.shape)
        image = image + spot + noise
        image = np.clip(image, 0, 255)
        
        return image.astype(np.uint8)
    
    def generate_non_bullet(self, bg):
        """Generate non-bullet artifacts (noise, reflections, random artifacts)"""
        image = bg.copy()
        
        artifact_type = np.random.choice(['noise', 'reflection', 'artifact', 'mixed'])
        
        if artifact_type == 'noise':
            # Random bright noise
            noise = np.random.normal(0, 15, image.shape)
            image = image + noise
        
        elif artifact_type == 'reflection':
            # Random bright streaks (reflections)
            num_streaks = np.random.randint(1, 3)
            for _ in range(num_streaks):
                x1, y1 = np.random.randint(0, self.image_width, 2)
                x2, y2 = np.random.randint(0, self.image_width, 2)
                intensity = np.random.randint(100, 200)
                cv2.line(image, (x1, y1), (x2, y2), intensity, np.random.randint(1, 4))
        
        elif artifact_type == 'artifact':
            # Random small artifacts
            num_artifacts = np.random.randint(2, 6)
            for _ in range(num_artifacts):
                cx = np.random.randint(20, self.image_width - 20)
                cy = np.random.randint(20, self.image_height - 20)
                radius = np.random.randint(3, 15)
                intensity = np.random.randint(80, 200)
                
                # Irregular shape (not circular)
                spot = self.create_bright_spot((cx, cy), radius, intensity, 
                                              fade_type=np.random.choice(['linear', 'quadratic']))
                image = image + spot
        
        else:  # mixed
            # Combination of noise and artifacts
            noise = np.random.normal(0, 8, image.shape)
            image = image + noise
            
            num_artifacts = np.random.randint(1, 3)
            for _ in range(num_artifacts):
                cx = np.random.randint(20, self.image_width - 20)
                cy = np.random.randint(20, self.image_height - 20)
                radius = np.random.randint(2, 10)
                intensity = np.random.randint(60, 180)
                spot = self.create_bright_spot((cx, cy), radius, intensity, fade_type='quadratic')
                image = image + spot
        
        image = np.clip(image, 0, 255)
        return image.astype(np.uint8)
    
    def extract_17_features(self, image):
        """Extract exactly 17 normalized features"""
        # Convert to float for processing
        img_float = image.astype(np.float32) / 255.0
        
        # Threshold to get candidate regions
        threshold = np.mean(image) + np.std(image) * 0.5
        binary = (image > threshold).astype(np.uint8)
        
        # Find contours
        contours, _ = cv2.findContours(binary, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        
        features = {}
        
        if len(contours) > 0:
            # Use largest contour
            cnt = max(contours, key=cv2.contourArea)
            area = cv2.contourArea(cnt)
            perimeter = cv2.arcLength(cnt, True)
            
            # A. GEOMETRY (4 features)
            features['area'] = self.normalize_feature(area, 'area')
            
            if perimeter > 0:
                features['circularity'] = 4 * np.pi * area / (perimeter ** 2)
            else:
                features['circularity'] = 0
            
            # Solidity
            hull = cv2.convexHull(cnt)
            hull_area = cv2.contourArea(hull)
            features['solidity'] = area / hull_area if hull_area > 0 else 0
            
            # Elongation
            if area > 0:
                rect = cv2.minAreaRect(cnt)
                box = cv2.boxPoints(rect)
                box = np.asarray(box, dtype=np.int32)
                edges = [np.linalg.norm(box[i] - box[(i+1)%4]) for i in range(4)]
                edges.sort()
                elongation = edges[0] / edges[1] if edges[1] > 0 else 0
                features['elongation'] = np.clip(elongation, 0, 1)
            else:
                features['elongation'] = 0
        else:
            # No contours found
            features['area'] = 0
            features['circularity'] = 0
            features['solidity'] = 0
            features['elongation'] = 0
        
        # B. RADIAL STRUCTURE (2 features)
        # Radial symmetry
        if len(contours) > 0 and area > 10:
            M = cv2.moments(cnt)
            if M['m00'] > 0:
                cx = int(M['m10'] / M['m00'])
                cy = int(M['m01'] / M['m00'])
                
                # Compute radial profile
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
                
                # Radial symmetry: std of radial distances
                if radial_profile:
                    features['radial_symmetry'] = 1.0 / (1.0 + np.std(radial_profile) / max(1, np.mean(radial_profile)))
                else:
                    features['radial_symmetry'] = 0
            else:
                features['radial_symmetry'] = 0
        else:
            features['radial_symmetry'] = 0
        
        # Radial gradient consistency
        gradient_x = cv2.Sobel(image, cv2.CV_32F, 1, 0, ksize=3)
        gradient_y = cv2.Sobel(image, cv2.CV_32F, 0, 1, ksize=3)
        gradient_magnitude = np.sqrt(gradient_x**2 + gradient_y**2)
        features['radial_gradient_consistency'] = self.normalize_feature(
            np.std(gradient_magnitude) if gradient_magnitude.size > 0 else 0, 
            'radial_gradient_consistency'
        )
        
        # C. ENERGY & TEXTURE (3 features)
        # SNR (Signal-to-Noise Ratio)
        mean_intensity = np.mean(image)
        std_intensity = np.std(image)
        features['snr'] = self.normalize_feature(
            mean_intensity / (std_intensity + 1e-6) * 10,
            'snr'
        )
        
        # Entropy
        hist, _ = np.histogram(image, bins=256, range=(0, 256))
        hist = hist / hist.sum()
        entropy = -np.sum(hist[hist > 0] * np.log2(hist[hist > 0]))
        features['entropy'] = self.normalize_feature(entropy, 'entropy')
        
        # Ring energy
        edges = cv2.Canny(image, 50, 150)
        features['ring_energy'] = self.normalize_feature(
            np.sum(edges) / 255,
            'ring_energy'
        )
        
        # D. FREQUENCY & PHASE (3 features)
        # Sharpness (using Laplacian)
        laplacian = cv2.Laplacian(image, cv2.CV_32F)
        features['sharpness'] = self.normalize_feature(
            np.var(laplacian),
            'sharpness'
        )
        
        # Laplacian density
        features['laplacian_density'] = self.normalize_feature(
            np.sum(np.abs(laplacian) > 10) / image.size,
            'laplacian_density'
        )
        
        # Phase coherence (simplified)
        fft = np.fft.fft2(img_float)
        fft_shift = np.fft.fftshift(fft)
        phase = np.angle(fft_shift)
        phase_coherence = np.abs(np.mean(np.exp(1j * phase)))
        features['phase_coherence'] = self.normalize_feature(phase_coherence, 'phase_coherence')
        
        # E. ADDITIONAL (5 features)
        features['mean_intensity'] = self.normalize_feature(mean_intensity, 'mean_intensity')
        features['intensity_contrast'] = self.normalize_feature(
            np.max(image) - np.min(image),
            'intensity_contrast'
        )
        features['variance'] = self.normalize_feature(
            np.var(image),
            'variance'
        )
        
        # Edge density
        edges = cv2.Canny(image, 50, 150)
        features['edge_density'] = self.normalize_feature(
            np.sum(edges) / 255 / image.size,
            'edge_density'
        )
        
        # Local uniformity (inverse of local variance)
        local_var = ndimage.generic_filter(image.astype(np.float32), np.var, size=9)
        features['local_uniformity'] = self.normalize_feature(
            1.0 / (1.0 + np.mean(local_var) / 100),
            'local_uniformity'
        )
        
        # Ensure all features are normalized to [0, 1]
        for key in features:
            features[key] = np.clip(features[key], 0, 1)
        
        # Map features to f1-f17 in consistent order
        feature_order = [
            'area', 'circularity', 'solidity', 'elongation',  # A. Geometry (4)
            'radial_symmetry', 'radial_gradient_consistency',  # B. Radial Structure (2)
            'snr', 'entropy', 'ring_energy',  # C. Energy & Texture (3)
            'sharpness', 'laplacian_density', 'phase_coherence',  # D. Frequency & Phase (3)
            'mean_intensity', 'intensity_contrast', 'variance', 'edge_density', 'local_uniformity'  # E. Additional (5)
        ]
        
        ordered_features = {}
        for i, feature_name in enumerate(feature_order, 1):
            ordered_features[f'f{i}'] = features.get(feature_name, 0.0)
        
        return ordered_features
    
    def generate_class_samples(self, class_label, num_samples, class_name):
        """Generate samples for a specific class"""
        print(f"\n{'='*70}")
        print(f"Generating {class_name} ({num_samples} samples)...")
        print(f"{'='*70}")
        
        generator_func = {
            'bullet_hole': self.generate_bullet_hole,
            'overlapping_bullet_holes': self.generate_overlapping_bullet_holes,
            'unclear': self.generate_unclear,
            'non_bullet': self.generate_non_bullet
        }[class_name]
        
        for i in range(num_samples):
            # Progress indicator
            if (i + 1) % 1000 == 0:
                print(f"  Progress: {i + 1}/{num_samples} samples generated...")
            
            # Generate background
            bg = self.create_ir_background()
            
            # Generate image
            image = generator_func(bg)
            
            # Extract features
            features = self.extract_17_features(image)
            
            # Save image
            image_id = f"{class_label:06d}"
            image_path = f"{image_id}.png"
            cv2.imwrite(str(self.images_dir / image_path), image)
            
            # Store annotation
            annotation = [image_id, class_name]
            annotation.extend([features[f'f{j}'] for j in range(1, 18)])  # f1-f17
            self.annotations.append(annotation)
            
            class_label += 1
        
        print(f"? {class_name} generation complete!")
        return class_label
    
    def generate_dataset(self):
        """Generate complete dataset (50,000 samples, 4 classes)"""
        print("\n" + "="*70)
        print("STARTING IR BACKLIT BULLET HOLE DATASET GENERATION")
        print(f"Total samples: {self.num_samples_per_class * 4:,}")
        print(f"Classes: 4 (balanced)")
        print(f"Output: {self.output_dir}")
        print("="*70)
        
        class_label = 0
        
        # Generate all classes
        class_label = self.generate_class_samples(class_label, self.num_samples_per_class, 'bullet_hole')
        class_label = self.generate_class_samples(class_label, self.num_samples_per_class, 'overlapping_bullet_holes')
        class_label = self.generate_class_samples(class_label, self.num_samples_per_class, 'unclear')
        class_label = self.generate_class_samples(class_label, self.num_samples_per_class, 'non_bullet')
        
        # Save annotations
        csv_path = self.output_dir / 'annotations.csv'
        print(f"\nSaving annotations to {csv_path}...")
        
        with open(csv_path, 'w', newline='') as f:
            writer = csv.writer(f)
            # Write header
            header = ['image_name', 'class_label']
            header.extend([f'f{i}' for i in range(1, 18)])
            writer.writerow(header)
            # Write data
            writer.writerows(self.annotations)
        
        print(f"? Annotations saved: {len(self.annotations)} entries")
        
        # Summary statistics
        print("\n" + "="*70)
        print("DATASET GENERATION COMPLETE")
        print("="*70)
        print(f"Total samples generated: {len(self.annotations):,}")
        print(f"Features per sample: 17 (normalized to [0,1])")
        print(f"Classes: 4")
        print(f"  - bullet_hole: {self.num_samples_per_class:,}")
        print(f"  - overlapping_bullet_holes: {self.num_samples_per_class:,}")
        print(f"  - unclear: {self.num_samples_per_class:,}")
        print(f"  - non_bullet: {self.num_samples_per_class:,}")
        print(f"\nOutput directory: {self.output_dir}")
        print(f"Images: {self.images_dir}")
        print(f"Annotations: {csv_path}")
        print("="*70 + "\n")


if __name__ == '__main__':
    # Generate dataset
    output_directory = Path(__file__).parent.parent / 'data' / 'datasets' / 'dataset_ir_final'
    
    generator = IRBacklitDatasetGenerator(
        output_dir=output_directory,
        num_samples_per_class=12500  # 50,000 total
    )
    
    generator.generate_dataset()
