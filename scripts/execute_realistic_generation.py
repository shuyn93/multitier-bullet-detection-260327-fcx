"""
Master Execution Script - Dataset Generation & Validation

This script:
1. Generates the realistic IR bullet hole dataset (v2)
2. Validates the dataset quality
3. Displays sample images
4. Generates quality control report
"""

import sys
import time
from pathlib import Path

def main():
    print("\n" + "=" * 80)
    print("REALISTIC IR BULLET HOLE DATASET - GENERATION & VALIDATION")
    print("=" * 80)
    
    # ---- Step 1: Generate Dataset ----
    print("\n[STEP 1] Generating realistic IR bullet hole dataset...")
    print("-" * 80)
    
    try:
        from scripts.generate_dataset_v2_realistic import generate_dataset
        
        start_time = time.time()
        df = generate_dataset()
        elapsed_time = time.time() - start_time
        
        print(f"\n? Dataset generation completed in {elapsed_time:.1f} seconds")
        print(f"  Total samples: {len(df)}")
        
    except Exception as e:
        print(f"\n? ERROR during dataset generation:")
        print(f"  {str(e)}")
        return False
    
    # ---- Step 2: Validate Dataset ----
    print("\n[STEP 2] Validating dataset quality...")
    print("-" * 80)
    
    try:
        from scripts.validate_realistic_dataset import analyze_dataset_quality, visualize_samples
        
        analyze_dataset_quality("dataset_realistic")
        
        print("\n? Dataset validation completed")
        
    except Exception as e:
        print(f"\n? ERROR during dataset validation:")
        print(f"  {str(e)}")
        return False
    
    # ---- Step 3: Generate Quality Control Visualization ----
    print("\n[STEP 3] Generating quality control visualization...")
    print("-" * 80)
    
    try:
        import matplotlib.pyplot as plt
        
        fig = visualize_samples(num_samples=10, dataset_dir="dataset_realistic")
        
        output_path = Path("dataset_realistic") / "quality_control.png"
        fig.savefig(str(output_path), dpi=100, bbox_inches='tight')
        print(f"\n? Saved visualization to: {output_path}")
        
        plt.close(fig)
        
    except Exception as e:
        print(f"\n? ERROR during visualization:")
        print(f"  {str(e)}")
    
    # ---- Summary ----
    print("\n" + "=" * 80)
    print("EXECUTION COMPLETE")
    print("=" * 80)
    print(f"\nDataset Location: {Path('dataset_realistic').absolute()}")
    print(f"  - Images: {Path('dataset_realistic/images').absolute()}")
    print(f"  - Annotations: {Path('dataset_realistic/annotations.csv').absolute()}")
    print(f"  - QC Report: {Path('dataset_realistic/quality_control.png').absolute()}")
    
    print("\n? ALL STEPS COMPLETED SUCCESSFULLY!")
    print("\nNext steps:")
    print("  1. Review quality_control.png to visually inspect samples")
    print("  2. Verify images match real IR bullet hole characteristics")
    print("  3. Use dataset_realistic/ directory for training")
    
    return True


if __name__ == '__main__':
    success = main()
    sys.exit(0 if success else 1)
