"""
STEP 2: REALISTIC IR BULLET HOLE GENERATOR
===========================================

Fix for synthetic data generation:
- Remove perfect Gaussian blobs
- Implement realistic irregular shapes
- Add proper intensity clamping
- Simulate jagged edges and internal variation
- Model realistic IR backlit physics

Key improvements:
1. Intensity: 180-230 (no saturation)
2. Shape: Irregular perturbed circle
3. Edges: Jagged/rough boundary
4. Internal: Non-uniform brightness with noise
5. Variations: Small holes, overlaps, unclear cases
"""

import numpy as np
import cv2
from pathlib import Path
from scipy import ndimage
import warnings

warnings.filterwarnings('ignore')


class RealisticBulletHoleGenerator:
    """Generate realistic IR bullet hole images"""
    
    def __init__(self, image_size=256):
        self.H = image_size
        self.W = image_size
        
        # Realistic intensity ranges (NO SATURATION)
        self.BULLET_INTENSITY_RANGE = (180, 230)      # Bright but not white
        self.UNCLEAR_INTENSITY_RANGE = (120, 180)      # Low contrast
        self.ARTIFACT_INTENSITY_RANGE = (80, 200)      # Noisy artifacts
        self.BG_INTENSITY_RANGE = (50, 120)           # Background
        
        # Shape parameters
        self.BULLET_RADIUS_RANGE = (8, 35)            # Small to large
        self.SMALL_HOLE_THRESHOLD = 10                # r < 10 is "small"
    
    # ==================== BACKGROUND GENERATION ====================
    
    def create_ir_background(self):
        """Create non-uniform IR background with realistic texture"""
        # Base uniform background
        bg_intensity = np.random.uniform(*self.BG_INTENSITY_RANGE)
        bg = np.full((self.H, self.W), bg_intensity, dtype=np.float32)
        
        # Add low-frequency gradient (vignetting effect)
        y, x = np.ogrid[0:self.H, 0:self.W]
        cx, cy = self.W / 2, self.H / 2
        dist_from_center = np.sqrt((x - cx)**2 + (y - cy)**2)
        max_dist = np.sqrt(cx**2 + cy**2)
        vignette = 1 - 0.3 * (dist_from_center / max_dist)  # Up to 30% darker at edges
        bg *= vignette
        
        # Add Perlin-like noise for texture
        for scale in [64, 32, 16, 8]:
            noise = np.random.uniform(-1, 1, (self.H // scale, self.W // scale))
            noise = cv2.resize(noise.astype(np.float32), (self.W, self.H), 
                             interpolation=cv2.INTER_LINEAR)
            amplitude = 5 * (64 / scale) * 0.5  # Decreasing amplitude per octave
            bg += noise * amplitude
        
        # Add sensor noise
        sensor_noise = np.random.normal(0, 3, bg.shape)
        bg += sensor_noise
        
        # Smooth and clip
        bg = cv2.GaussianBlur(bg, (31, 31), 10)
        bg = np.clip(bg, *self.BG_INTENSITY_RANGE)
        
        return bg
    
    # ==================== REALISTIC SHAPE GENERATION ====================
    
    def create_irregular_circle_mask(self, center, radius, perturbation_scale=0.15):
        """
        Create irregular circle by perturbing contour with radial noise.
        Simulates bullet tearing and jagged edges.
        """
        y, x = np.ogrid[0:self.H, 0:self.W]
        
        # Distance from center
        dx = x - center[0]
        dy = y - center[1]
        dist = np.sqrt(dx**2 + dy**2)
        
        # Create angle map for polar coordinates
        angles = np.arctan2(dy, dx)
        
        # Generate radial perturbation (spiky/jagged)
        num_spikes = np.random.randint(12, 24)  # Number of irregular bumps
        angle_samples = np.linspace(0, 2*np.pi, num_spikes, endpoint=False)
        
        # Create smooth interpolation of perturbations
        perturbations = np.random.uniform(1 - perturbation_scale, 
                                         1 + perturbation_scale, 
                                         num_spikes)
        
        # Interpolate to all angles
        from scipy.interpolate import interp1d
        perturb_func = interp1d(angle_samples, perturbations, 
                               kind='cubic', fill_value='extrapolate')
        perturb_map = perturb_func(angles)
        
        # Perturbed radius
        perturbed_radius = radius * perturb_map
        
        # Create mask: circle is where dist <= perturbed_radius
        mask = (dist <= perturbed_radius).astype(np.float32)
        
        return mask
    
    def add_edge_roughness(self, mask, roughness=0.2):
        """Add micro-roughness to edges using morphological operations"""
        # Dilate and erode to create rough edges
        kernel_size = 3
        kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (kernel_size, kernel_size))
        
        # Add small random perturbations at boundary
        edge = cv2.morphologyEx(mask, cv2.MORPH_GRADIENT, kernel)
        noise_mask = np.random.uniform(0, 1, mask.shape)
        edge_perturbation = (noise_mask < roughness).astype(np.float32) * edge
        
        mask = np.clip(mask + edge_perturbation * 0.3, 0, 1)
        
        return mask
    
    def add_internal_variation(self, mask, intensity, variation_scale=0.15):
        """
        Add non-uniform internal brightness.
        Simulates material variation and reflection effects.
        """
        # Create base intensity map (not uniform!)
        intensity_map = np.ones_like(mask) * intensity
        
        # Add large-scale internal variation (material inhomogeneity)
        large_var = np.random.uniform(-1, 1, (self.H // 16, self.W // 16))
        large_var = cv2.resize(large_var.astype(np.float32), (self.W, self.H),
                              interpolation=cv2.INTER_LINEAR)
        large_var = (large_var - large_var.min()) / (large_var.max() - large_var.min() + 1e-6)
        intensity_map *= (1 + variation_scale * (2 * large_var - 1))
        
        # Add small-scale noise (surface roughness)
        small_noise = np.random.normal(0, intensity * 0.05, mask.shape)
        intensity_map += small_noise
        
        # Apply mask and clamp
        intensity_map = intensity_map * mask
        intensity_map = np.clip(intensity_map, 0, 255)
        
        return intensity_map
    
    # ==================== BULLET HOLE VARIATIONS ====================
    
    def generate_bullet_hole(self, bg):
        """Generate a single, clear bullet hole (realistic)"""
        bg_copy = bg.copy()
        
        # Random position (avoid edges)
        margin = 50
        cx = np.random.randint(margin, self.W - margin)
        cy = np.random.randint(margin, self.H - margin)
        center = (cx, cy)
        
        # Bullet hole parameters
        radius = np.random.uniform(*self.BULLET_RADIUS_RANGE)
        intensity = np.random.uniform(*self.BULLET_INTENSITY_RANGE)
        
        # Create irregular mask (NOT perfect circle!)
        mask = self.create_irregular_circle_mask(center, radius, perturbation_scale=0.12)
        mask = self.add_edge_roughness(mask, roughness=0.15)
        
        # Create intensity map with internal variation (NOT uniform!)
        intensity_map = self.add_internal_variation(mask, intensity, variation_scale=0.1)
        
        # Blend onto background (not simple addition!)
        # Use blending to simulate physics
        alpha = mask * 0.9  # Partial transparency at edges
        result = bg_copy * (1 - alpha) + intensity_map * alpha
        
        result = np.clip(result, 0, 255)
        
        return result.astype(np.uint8)
    
    def generate_overlapping_holes(self, bg):
        """Generate overlapping bullet holes (realistic merge)"""
        result = bg.copy()
        num_holes = np.random.randint(2, 4)
        
        for _ in range(num_holes):
            margin = 40
            cx = np.random.randint(margin, self.W - margin)
            cy = np.random.randint(margin, self.H - margin)
            center = (cx, cy)
            
            radius = np.random.uniform(6, 25)
            intensity = np.random.uniform(*self.BULLET_INTENSITY_RANGE)
            
            # Irregular shape
            mask = self.create_irregular_circle_mask(center, radius, perturbation_scale=0.1)
            mask = self.add_edge_roughness(mask, roughness=0.12)
            
            intensity_map = self.add_internal_variation(mask, intensity, variation_scale=0.08)
            
            # Accumulate
            alpha = mask * 0.8
            result = result * (1 - alpha) + intensity_map * alpha
        
        result = np.clip(result, 0, 255)
        return result.astype(np.uint8)
    
    def generate_unclear_hole(self, bg):
        """Generate unclear/low-contrast bullet hole (hard to detect)"""
        result = bg.copy()
        
        margin = 50
        cx = np.random.randint(margin, self.W - margin)
        cy = np.random.randint(margin, self.H - margin)
        center = (cx, cy)
        
        radius = np.random.uniform(5, 20)
        # LOWER intensity = low contrast
        intensity = np.random.uniform(*self.UNCLEAR_INTENSITY_RANGE)
        
        # More perturbation = less clear shape
        mask = self.create_irregular_circle_mask(center, radius, perturbation_scale=0.2)
        mask = self.add_edge_roughness(mask, roughness=0.2)
        
        # More internal variation
        intensity_map = self.add_internal_variation(mask, intensity, variation_scale=0.15)
        
        # Add extra noise to make it unclear
        noise = np.random.normal(0, 5, intensity_map.shape)
        intensity_map = intensity_map + noise * mask
        
        alpha = mask * 0.7
        result = result * (1 - alpha) + intensity_map * alpha
        
        result = np.clip(result, 0, 255)
        return result.astype(np.uint8)
    
    def generate_non_bullet_artifact(self, bg):
        """Generate non-bullet noise/artifact"""
        result = bg.copy()
        
        artifact_type = np.random.choice(['noise', 'streak', 'speckle'])
        
        if artifact_type == 'noise':
            # Random Gaussian noise
            noise = np.random.normal(0, 10, result.shape)
            result = result + noise
        
        elif artifact_type == 'streak':
            # Random bright streaks (not circular!)
            num_streaks = np.random.randint(1, 3)
            for _ in range(num_streaks):
                y1, y2 = np.random.randint(0, self.H, 2)
                x1, x2 = np.random.randint(0, self.W, 2)
                intensity = np.random.randint(100, 200)
                thickness = np.random.randint(1, 4)
                cv2.line(result, (x1, y1), (x2, y2), intensity, thickness)
        
        else:  # speckle
            # Random small artifacts (irregular shapes)
            num_speckles = np.random.randint(3, 8)
            for _ in range(num_speckles):
                cx = np.random.randint(20, self.W - 20)
                cy = np.random.randint(20, self.H - 20)
                radius = np.random.randint(2, 8)
                intensity = np.random.randint(100, 180)
                
                # Irregular mask (not circular)
                mask = self.create_irregular_circle_mask((cx, cy), radius, 
                                                         perturbation_scale=0.25)
                intensity_map = intensity * mask
                
                alpha = mask * 0.6
                result = result * (1 - alpha) + intensity_map * alpha
        
        result = np.clip(result, 0, 255)
        return result.astype(np.uint8)
    
    # ==================== VALIDATION & ANALYSIS ====================
    
    def analyze_generated_image(self, image, label):
        """Analyze generated image for realism"""
        stats = {
            'label': label,
            'max_intensity': float(image.max()),
            'mean_intensity': float(image.mean()),
            'min_intensity': float(image.min()),
            'is_saturated': image.max() == 255,
            'has_varied_edges': True,  # Check later
            'peak_at_center': False,   # Check later
        }
        
        # Check if it looks realistic
        is_realistic = (
            stats['max_intensity'] <= 230 and  # Not saturated
            stats['max_intensity'] >= 150 and  # Not too dark
            stats['is_saturated'] == False
        )
        
        stats['is_realistic'] = is_realistic
        
        return stats
    
    # ==================== BATCH GENERATION ====================
    
    def generate_batch(self, num_samples=100):
        """Generate batch of realistic images"""
        print(f"Generating {num_samples} realistic bullet hole images...")
        
        images = []
        labels = []
        
        for i in range(num_samples):
            # Create background
            bg = self.create_ir_background()
            
            # Random class distribution
            cls = np.random.choice(['bullet_hole', 'overlapping', 'unclear', 'non_bullet'],
                                  p=[0.4, 0.2, 0.2, 0.2])
            
            if cls == 'bullet_hole':
                image = self.generate_bullet_hole(bg)
            elif cls == 'overlapping':
                image = self.generate_overlapping_holes(bg)
            elif cls == 'unclear':
                image = self.generate_unclear_hole(bg)
            else:  # non_bullet
                image = self.generate_non_bullet_artifact(bg)
            
            images.append(image)
            labels.append(cls)
            
            # Print stats for first few
            if i < 5:
                stats = self.analyze_generated_image(image, cls)
                print(f"  [{i}] {cls:15} | max={stats['max_intensity']:.0f} "
                      f"mean={stats['mean_intensity']:.0f} realistic={stats['is_realistic']}")
        
        return images, labels


# ==================== DEMO / TESTING ====================

if __name__ == '__main__':
    import matplotlib.pyplot as plt
    
    # Create generator
    gen = RealisticBulletHoleGenerator(image_size=256)
    
    # Generate batch
    images, labels = gen.generate_batch(num_samples=20)
    
    # Show first 4
    fig, axes = plt.subplots(2, 2, figsize=(10, 10))
    for i, ax in enumerate(axes.flat):
        ax.imshow(images[i], cmap='hot')
        ax.set_title(f"{labels[i]} | max={images[i].max()}")
        ax.axis('off')
    
    plt.tight_layout()
    plt.savefig('realistic_bullet_holes_sample.png', dpi=100, bbox_inches='tight')
    print("? Sample saved: realistic_bullet_holes_sample.png")
    
    # Validate all
    print("\n=== VALIDATION SUMMARY ===")
    for img, lbl in zip(images, labels):
        stats = gen.analyze_generated_image(img, lbl)
        status = "? REALISTIC" if stats['is_realistic'] else "? NOT REALISTIC"
        print(f"{status:15} {lbl:15} max={stats['max_intensity']:.0f}")
