"""
Ultra-fast test: 20 samples total (5 per class, batch=5)
Just to verify structure works
"""

import numpy as np
import cv2
from pathlib import Path
import csv
import time

print("Starting ultra-fast test...")
start = time.time()

output_dir = Path(__file__).parent.parent / 'data' / 'datasets' / 'dataset_ir_final'
images_dir = output_dir / 'images'
images_dir.mkdir(parents=True, exist_ok=True)

# Generate 20 samples (5 per class)
annotations = []
image_id = 0

for class_idx, class_name in enumerate(['bullet_hole', 'overlapping_bullet_holes', 'unclear', 'non_bullet']):
    print(f"\nGenerating {class_name}...")
    
    for sample_idx in range(5):
        # Create random image
        image = np.random.randint(50, 150, (256, 256), dtype=np.uint8)
        
        # Save image
        img_path = images_dir / f"{image_id:06d}.png"
        cv2.imwrite(str(img_path), image)
        
        # Create dummy features (17 values)
        features = [np.random.random() for _ in range(17)]
        
        # Create annotation
        annotation = [f"{image_id:06d}", class_name] + features
        annotations.append(annotation)
        
        image_id += 1
        print(f"  {sample_idx + 1}/5", end='\r')

# Save annotations
csv_path = output_dir / 'annotations.csv'
with open(csv_path, 'w', newline='') as f:
    writer = csv.writer(f)
    header = ['image_name', 'class_label'] + [f'f{i}' for i in range(1, 18)]
    writer.writerow(header)
    writer.writerows(annotations)

elapsed = time.time() - start
print(f"\n? Test complete! Generated 20 samples in {elapsed:.1f}s")
print(f"  Images saved to: {images_dir}")
print(f"  Annotations: {csv_path}")
print(f"  Total files: {len(list(images_dir.glob('*.png')))}")
