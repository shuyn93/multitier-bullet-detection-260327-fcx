"""
Quick test with 100 samples total (25 per class)
"""

from pathlib import Path
import sys

# Add scripts to path
sys.path.insert(0, str(Path(__file__).parent))

from generate_dataset_batch import BatchIRDatasetGenerator

if __name__ == '__main__':
    output_directory = Path(__file__).parent.parent / 'data' / 'datasets' / 'dataset_ir_final'
    
    # Test with small batch: 25 samples per class, batch size 10
    generator = BatchIRDatasetGenerator(
        output_dir=output_directory,
        num_samples_per_class=25,
        batch_size=10
    )
    
    print("Starting QUICK TEST with 100 total samples...")
    generator.generate_dataset()
