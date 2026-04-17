@echo off
REM ===== ORGANIZE DOCUMENTATION FILES =====
REM Move all report files to /docs, keep only ARCHITECTURAL_SYSTEM_REPORT_VI.md at root

cd "C:\Users\Admin\source\repos\bullet_hole_detection_system"

echo [1] Moving report files to /docs...

REM Files to move (except ARCHITECTURAL_SYSTEM_REPORT_VI.md and README.md)
move /Y "00_START_TESTING_NOW.md" "docs\" 2>nul
move /Y "CLEANUP_DATASET_COMPLETION_REPORT.md" "docs\" 2>nul
move /Y "CLEANUP_ORGANIZATION_SUMMARY.md" "docs\" 2>nul
move /Y "COMPREHENSIVE_DOCUMENTATION_INDEX_AND_SUMMARY.md" "docs\" 2>nul
move /Y "COMPREHENSIVE_SYSTEM_ARCHITECTURE_REPORT.md" "docs\" 2>nul
move /Y "CSV_DATASET_ANALYSIS_REPORT.md" "docs\" 2>nul
move /Y "DATASETS_INVENTORY_REPORT.md" "docs\" 2>nul
move /Y "DATASETS_QUICK_REFERENCE.md" "docs\" 2>nul
move /Y "DATASET_DELETION_PLAN.md" "docs\" 2>nul
move /Y "DIRECTORY_STRUCTURE_GUIDE.md" "docs\" 2>nul
move /Y "FILE_INDEX_AND_NAVIGATION.md" "docs\" 2>nul
move /Y "GITIGNORE_RECOMMENDATIONS.md" "docs\" 2>nul
move /Y "INTEGRATION_CHECKLIST.md" "docs\" 2>nul
move /Y "MASTER_TEST_DEPLOYMENT_EXECUTION.md" "docs\" 2>nul
move /Y "ORGANIZATION_NEXT_STEPS.md" "docs\" 2>nul
move /Y "PROJECT_ORGANIZATION_INDEX.md" "docs\" 2>nul
move /Y "QUICK_REFERENCE_GUIDE_VI.md" "docs\" 2>nul
move /Y "READY_TO_TEST_START_HERE.md" "docs\" 2>nul
move /Y "STEP1_ANALYSIS_CURRENT_ISSUES.md" "docs\" 2>nul
move /Y "STEP1_AUDIT_BLOB_DETECTION.md" "docs\" 2>nul
move /Y "STEP1_COMPLETION_SUMMARY.md" "docs\" 2>nul
move /Y "STEP1_VISUAL_SUMMARY.md" "docs\" 2>nul
move /Y "STEP2_3_INTEGRATION_AND_VALIDATION_SUMMARY.md" "docs\" 2>nul
move /Y "STEP2_COMPLETE_SUMMARY.md" "docs\" 2>nul
move /Y "STEP2_COMPLETION_SUMMARY.md" "docs\" 2>nul
move /Y "STEP2_IMPLEMENTATION_COMPLETE.md" "docs\" 2>nul
move /Y "STEP2_NEW_REALISTIC_MODEL.md" "docs\" 2>nul
move /Y "STEP2_QUICK_START_GUIDE.md" "docs\" 2>nul
move /Y "STEP3_COMPLETION_SUMMARY.md" "docs\" 2>nul
move /Y "STEP3_IMPLEMENTATION_COMPLETE.md" "docs\" 2>nul
move /Y "STEP_COMPARISON_SUMMARY.md" "docs\" 2>nul
move /Y "TECHNICAL_GUIDE_VI.md" "docs\" 2>nul
move /Y "TESTING_AND_DEPLOYMENT_COMPLETE_INDEX.md" "docs\" 2>nul
move /Y "TESTING_AND_EXECUTION_GUIDE.md" "docs\" 2>nul
move /Y "TEST_RESULTS_AND_TROUBLESHOOTING.md" "docs\" 2>nul
move /Y "VALIDATION_AND_METRICS_GUIDE.md" "docs\" 2>nul
move /Y "VISUAL_COMPARISON_OLD_vs_NEW.md" "docs\" 2>nul
move /Y "VISUAL_REFERENCE_AND_QUICK_LOOKUP.md" "docs\" 2>nul

echo [2] Moving .txt files to /docs...
move /Y "*.txt" "docs\" 2>nul

echo [3] Verifying files...
echo Root files (should be minimal):
dir /B *.md
dir /B *.txt 2>nul

echo.
echo [4] Docs folder contents:
dir docs /B | find /C /V "" && echo files in docs/

echo.
echo [DONE] Organization complete!
echo Remaining at root:
echo   - ARCHITECTURAL_SYSTEM_REPORT_VI.md (kept)
echo   - README.md (kept)
echo   - CMakeLists.txt (code file)
echo   
echo All reports moved to: /docs/
pause
