# Recommended .gitignore Updates

This document provides recommended updates to the .gitignore file to properly handle the new directory structure.

## Current .gitignore Status
Check the existing .gitignore file at the repository root for current patterns.

## Recommended Additions

Based on the new directory structure, add these patterns to `.gitignore`:

```gitignore
# Build artifacts - BUILD DIRECTORY
/build/*
!/build/.gitkeep
*.o
*.obj
*.exe
*.dll
*.lib
*.a
*.so

# Build output
CMakeFiles/
cmake_install.cmake
CMakeCache.txt
CTestTestfile.cmake
*.vcxproj.user
*.vcxproj.filters

# Logs - LOGS DIRECTORY
/logs/
*.log

# Results and output - OUTPUT DIRECTORY
/output/results_latest/
/output/results_archive/
*.tmp
*.tmp.*

# Data files (OPTIONAL - only if datasets/models are large)
# Uncomment these if you want to exclude large data files
# /data/datasets/
# /data/models/trained_models/
# /data/models/backup/

# IDE specific files
.vs/
.vscode/
*.user
*.suo
*.sln.docstates

# OS specific
.DS_Store
Thumbs.db
*.swp
*.swo
*~

# Python cache
__pycache__/
*.py[cod]
*$py.class

# Temporary files
*.bak
*.tmp
tmp/
temp/
```

## File Structure to Preserve

Some directories should NOT be ignored. Ensure these remain tracked:

```gitignore
# Keep these directories tracked (remove from ignore if present)
!/cmake/
!/config/
!/include/
!/src/
!/tests/
!/scripts/
!/docs/
!/apps/
```

## Data Directory Handling

### Option 1: Include Data (Smaller Projects)
If datasets and models are reasonably sized, commit them:
- Remove `/data/datasets/` from ignore
- Remove `/data/models/` from ignore
- Use Git LFS for large files (if any > 100MB)

### Option 2: Exclude Data (Large Projects - RECOMMENDED)
For large datasets and models:

```gitignore
# Large data files
/data/datasets/
/data/models/trained_models/
/data/models/backup/

# Create .gitkeep files to preserve directory structure
# Then add: (optional pattern)
!/data/datasets/.gitkeep
!/data/models/.gitkeep
```

## Preservation of Directory Structure

To ensure empty directories are preserved in Git:

```bash
# Create .gitkeep files
touch data/datasets/.gitkeep
touch data/models/.gitkeep
touch logs/.gitkeep
touch output/.gitkeep

# Then add to git
git add */.gitkeep
```

## Implementation Steps

1. **Backup current .gitignore:**
   ```bash
   cp .gitignore .gitignore.backup
   ```

2. **Update .gitignore** with recommended patterns above

3. **Remove already-tracked artifacts** (if any):
   ```bash
   git rm --cached build/ -r  # If build/ was tracked
   git rm --cached logs/ -r   # If logs/ was tracked
   git rm --cached *.exe -r   # If .exe files were tracked
   ```

4. **Add preserve markers:**
   ```bash
   touch logs/.gitkeep output/.gitkeep
   git add logs/.gitkeep output/.gitkeep
   ```

5. **Commit changes:**
   ```bash
   git add .gitignore
   git commit -m "Update .gitignore for reorganized directory structure"
   ```

## Verification

After implementing .gitignore changes, verify:

```bash
# Show what would be staged
git add -n .

# Should NOT include:
# - /build/
# - /logs/
# - /output/results_latest/
# - Large data files (if Option 2 chosen)

# Should include:
# - /src/
# - /include/
# - /config/
# - /docs/
# - /tests/
```

## Data Management Strategy

### For Large Datasets (> 1GB total)

Consider one of these approaches:

1. **Git LFS (Large File Storage)**
   ```bash
   git lfs track "*.bin"
   git lfs track "*.h5"
   git add .gitattributes
   ```

2. **Separate Data Repository**
   - Create a separate repository for datasets
   - Use submodules or documentation for access

3. **External Storage**
   - Use cloud storage (S3, Azure Blob)
   - Document download/setup in README

4. **Exclude from Git** (Recommended)
   - Add to .gitignore (Option 2)
   - Provide setup scripts for local data
   - Document in README/docs/guides/

## Documentation Links

- Create `.gitignore` in each data directory with instructions:

**data/.gitignore:**
```
# This directory contains datasets and models
# Either:
# 1. Commit if reasonably sized
# 2. Add to parent .gitignore if large
# 3. Use external storage and update documentation

*/
!.gitkeep
!.gitignore
```

---

**Date:** April 7, 2026
**Related:** CLEANUP_ORGANIZATION_SUMMARY.md, DIRECTORY_STRUCTURE_GUIDE.md
