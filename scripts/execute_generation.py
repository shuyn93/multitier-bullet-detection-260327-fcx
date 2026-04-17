"""
Main dataset generation runner
Designed to run without timeout issues
Logs all output to file for monitoring
"""

import sys
import subprocess
from pathlib import Path
from datetime import datetime
import time

def run_generation():
    """Run the full dataset generation"""
    
    script_path = Path(__file__).parent / 'generate_phase_batch.py'
    output_dir = Path(__file__).parent.parent / 'data' / 'datasets' / 'dataset_ir_final'
    log_file = Path(__file__).parent.parent / 'generation_progress.log'
    
    print(f"?? Dataset Generation Runner")
    print(f"?? Script: {script_path.name}")
    print(f"?? Output: {output_dir}")
    print(f"?? Log: {log_file}")
    print(f"? Started: {datetime.now()}")
    print(f"? Estimated time: ~6.2 hours (50,000 samples)")
    print(f"\n{'='*70}\n")
    
    # Run using subprocess to avoid timeout
    try:
        process = subprocess.Popen(
            [sys.executable, str(script_path)],
            cwd=str(Path(__file__).parent.parent),
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1
        )
        
        # Write to both console and log file
        with open(log_file, 'w') as log:
            log.write(f"Generation started at {datetime.now()}\n")
            log.write("="*70 + "\n\n")
            
            for line in process.stdout:
                print(line, end='')
                log.write(line)
                log.flush()
        
        process.wait()
        
        if process.returncode == 0:
            print(f"\n\n{'='*70}")
            print(f"? Generation completed successfully!")
            print(f"? Finished: {datetime.now()}")
            print(f"?? Check {output_dir} for results")
            print(f"{'='*70}")
        else:
            print(f"\n\n{'='*70}")
            print(f"? Generation failed with return code: {process.returncode}")
            print(f"{'='*70}")
            return False
        
        return True
        
    except Exception as e:
        print(f"\n? Error: {e}")
        return False


if __name__ == '__main__':
    success = run_generation()
    sys.exit(0 if success else 1)
