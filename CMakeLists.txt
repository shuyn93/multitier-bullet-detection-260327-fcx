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

find_package(OpenCV REQUIRED)
find_package(Eigen3 REQUIRED)

add_executable(BulletHoleDetection 
    src/main.cpp
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

target_include_directories(BulletHoleDetection PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${EIGEN3_INCLUDE_DIRS}
)

target_link_libraries(BulletHoleDetection PRIVATE ${OpenCV_LIBS})

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

# TODO: Add tests and install targets if needed.
