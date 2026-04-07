#!/usr/bin/env python
"""
Bullet Hole Dataset Generator - Execution Wrapper
Runs generation with real-time output capture
"""

import subprocess
import sys
import time
from pathlib import Path

print("=" * 80)
print("DATASET GENERATION EXECUTOR")
print("=" * 80)
print(f"Python: {sys.version.split()[0]}")
print(f"Working directory: {Path.cwd().absolute()}")
print()

# Start generation in subprocess with output capture
print("Starting dataset generation...")
print("This will take approximately 30-50 minutes for 10,000 samples.")
print()

start_time = time.time()

try:
    # Run the generation script
    result = subprocess.run(
        [sys.executable, 'generate_dataset.py'],
        capture_output=False,
        text=True,
        timeout=3600  # 1 hour timeout
    )
    
    elapsed = time.time() - start_time
    
    if result.returncode == 0:
        print("\n" + "=" * 80)
        print("? DATASET GENERATION COMPLETED SUCCESSFULLY")
        print("=" * 80)
        print(f"Total time: {elapsed:.1f} seconds ({elapsed/60:.1f} minutes)")
    else:
        print(f"\n? Generation failed with return code {result.returncode}")
        sys.exit(1)

except subprocess.TimeoutExpired:
    print("\n? Generation timed out after 1 hour")
    sys.exit(1)
except Exception as e:
    print(f"\n? Error: {e}")
    sys.exit(1)
