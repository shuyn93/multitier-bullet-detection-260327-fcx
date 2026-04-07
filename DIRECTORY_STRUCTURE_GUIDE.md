# Project Directory Structure Guide

## Quick Reference

```
bullet_hole_detection_system/
?
??? .github/                          # GitHub workflows and configuration
??? .vs/                              # Visual Studio settings
?
??? build/                            # CMake build outputs
?   ??? CMakeCache.txt               # CMake configuration
?   ??? CMakeLists.txt               # CMake configuration
?   ??? cmake_install.cmake          # Installation script
?   ??? *.vcxproj                    # Visual Studio projects
?   ??? *.exe                        # Executable files
?   ??? *.obj                        # Object files
?   ??? Release/                     # Release build artifacts
?   ??? x64/                         # x64 build artifacts
?
??? cmake/                            # CMake modules and helpers
?
??? config/                           # Configuration files
?   ??? dataset_config.json          # Dataset configuration
?
??? data/                             # Data storage (ORGANIZED)
?   ??? datasets/                     # Active datasets
?   ?   ??? dataset_main/            # Main training dataset
?   ?   ??? dataset_realistic/       # Realistic dataset variant
?   ?   ??? dataset_ir_realistic/    # IR realistic dataset variant
?   ??? datasets_archive/             # Archived/test datasets
?   ?   ??? dataset_previous/        # Previous version
?   ?   ??? dataset_ir_test/         # Test dataset (IR)
?   ?   ??? dataset_realistic_test/  # Test dataset (realistic)
?   ??? models/                       # Trained models
?       ??? trained_models/          # Active trained models
?       ??? backup/                  # Model backups
?           ??? models_backup_20260406_204304/
?           ??? models_backup_20260406_211843/
?
??? docs/                             # Documentation (ORGANIZED)
?   ??? guides/                       # User guides and tutorials
?   ?   ??? INCREMENTAL_TRAINING_GUIDE.md
?   ?   ??? INCREMENTAL_TRAINING_QUICKSTART.md
?   ?   ??? ONLINE_CURRICULUM_LEARNING_GUIDE.md
?   ?   ??? ONLINE_CURRICULUM_LEARNING_QUICKSTART.md
?   ?   ??? TRAINING_GUIDE.md
?   ?   ??? QUICK_REFERENCE.md
?   ?   ??? QUICK_START.txt
?   ?   ??? IMAGE_DISPLAY_GUIDE.txt
?   ?   ??? C++_TRAINING_PIPELINE_GUIDE.txt
?   ??? reports/                     # Technical reports and documentation
?       ??? IMPLEMENTATION_SUMMARY.md
?       ??? PROJECT_COMPLETION_REPORT.md
?       ??? INCREMENTAL_TRAINING_*.md (multiple files)
?       ??? ONLINE_CURRICULUM_*.md (multiple files)
?       ??? DATASET_*.md (multiple files)
?       ??? README_EXECUTION.md
?       ??? *.txt (status reports and completion summaries)
?
??? include/                          # Header files (.h)
?   ??? candidate/
?   ??? confidence/
?   ??? core/
?   ??? feature/
?   ??? performance/
?   ??? pipeline/
?   ??? reconstruction/
?   ??? tier/
?   ??? tracking/
?   ??? training/
?
??? logs/                             # Log files (ORGANIZED)
?   ??? training_run_20000.log       # Training execution log
?   ??? extend_dataset_output.log    # Dataset extension log
?
??? output/                           # Results and outputs
?   ??? results_latest/              # Latest execution results
?   ??? results_archive/             # Archived results
?   ??? out_legacy/                  # Legacy output files
?
??? scripts/                          # Utility and automation scripts
?
??? src/                              # Source code files (.cpp)
?   ??? autonomous_training_debug.cpp
?   ??? incremental_training_main.cpp
?   ??? online_curriculum_execution.cpp
?   ??? online_curriculum_learning_main.cpp
?   ??? training_execution.cpp
?   ??? training_main.cpp
?   ??? training_standalone.cpp      # (moved from root)
?   ??? main.cpp
?   ??? calibration/                 # Camera calibration
?   ??? candidate/                   # Candidate detection
?   ??? confidence/                  # Confidence calibration
?   ??? core/                        # Core functionality
?   ??? feature/                     # Feature extraction
?   ??? performance/                 # Performance optimization
?   ??? pipeline/                    # Pipeline management
?   ??? reconstruction/              # Triangulation and reconstruction
?   ??? tier/                        # Tier-based logic
?   ??? tracking/                    # Tracking management
?   ??? training/                    # Training modules
?       ??? IncrementalTrainer.cpp
?       ??? OnlineCurriculumLearner.cpp
?
??? tests/                            # Test files
?   ??? CMakeLists.txt
?   ??? test_worst_case_scenarios.cpp
?
??? apps/                             # Application files
?
??? .github/                          # GitHub configuration
?
??? .gitignore                        # Git ignore patterns
??? build_and_run.bat                 # Build and run script
??? CMakeLists.txt                    # CMake configuration (root)
??? CMakePresets.json                 # CMake presets
?
??? CLEANUP_ORGANIZATION_SUMMARY.md  # This cleanup summary

```

## Directory Purposes

### Core Development Directories

| Directory | Purpose | Contents |
|-----------|---------|----------|
| `src/` | Source code files | C++ implementation files (.cpp) |
| `include/` | Header files | C++ declarations (.h) |
| `tests/` | Test suites | Unit and integration tests |
| `cmake/` | CMake modules | Build configuration helpers |

### Build and Execution

| Directory | Purpose | Contents |
|-----------|---------|----------|
| `build/` | Build artifacts | Compiled executables, object files, Visual Studio projects |
| `config/` | Configuration | JSON configs, settings files |
| `scripts/` | Automation | Build scripts, utility scripts |

### Data Management

| Directory | Purpose | Contents |
|-----------|---------|----------|
| `data/datasets/` | Active datasets | Training and test data for model training |
| `data/datasets_archive/` | Archived datasets | Previous versions and test datasets |
| `data/models/trained_models/` | Active models | Trained ML models ready for use |
| `data/models/backup/` | Model backups | Backup copies of trained models |

### Documentation and Logs

| Directory | Purpose | Contents |
|-----------|---------|----------|
| `docs/guides/` | User guides | How-to guides, quickstart guides, tutorials |
| `docs/reports/` | Technical reports | Implementation reports, execution results |
| `logs/` | Execution logs | Training logs, execution records |
| `output/` | Results | Experiment results, output data |

### Configuration and Metadata

| Directory | Purpose | Contents |
|-----------|---------|----------|
| `.github/` | GitHub config | Workflows, templates, settings |
| `.vs/` | Visual Studio | IDE settings and configuration |

## File Organization Rules

### Source Files (.cpp)
- Main entry points in `src/` root
- Modular implementations in subdirectories
- Convention: `<module>_main.cpp` for entry points

### Header Files (.h)
- Mirror the `src/` structure
- Located in `include/`
- Convention: `<module>/<Class>.h`

### Documentation (.md/.txt)
- Guides in `docs/guides/`
- Reports in `docs/reports/`
- README files should be at root for visibility

### Data Files
- Training datasets: `data/datasets/`
- Test datasets: `data/datasets_archive/`
- Models: `data/models/trained_models/`
- Backups: `data/models/backup/`

### Build Outputs
- All build artifacts: `build/`
- Never commit build artifacts
- Configure .gitignore appropriately

### Logs and Output
- Logs: `logs/`
- Results: `output/results_latest/`
- Periodic archival recommended

## Path Examples

### Building
```bash
# Generate build (CMake output goes to build/)
cmake -B build -G "Visual Studio 17 2022"

# Build
cmake --build build --config Release

# Run executable
./build/Release/online_curriculum_execution.exe
```

### Finding Files
```
Training source:    src/training/IncrementalTrainer.cpp
Training header:    include/training/IncrementalTrainer.h
Config file:        config/dataset_config.json
Training dataset:   data/datasets/dataset_main/
Test dataset:       data/datasets_archive/dataset_ir_test/
Training log:       logs/training_run_20000.log
Results:            output/results_latest/
```

## Best Practices

1. **Do not** commit to `build/`, `logs/`, or `output/` directories
2. **Do** keep data files outside version control (use .gitignore)
3. **Do** document new modules with guides in `docs/guides/`
4. **Do** save execution results to `output/results_latest/`
5. **Do** maintain model backups in `data/models/backup/`
6. **Do** archive old datasets in `data/datasets_archive/`
7. **Do** keep logs for post-execution analysis

## Git Configuration

Add to `.gitignore`:
```
# Build artifacts
/build/*
!/build/.gitkeep

# Logs
/logs/
*.log

# Results
/output/results_latest/
*.tmp

# Data (optional - large files)
/data/datasets/
/data/models/trained_models/

# IDE
.vs/
```

---

**Last Updated:** April 7, 2026
**Version:** 1.0
