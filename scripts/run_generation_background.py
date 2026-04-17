"""
Background runner for dataset generation
Saves logs to file
"""

import subprocess
import sys
from pathlib import Path
from datetime import datetime

log_file = Path(__file__).parent / 'generation_log.txt'

print(f"?? Starting dataset generation...")
print(f"?? Log file: {log_file}")

with open(log_file, 'w') as f:
    f.write(f"Dataset Generation Started: {datetime.now()}\n")
    f.write("="*70 + "\n\n")

try:
    # Run the main generator
    result = subprocess.run(
        [sys.executable, 'scripts/generate_phase_batch.py'],
        cwd=Path(__file__).parent.parent,
        capture_output=False,
        text=True
    )
    
    with open(log_file, 'a') as f:
        f.write(f"\nGeneration completed with return code: {result.returncode}\n")
        f.write(f"Completion time: {datetime.now()}\n")
    
    print(f"? Generation complete! Check {log_file}")
    
except Exception as e:
    with open(log_file, 'a') as f:
        f.write(f"\nError: {str(e)}\n")
    print(f"? Error: {str(e)}")
