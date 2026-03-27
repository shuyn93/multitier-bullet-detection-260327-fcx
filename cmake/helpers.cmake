# CMake helper functions and macros

# ===== COMPILER DETECTION =====

function(print_compiler_info)
    message(STATUS "=== Compiler Information ===")
    message(STATUS "CXX Compiler: ${CMAKE_CXX_COMPILER}")
    message(STATUS "CXX Compiler Version: ${CMAKE_CXX_COMPILER_VERSION}")
    message(STATUS "CXX Compiler ID: ${CMAKE_CXX_COMPILER_ID}")
    message(STATUS "Platform: ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_VERSION}")
    message(STATUS "Processor: ${CMAKE_SYSTEM_PROCESSOR}")
endfunction()

# ===== FEATURE DETECTION =====

function(check_cxx_features)
    include(CheckCXXSourceCompiles)
    
    # Check for C++20 concepts
    check_cxx_source_compiles("
        template<typename T>
        concept Integral = __is_integral(T);
        
        int main() {
            return 0;
        }
    " HAS_CONCEPTS)
    
    if(HAS_CONCEPTS)
        message(STATUS "C++20 Concepts: Supported")
    else()
        message(WARNING "C++20 Concepts: Not supported")
    endif()
endfunction()

# ===== COMPILER FLAGS =====

function(add_standard_flags TARGET)
    if(MSVC)
        target_compile_options(${TARGET} PRIVATE
            /EHsc  # Exception handling
            /Zc:inline  # Inline functions
        )
    else()
        target_compile_options(${TARGET} PRIVATE
            -fPIC  # Position independent code
            -fno-common
        )
    endif()
endfunction()

# ===== OPTIMIZATION FLAGS =====

function(add_optimization_flags TARGET)
    if(MSVC)
        if(CMAKE_BUILD_TYPE STREQUAL "Release")
            target_compile_options(${TARGET} PRIVATE /GL)  # Whole program optimization
            target_link_options(${TARGET} PRIVATE /LTCG)
        endif()
    else()
        if(CMAKE_BUILD_TYPE STREQUAL "Release")
            target_compile_options(${TARGET} PRIVATE -flto)
        endif()
    endif()
endfunction()

# ===== SANITIZER FLAGS =====

function(add_sanitizer_flags TARGET SANITIZERS)
    if(NOT MSVC)
        foreach(SANITIZER ${SANITIZERS})
            target_compile_options(${TARGET} PRIVATE -fsanitize=${SANITIZER})
            target_link_options(${TARGET} PRIVATE -fsanitize=${SANITIZER})
        endforeach()
    endif()
endfunction()

# ===== DEPENDENCY HELPERS =====

function(require_package PACKAGE_NAME)
    find_package(${PACKAGE_NAME} REQUIRED)
    message(STATUS "${PACKAGE_NAME}: Found")
endfunction()

function(require_package_version PACKAGE_NAME MIN_VERSION)
    find_package(${PACKAGE_NAME} ${MIN_VERSION} REQUIRED)
    message(STATUS "${PACKAGE_NAME}: Found (version ${${PACKAGE_NAME}_VERSION})")
endfunction()

# ===== HEADER-ONLY LIBRARY SETUP =====

function(add_header_only_library TARGET INCLUDE_DIR)
    add_library(${TARGET} INTERFACE)
    target_include_directories(${TARGET} INTERFACE ${INCLUDE_DIR})
endfunction()

# ===== PRINT CONFIGURATION =====

function(print_build_config)
    message(STATUS "")
    message(STATUS "=== Build Configuration ===")
    message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
    message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
    message(STATUS "Shared Libraries: ${BUILD_SHARED_LIBS}")
    message(STATUS "Tests: ${BUILD_TESTS}")
    message(STATUS "SIMD: ${WITH_SIMD}")
    message(STATUS "CUDA: ${WITH_CUDA}")
    message(STATUS "")
endfunction()

# ===== TARGET INFO =====

function(print_target_info TARGET)
    get_target_property(SOURCES ${TARGET} SOURCES)
    get_target_property(INCLUDE_DIRS ${TARGET} INCLUDE_DIRECTORIES)
    get_target_property(LINK_LIBS ${TARGET} LINK_LIBRARIES)
    
    message(STATUS "Target: ${TARGET}")
    message(STATUS "  Sources: ${SOURCES}")
    message(STATUS "  Includes: ${INCLUDE_DIRS}")
    message(STATUS "  Links: ${LINK_LIBS}")
endfunction()

