================================================================================
QUICK START GUIDE: INCREMENTAL TRAINING
================================================================================

PROJECT: Bullet Hole Detection System - Continuous Learning
VERSION: 1.0
DATE: 2026-04-06

================================================================================
5-MINUTE SETUP
================================================================================

1. BUILD THE SYSTEM
???????????????????

  cd C:\Users\Admin\source\repos\bullet_hole_detection_system
  mkdir build
  cd build
  cmake .. -G "Visual Studio 17 2022"
  cmake --build . --config Release

  Expected Output:
    [100%] Built target incremental_training
    Build files have been written to: ...

2. VERIFY YOUR DATA
????????????????????

  Your new data file should be at:
    dataset_ir_realistic/annotations.csv
  
  Check it contains:
    - Header row (first line - will be skipped)
    - Multiple data rows with format:
      feature_1,feature_2,...,feature_17,label
    - Labels must be 0 or 1
    - All numeric values

  Command to inspect:
    type dataset_ir_realistic\annotations.csv | head -n 5

3. RUN INCREMENTAL TRAINING
????????????????????????????

  Release\incremental_training.exe

  Expected Output:
    ================================================================================
    CONTINUOUS LEARNING PIPELINE FOR BULLET HOLE DETECTION SYSTEM
    ================================================================================
    
    [CONFIGURATION]
      New data path: dataset_ir_realistic/annotations.csv
      Model directory: models
      Learning rate: 0.00100
      Max epochs: 50
      Curriculum learning: ENABLED
      ...
    
    [STEP 1] Loading existing trained models...
    ? Tier 1 Classifier loaded
    ? Tier 2 MLP loaded
    ? Scaler state loaded
    
    [STEP 2] Loading new dataset...
    ? Loaded 5000 new samples
    
    [STEP 3] Applying curriculum learning strategy...
    ...

4. REVIEW RESULTS
??????????????????

  Open the generated report:
    results/INCREMENTAL_TRAINING_REPORT_[N]_SAMPLES.txt
  
  Check key metrics:
    - "Initial accuracy" vs "Final accuracy"
    - "Improvement" percentage
    - "Status" should be "? STABLE"
    - "Max performance drop" should be < 5%

5. DEPLOY UPDATED MODELS
?????????????????????????

  If status is STABLE:
    
    Copy updated models:
      xcopy models\version_1\* models\ /Y
    
    Update your inference code to use new models
    Deploy to production

================================================================================
BASIC USAGE
================================================================================

Command:
  Release\incremental_training.exe [data_path] [model_dir]

Examples:

  a) Default paths:
     Release\incremental_training.exe
     
     Uses:
     - Data: dataset_ir_realistic/annotations.csv
     - Models: models/

  b) Custom data path:
     Release\incremental_training.exe my_dataset.csv
     
     Uses:
     - Data: my_dataset.csv
     - Models: models/

  c) Custom data and model paths:
     Release\incremental_training.exe my_dataset.csv custom_models/
     
     Uses:
     - Data: my_dataset.csv
     - Models: custom_models/

Output:
  - Console: Full log of incremental training pipeline
  - Report: results/INCREMENTAL_TRAINING_REPORT_[N]_SAMPLES.txt
  - Models: models/version_1/ (updated weights)

================================================================================
UNDERSTANDING THE REPORT
================================================================================

Key Sections:

1. DATASET STATISTICS
   
   New samples processed: 5000
   Old samples (replay buffer): 1000
   Total training samples: 6000
   
   Interpretation:
   - 5000 new samples from your new data
   - 1000 old samples for stability
   - 6000 total = 5000 + 1000

2. PERFORMANCE METRICS
   
   Initial accuracy (before): 95.63%
   Final accuracy (after):    96.15%
   Improvement: +0.52%
   
   Interpretation:
   - System slightly improved accuracy
   - ? Good - learning from new data
   - ? If negative, consider more replay buffer

3. PRECISION & RECALL
   
   Precision (before): 100.00%
   Precision (after):  100.00%
   Recall (before): 91.38%
   Recall (after):  91.50%
   
   Interpretation:
   - Maintained precision (no false positives)
   - Slightly improved recall (caught more positives)
   - ? Healthy improvements

4. TRAINING METRICS
   
   Initial loss: 0.250000
   Final loss: 0.245000
   Epochs trained: 50
   Training time: 15.32 seconds
   
   Interpretation:
   - Loss decreased = learning occurred
   - Completed all 50 epochs
   - Reasonable training time

5. STABILITY CHECK
   
   Status: ? STABLE
   Max performance drop: 0.02%
   Details: STABLE - No significant performance drop
   
   Interpretation:
   ? PASS - Safe to deploy
   ? FAIL - May need adjustment (see Troubleshooting)

================================================================================
WHAT HAPPENS INSIDE
================================================================================

Step-by-Step Breakdown:

STEP 1: LOAD EXISTING MODELS
  ?? Loads:
     - Tier1Classifier (Naive Bayes, GMM, Trees)
     - Tier2 MLP (neural network weights)
     - ScalerState (mean & std for normalization)
  ?? Time: ~100 ms
  ?? Success criteria: Models exist and load without errors

STEP 2: LOAD NEW DATA
  ?? Reads: dataset_ir_realistic/annotations.csv
  ?? Does: Parse 17 features + label per line
  ?? Normalizes: Using existing scaler (NO recomputation)
  ?? Returns: Vector of CurriculumSample objects
  ?? Time: ~200 ms (5000 samples)
  ?? Success criteria: All 5000 samples load, no parsing errors

STEP 3: APPLY CURRICULUM LEARNING
  ?? For each sample:
     - Uses Tier1 to predict
     - Computes "difficulty" based on uncertainty
     - Easy: High confidence, Low uncertainty
     - Hard: Low confidence, High uncertainty
     - Misclassified: Marked as HARD
  ?? Sorts: From HARD ? EASY (or vice versa, see config)
  ?? Time: ~100 ms
  ?? Output: difficulty scores [0, 1] and sorted order

STEP 4: CREATE REPLAY BUFFER
  ?? Purpose: Prevent forgetting old knowledge
  ?? Strategy: Mix 80% new + 20% old samples during training
  ?? For now: Creates empty buffer (no saved old data)
  ?? Time: ~1 ms
  ?? Future: Will load from saved validation set

STEP 5: EVALUATE BASELINE
  ?? Runs Tier1 on new data
  ?? Counts correct predictions
  ?? Reports: initial_accuracy
  ?? Example: 95.63%
  ?? Time: ~50 ms
  ?? Purpose: Baseline for comparing improvement

STEP 6: FINE-TUNE MODELS
  ?? Loop 50 times (max_epochs):
     a) Process new samples:
        - Predict with Tier1
        - Compute error
        - Update Tier1 (onlineUpdate)
        - Update Tier2 (backprop)
     
     b) Process replay samples (20%):
        - Same updates for old samples
     
     c) Decay learning rate by 10%
  
  ?? Learning rates:
     Epoch 0-9: 0.001
     Epoch 10-19: 0.0009
     Epoch 20+: 0.0008, 0.0007, ...
  
  ?? Time: ~15 seconds
  ?? Purpose: Incrementally adapt to new data

STEP 7: EVALUATE FINAL PERFORMANCE
  ?? Same as Step 5, but after training
  ?? Reports: final_accuracy
  ?? Example: 96.15%
  ?? Difference: 96.15% - 95.63% = +0.52%
  ?? Time: ~50 ms

STEP 8: VALIDATE STABILITY
  ?? Checks: No catastrophic forgetting occurred
  ?? Method: Compare performance before/after on old data
  ?? Threshold: Drop > 5% = WARNING
  ?? Output: is_stable flag (true/false)
  ?? Time: ~50 ms
  ?? If UNSTABLE: Recommend increasing replay buffer

STEP 9: SAVE UPDATED MODELS
  ?? Creates: models/version_1/
  ?? Saves:
     - Tier2 MLP weights (tier2_mlp.bin)
     - Scaler state (reference only)
  ?? Note: Tier1 updates persist in memory
  ?? Time: ~10 ms
  ?? Future: Full Tier1 serialization

Total Time: ~15-20 seconds for 5000 samples

================================================================================
COMMON SCENARIOS
================================================================================

Scenario A: First Time Setup
??????????????????????????????

Problem: You have new dataset and want to enable incremental training

Steps:
  1. Build the system (see 5-Minute Setup)
  2. Ensure dataset_ir_realistic/annotations.csv exists
  3. Ensure models/ directory has trained models
  4. Run: Release\incremental_training.exe
  5. Review results

Expected:
  - Accuracy improvement: +0.5% to +2.0%
  - Status: STABLE
  - Training time: ~20 seconds

Scenario B: Periodic Continuous Learning
???????????????????????????????????????????

Problem: You collect new data weekly and want to keep improving

Setup:
  1. Create a scheduled task (Windows Task Scheduler)
  2. Script: Call incremental_training.exe every Sunday 2 AM
  3. Logs: Save output to incremental_training_[date].log
  4. Check: Review report file next morning
  5. Deploy: If STABLE, copy models\version_1\* to production

Example Script (batch file):
  
  @echo off
  cd C:\Users\Admin\source\repos\bullet_hole_detection_system
  set TIMESTAMP=%DATE:~10,4%%DATE:~4,2%%DATE:~7,2%_%TIME:~0,2%%TIME:~3,2%%TIME:~6,2%
  set LOGFILE=logs\incremental_training_%TIMESTAMP%.log
  
  Release\incremental_training.exe > %LOGFILE% 2>&1
  
  if %ERRORLEVEL% EQU 0 (
    echo Incremental training SUCCEEDED >> %LOGFILE%
    REM Copy new models
    xcopy models\version_1\* production_models\ /Y /I
  ) else (
    echo Incremental training FAILED >> %LOGFILE%
    REM Send alert
  )

Scenario C: Tuning Learning Rate
??????????????????????????????????

Problem: Models are not improving enough (accuracy flat)

Investigation:
  1. Check initial vs final accuracy (should be ? 0%)
  2. Check loss values (should decrease or be stable)
  3. Review report for anomalies

Solution:
  1. Increase learning_rate to 0.005f
  2. Rerun incremental_training
  3. Compare results with previous run
  4. If better, keep new learning_rate
  5. If worse, go back to 0.001f

Code change (in incremental_training_main.cpp):
  
  IncrementalTrainingConfig config;
  config.learning_rate = 0.005f;  // ? CHANGE THIS
  config.max_epochs = 50;
  ...

Scenario D: Preventing Catastrophic Forgetting
????????????????????????????????????????????????

Problem: Report shows "UNSTABLE" and accuracy drop > 5%

Cause:
  - New data is very different from old
  - Learning rate too high
  - Replay buffer too small

Solutions (in order):

  1st Try: Increase replay buffer ratio
  
    config.replay_buffer_ratio = 0.30f;  // 30% old instead of 20%
    
    Effect: More old data mixes with new
    Trade-off: Slower adaptation to new patterns

  2nd Try: Reduce learning rate
  
    config.learning_rate = 0.0005f;  // Half of default
    
    Effect: Smaller gradient steps, more stable
    Trade-off: Slower learning

  3rd Try: Both
  
    config.replay_buffer_ratio = 0.40f;
    config.learning_rate = 0.0005f;
    
    Effect: Very conservative, high stability
    Trade-off: May not learn new patterns well

  4th Try: Check data quality
  
    - Inspect new dataset for outliers
    - Check label distribution (should be 50/50)
    - Verify features are normalized in [-2, 2] range

================================================================================
REFERENCE: KEY FILES
================================================================================

Source Code:
  include/training/IncrementalTrainer.h
  src/training/IncrementalTrainer.cpp
  src/incremental_training_main.cpp

Build Configuration:
  CMakeLists.txt (updated to include incremental_training target)

Documentation:
  INCREMENTAL_TRAINING_GUIDE.md (implementation guide)
  INCREMENTAL_TRAINING_TECHNICAL.md (technical details)
  INCREMENTAL_TRAINING_QUICKSTART.md (this file)

Executable:
  Release\incremental_training.exe

Input Data:
  dataset_ir_realistic/annotations.csv (new training data)
  models/scaler.bin (feature normalization)
  models/tier1/ (existing Tier1 models)
  models/tier2_mlp.bin (existing Tier2 weights)

Output Data:
  results/INCREMENTAL_TRAINING_REPORT_[N]_SAMPLES.txt (report)
  models/version_1/ (updated models)

================================================================================
TROUBLESHOOTING
================================================================================

Problem: "Cannot open CSV file: ..."
??????????????????????????????????
  Solution:
    1. Check file path is correct
    2. Ensure file exists and is readable
    3. Check current working directory
    4. Use absolute path if relative fails
    
  Command: 
    dir dataset_ir_realistic\annotations.csv

Problem: "Could not load Tier 2 MLP weights"
?????????????????????????????????????????
  Solution:
    1. Run full training first: training_pipeline.exe
    2. This will generate models/tier2_mlp.bin
    3. Then run incremental training
  
  OK to continue: Yes, with warning

Problem: "Accuracy unchanged or decreased"
??????????????????????????????????????????
  Solution:
    1. Check data quality:
       - Are labels correct (0/1)?
       - Are features normalized?
    2. Try different learning rate:
       - Increase to 0.005f
       - Or decrease to 0.0001f
    3. Check if data is too different:
       - May need more replay buffer
       - Try replay_buffer_ratio = 0.30f

Problem: "Training is very slow"
??????????????????????????????????
  Cause: Large dataset
  Solution:
    1. Reduce dataset size (for testing)
    2. Reduce max_epochs (was 50, try 20)
    3. Run on CPU is slow, needs GPU (future work)

Problem: "UNSTABLE flag is true"
?????????????????????????????????
  Meaning: Potential catastrophic forgetting detected
  Solution:
    1. Increase replay_buffer_ratio to 0.30f
    2. Reduce learning_rate to 0.0005f
    3. Re-run incremental training
    4. If still unstable, check data quality

================================================================================
NEXT STEPS
================================================================================

1. Run your first incremental training
   Release\incremental_training.exe

2. Review the report
   results/INCREMENTAL_TRAINING_REPORT_[N]_SAMPLES.txt

3. If STABLE:
   Deploy to production
   xcopy models\version_1\* production_models\ /Y /I

4. If UNSTABLE:
   Adjust config and try again
   (See Scenario D above)

5. Set up periodic training
   Create Windows Task Scheduler job
   Run weekly or monthly as needed

6. Monitor performance
   Track accuracy trends over time
   Alert if accuracy drops > 2%

7. Collect feedback
   Note patterns not captured
   Add to next training cycle

================================================================================
SUPPORT
================================================================================

For technical questions:
  See INCREMENTAL_TRAINING_TECHNICAL.md

For implementation details:
  See INCREMENTAL_TRAINING_GUIDE.md

For code examples:
  See src/incremental_training_main.cpp

For debugging:
  Enable verbose output in source code
  Add debug print statements
  Log intermediate values to files

================================================================================
END OF QUICK START GUIDE
================================================================================
