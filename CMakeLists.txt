# CMakeList.txt : CMake project for bullet_hole_detection_system, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("bullet_hole_detection_system")

# Add source to this project's executable.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Enable strict warnings and error handling
if(MSVC)
  add_compile_options(/W3)
else()
  add_compile_options(-Wall -Wextra)
  add_compile_options(-std=c++20)
endif()

find_package(OpenCV QUIET)
find_package(Eigen3 QUIET)

if(OpenCV_FOUND AND Eigen3_FOUND)
add_executable(BulletHoleDetection 
    src/main.cpp
    src/pipeline/Pipeline.cpp
    src/feature/FeatureExtractor.cpp
    src/feature/ImprovedFeatureExtractor.cpp
    src/tier/Tiers.cpp
    src/tier/ImprovedModels.cpp
    src/tier/ImprovedTierLogic.cpp
    src/candidate/CandidateDetector.cpp
    src/candidate/ImprovedCandidateDetector.cpp
    src/candidate/ImprovedBlobDetectorStep2.cpp
    src/candidate/RobustNoiseFilter.cpp
    src/calibration/CameraModel.cpp
    src/reconstruction/Triangulation.cpp
    src/reconstruction/ImprovedTriangulation.cpp
    src/tracking/TrackManager.cpp
    src/tracking/ImprovedTracking.cpp
    src/confidence/Calibration.cpp
    src/performance/OptimizedProcessing.cpp
)

target_include_directories(BulletHoleDetection PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${EIGEN3_INCLUDE_DIRS}
)

target_link_libraries(BulletHoleDetection PRIVATE ${OpenCV_LIBS})
endif()

# ===== AUTONOMOUS TRAINING & DEBUGGING PIPELINE =====
add_executable(autonomous_training
    src/autonomous_training_debug.cpp
)

target_include_directories(autonomous_training PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# ===== TRAINING PIPELINE EXECUTABLE (Minimal dependencies) =====
add_executable(training_pipeline
    src/training_execution.cpp
    src/core/ProductionReady.cpp
)

target_include_directories(training_pipeline PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# ===== INCREMENTAL TRAINING EXECUTABLE (Continuous Learning) =====
if(OpenCV_FOUND AND Eigen3_FOUND)
add_executable(incremental_training
    src/incremental_training_main.cpp
    src/training/IncrementalTrainer.cpp
    src/tier/Tiers.cpp
    src/tier/ImprovedModels.cpp
    src/feature/FeatureExtractor.cpp
    src/confidence/Calibration.cpp
    src/core/ProductionReady.cpp
)

target_include_directories(incremental_training PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${EIGEN3_INCLUDE_DIRS}
)

target_link_libraries(incremental_training PRIVATE ${OpenCV_LIBS})
endif()

# ===== ONLINE CURRICULUM EXECUTION (Direct simulator - no OpenCV needed) =====
add_executable(online_curriculum_execution
    src/online_curriculum_execution.cpp
)

target_include_directories(online_curriculum_execution PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# ===== ONLINE CURRICULUM LEARNING EXECUTABLE (Self-Improving System) =====
if(OpenCV_FOUND AND Eigen3_FOUND)
add_executable(online_curriculum_learning
    src/online_curriculum_learning_main.cpp
    src/training/OnlineCurriculumLearner.cpp
    src/training/IncrementalTrainer.cpp
    src/tier/Tiers.cpp
    src/tier/ImprovedModels.cpp
    src/feature/FeatureExtractor.cpp
    src/confidence/Calibration.cpp
    src/core/ProductionReady.cpp
)

target_include_directories(online_curriculum_learning PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${EIGEN3_INCLUDE_DIRS}
)

target_link_libraries(online_curriculum_learning PRIVATE ${OpenCV_LIBS})
endif()

# ===== WORST-CASE SCENARIO & STRESS TESTS =====
option(BUILD_WORST_CASE_TESTS "Build worst-case scenario tests" ON)

if(BUILD_WORST_CASE_TESTS)
  # Worst-Case Scenario Test
  add_executable(test_worst_case_scenarios
    src/testing/test_worst_case_scenarios.cpp
    src/core/ProductionReady.cpp
    src/pipeline/Pipeline.cpp
    src/feature/FeatureExtractor.cpp
    src/feature/ImprovedFeatureExtractor.cpp
    src/tier/Tiers.cpp
    src/tier/ImprovedModels.cpp
    src/tier/ImprovedTierLogic.cpp
    src/candidate/CandidateDetector.cpp
    src/calibration/CameraModel.cpp
    src/reconstruction/Triangulation.cpp
    src/reconstruction/ImprovedTriangulation.cpp
    src/tracking/TrackManager.cpp
    src/tracking/ImprovedTracking.cpp
    src/confidence/Calibration.cpp
    src/performance/OptimizedProcessing.cpp
  )
  
  target_include_directories(test_worst_case_scenarios PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${EIGEN3_INCLUDE_DIRS}
  )
  
  target_link_libraries(test_worst_case_scenarios PRIVATE ${OpenCV_LIBS})
  
  # Register with CTest
  enable_testing()
  add_test(NAME WorstCaseScenarios COMMAND test_worst_case_scenarios)
endif()

# ===== STEP 2 & 3 UNIT TEST =====
if(OpenCV_FOUND)
add_executable(test_step2_step3_unit
    tests/test_step2_step3_unit.cpp
    src/candidate/ImprovedCandidateDetector.cpp
    src/candidate/RobustNoiseFilter.cpp
)

target_include_directories(test_step2_step3_unit PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(test_step2_step3_unit PRIVATE ${OpenCV_LIBS})

enable_testing()
add_test(NAME STEP2STEP3Unit COMMAND test_step2_step3_unit)
endif()

# ===== STEP 2 & 3 INTEGRATION TEST =====
if(OpenCV_FOUND)
add_executable(test_integration_real_image
    tests/test_integration_real_image.cpp
    src/candidate/ImprovedCandidateDetector.cpp
    src/candidate/RobustNoiseFilter.cpp
)

target_include_directories(test_integration_real_image PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(test_integration_real_image PRIVATE ${OpenCV_LIBS})

add_test(NAME IntegrationRealImage COMMAND test_integration_real_image)
endif()

# TODO: Add tests and install targets if needed.
