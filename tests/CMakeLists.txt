# ===== TESTING SETUP =====

# Option 1: Use GoogleTest (if available)
# For now, we'll create a simple test structure

# Create tests directory structure
add_executable(test_feature_extraction
    tests/test_feature_extraction.cpp
)

target_link_libraries(test_feature_extraction
    PRIVATE
        bullet_detection
)

# Register with CTest
add_test(
    NAME FeatureExtraction
    COMMAND test_feature_extraction
)

# ===== ADDITIONAL TESTS =====
add_executable(test_triangulation
    tests/test_triangulation.cpp
)

target_link_libraries(test_triangulation
    PRIVATE
        bullet_detection
)

add_test(
    NAME Triangulation
    COMMAND test_triangulation
)

# ===== TEST COVERAGE =====
# Add more tests as needed
message(STATUS "Tests registered:")
message(STATUS "  - FeatureExtraction")
message(STATUS "  - Triangulation")

# To run all tests:
# ctest --output-on-failure

