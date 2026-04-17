"""
Quick test: 500 samples (125 per class) to verify progress display
"""

from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).parent))

from generate_phase_batch import OptimizedBatchGenerator

if __name__ == '__main__':
    output_directory = Path(__file__).parent.parent / 'data' / 'datasets' / 'dataset_ir_final'
    
    # Clean up old data
    import shutil
    if output_directory.joinpath('images').exists():
        shutil.rmtree(output_directory / 'images')
    if output_directory.joinpath('annotations.csv').exists():
        output_directory.joinpath('annotations.csv').unlink()
    
    generator = OptimizedBatchGenerator(
        output_dir=output_directory,
        batch_size=100  # Batch size: 100 images
    )
    
    # Quick test: 500 samples (125 per class)
    print("\n?? QUICK TEST: 500 samples (125 per class, batch=100)")
    print("To verify progress display before full generation\n")
    total = generator.generate_full_dataset(num_samples_per_class=125)
    print(f"\n? Test complete: {total:,} samples generated")
    print("Ready for full generation!")
