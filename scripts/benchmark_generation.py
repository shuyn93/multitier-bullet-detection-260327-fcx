"""
Benchmark test: 1000 samples to estimate full generation time
"""

import numpy as np
import cv2
import time
from pathlib import Path
from scipy import ndimage
import warnings

warnings.filterwarnings('ignore')

class QuickBenchmark:
    def __init__(self, output_dir, samples=100):
        self.output_dir = Path(output_dir)
        self.images_dir = self.output_dir / 'images'
        self.samples = samples
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
        self.images_dir.mkdir(parents=True, exist_ok=True)
    
    def normalize_feature(self, value, feature_name):
        if feature_name not in self.feature_ranges:
            return np.clip(value, 0, 1)
        min_val, max_val = self.feature_ranges[feature_name]
        if max_val == min_val:
            return 0.5
        normalized = (value - min_val) / (max_val - min_val)
        return np.clip(normalized, 0, 1)
    
    def create_ir_background(self, intensity_range=(50, 120)):
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
        spot = np.zeros((self.image_height, self.image_width), dtype=np.float32)
        y, x = np.ogrid[:self.image_height, :self.image_width]
        dist = np.sqrt((x - center[0])**2 + (y - center[1])**2)
        
        if fade_type == 'gaussian':
            mask = np.exp(-(dist**2) / (2 * radius**2))
        else:
            mask = np.maximum(0, 1 - (dist / radius)**2)
        
        return intensity * mask
    
    def generate_bullet_hole(self, bg):
        cx = np.random.randint(50, self.image_width - 50)
        cy = np.random.randint(50, self.image_height - 50)
        radius = np.random.uniform(8, 35)
        intensity = np.random.uniform(200, 255)
        spot = self.create_bright_spot((cx, cy), radius, intensity)
        spot = cv2.GaussianBlur(spot, (5, 5), 1)
        image = bg + spot
        return np.clip(image, 0, 255).astype(np.uint8)
    
    def extract_17_features(self, image):
        img_float = image.astype(np.float32) / 255.0
        threshold = np.mean(image) + np.std(image) * 0.5
        binary = (image > threshold).astype(np.uint8)
        contours, _ = cv2.findContours(binary, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        
        features = {
            'area': 0, 'circularity': 0, 'solidity': 0, 'elongation': 0,
            'radial_symmetry': 0, 'radial_gradient_consistency': 0,
            'snr': 0, 'entropy': 0, 'ring_energy': 0,
            'sharpness': 0, 'laplacian_density': 0, 'phase_coherence': 0,
            'mean_intensity': 0, 'intensity_contrast': 0, 'variance': 0,
            'edge_density': 0, 'local_uniformity': 0
        }
        
        if len(contours) > 0:
            cnt = max(contours, key=cv2.contourArea)
            area = cv2.contourArea(cnt)
            perimeter = cv2.arcLength(cnt, True)
            
            features['area'] = self.normalize_feature(area, 'area')
            features['circularity'] = 4 * np.pi * area / (perimeter ** 2) if perimeter > 0 else 0
            
            hull = cv2.convexHull(cnt)
            hull_area = cv2.contourArea(hull)
            features['solidity'] = area / hull_area if hull_area > 0 else 0
        
        mean_intensity = np.mean(image)
        std_intensity = np.std(image)
        features['snr'] = self.normalize_feature(mean_intensity / (std_intensity + 1e-6) * 10, 'snr')
        features['mean_intensity'] = self.normalize_feature(mean_intensity, 'mean_intensity')
        features['variance'] = self.normalize_feature(np.var(image), 'variance')
        
        hist, _ = np.histogram(image, bins=256, range=(0, 256))
        hist = hist / hist.sum()
        entropy = -np.sum(hist[hist > 0] * np.log2(hist[hist > 0])) if np.any(hist) else 0
        features['entropy'] = self.normalize_feature(entropy, 'entropy')
        
        gradient_x = cv2.Sobel(image, cv2.CV_32F, 1, 0, ksize=3)
        gradient_y = cv2.Sobel(image, cv2.CV_32F, 0, 1, ksize=3)
        gradient_magnitude = np.sqrt(gradient_x**2 + gradient_y**2)
        features['radial_gradient_consistency'] = self.normalize_feature(
            np.std(gradient_magnitude) if gradient_magnitude.size > 0 else 0,
            'radial_gradient_consistency'
        )
        
        edges = cv2.Canny(image, 50, 150)
        features['ring_energy'] = self.normalize_feature(np.sum(edges) / 255, 'ring_energy')
        
        laplacian = cv2.Laplacian(image, cv2.CV_32F)
        features['sharpness'] = self.normalize_feature(np.var(laplacian), 'sharpness')
        features['laplacian_density'] = self.normalize_feature(
            np.sum(np.abs(laplacian) > 10) / image.size,
            'laplacian_density'
        )
        
        fft = np.fft.fft2(img_float)
        fft_shift = np.fft.fftshift(fft)
        phase = np.angle(fft_shift)
        phase_coherence = np.abs(np.mean(np.exp(1j * phase)))
        features['phase_coherence'] = self.normalize_feature(phase_coherence, 'phase_coherence')
        
        features['intensity_contrast'] = self.normalize_feature(
            np.max(image) - np.min(image),
            'intensity_contrast'
        )
        
        local_var = ndimage.generic_filter(image.astype(np.float32), np.var, size=9)
        features['local_uniformity'] = self.normalize_feature(
            1.0 / (1.0 + np.mean(local_var) / 100),
            'local_uniformity'
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
    
    def benchmark(self):
        print(f"\n{'='*70}")
        print(f"BENCHMARK: {self.samples} samples")
        print(f"Image size: {self.image_height}x{self.image_width}")
        print(f"{'='*70}\n")
        
        start = time.time()
        
        for i in range(self.samples):
            bg = self.create_ir_background()
            image = self.generate_bullet_hole(bg)
            features = self.extract_17_features(image)
            
            img_path = self.images_dir / f"{i:06d}.png"
            cv2.imwrite(str(img_path), image)
            
            if (i + 1) % 20 == 0 or i == self.samples - 1:
                elapsed = time.time() - start
                per_sample = elapsed / (i + 1)
                remaining = (self.samples - i - 1) * per_sample
                print(f"  {i+1}/{self.samples} | Time: {elapsed:.1f}s | Remaining: {remaining:.1f}s | Speed: {per_sample:.3f}s/img", end='\r')
        
        total_time = time.time() - start
        per_sample = total_time / self.samples
        
        print(f"\n{'='*70}")
        print(f"? Benchmark complete!")
        print(f"{'='*70}")
        print(f"Total time: {total_time:.1f}s")
        print(f"Per sample: {per_sample:.3f}s")
        print(f"Speed: {1/per_sample:.1f} samples/second")
        print(f"\nEstimated time for 50,000 samples:")
        estimated = 50000 * per_sample
        hours = estimated / 3600
        print(f"  {hours:.1f} hours ({estimated:.0f}s)")
        print(f"{'='*70}\n")


if __name__ == '__main__':
    output_directory = Path(__file__).parent.parent / 'data' / 'datasets' / 'dataset_ir_final'
    
    benchmark = QuickBenchmark(output_dir=output_directory, samples=100)
    benchmark.benchmark()
