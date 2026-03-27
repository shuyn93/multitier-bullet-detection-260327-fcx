# Bullet Hole Detection System - Build Instructions

## Quick Start

### Linux / macOS

```bash
# Create build directory
mkdir build && cd build

# Configure
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build . --parallel $(nproc)

# Run tests
ctest --output-on-failure

# Install
cmake --install . --prefix ~/local
```

### Windows (Visual Studio)

```bash
# Create build directory
mkdir build
cd build

# Configure
cmake .. -G "Visual Studio 17 2022" -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build . --config Release --parallel 4

# Run tests
ctest --output-on-failure --config Release

# Install
cmake --install . --prefix "C:\Program Files\BulletDetection"
```

## Build Options

### Feature Flags

```bash
# SIMD Optimization (enabled by default)
cmake .. -DWITH_SIMD=OFF

# Build shared libraries instead of static
cmake .. -DBUILD_SHARED_LIBS=ON

# Enable CUDA support (if available)
cmake .. -DWITH_CUDA=ON

# Disable tests
cmake .. -DBUILD_TESTS=OFF

# Enable Address Sanitizer (Linux/macOS)
cmake .. -DENABLE_ASAN=ON
```

### Build Types

```bash
# Release (optimized, small size)
cmake .. -DCMAKE_BUILD_TYPE=Release

# Debug (debug symbols, no optimization)
cmake .. -DCMAKE_BUILD_TYPE=Debug

# RelWithDebInfo (optimized with debug symbols)
cmake .. -DCMAKE_BUILD_TYPE=RelWithDebInfo

# MinSizeRel (smallest size, some optimization)
cmake .. -DCMAKE_BUILD_TYPE=MinSizeRel
```

## Dependencies

### Required

- **CMake**: 3.15 or later
- **C++ Compiler**: Supporting C++20
  - MSVC 17 (2022) or later
  - GCC 11 or later
  - Clang 12 or later
- **OpenCV**: 4.0 or later
- **Eigen3**: 3.3 or later

### Installation

#### Ubuntu/Debian

```bash
sudo apt-get update
sudo apt-get install -y \
    cmake \
    build-essential \
    libopencv-dev \
    libeigen3-dev
```

#### macOS

```bash
# Using Homebrew
brew install cmake opencv eigen
```

#### Windows

Using **vcpkg** (recommended):

```bash
# Clone vcpkg
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
./vcpkg integrate install

# Install dependencies
./vcpkg install opencv:x64-windows eigen3:x64-windows

# Build project
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=<vcpkg-path>/scripts/buildsystems/vcpkg.cmake
```

## Advanced Configuration

### Compiler Selection

```bash
# Use Clang instead of default compiler
cmake .. -DCMAKE_CXX_COMPILER=clang++

# Use specific MSVC version
cmake .. -G "Visual Studio 17 2022" -A x64
```

### Cross-Compilation

```bash
# For ARM (e.g., Raspberry Pi)
cmake .. \
    -DCMAKE_SYSTEM_NAME=Linux \
    -DCMAKE_SYSTEM_PROCESSOR=armv7l \
    -DCMAKE_C_COMPILER=arm-linux-gnueabihf-gcc \
    -DCMAKE_CXX_COMPILER=arm-linux-gnueabihf-g++
```

### CMake Presets (C++20 feature)

```bash
# List available presets
cmake --list-presets

# Use preset
cmake --preset=default
cmake --build --preset=default
```

## Testing

### Run all tests

```bash
cd build
ctest --output-on-failure
```

### Run specific test

```bash
ctest -R "FeatureExtraction" --output-on-failure
```

### Verbose output

```bash
ctest --output-on-failure -V
```

### Parallel testing

```bash
ctest --output-on-failure --parallel 4
```

## Installation

### Default installation

```bash
cmake --install build
```

### Custom installation path

```bash
cmake --install build --prefix /usr/local
```

### Generate package

```bash
cpack --config build/CPackConfig.cmake
```

## Troubleshooting

### OpenCV not found

```bash
# Manually specify OpenCV path
cmake .. -DOpenCV_DIR=/path/to/opencv/lib/cmake/opencv4
```

### Eigen3 not found

```bash
# Manually specify Eigen3 path
cmake .. -DEigen3_DIR=/path/to/eigen3/share/cmake/Eigen3
```

### CMake version too old

```bash
# Download newer CMake from cmake.org or upgrade
sudo apt-get install -y cmake  # Ubuntu
brew upgrade cmake             # macOS
```

### Compilation errors with C++20

Ensure your compiler supports C++20:

```bash
# Check GCC version
g++ --version  # Should be 11 or later

# Check Clang version
clang++ --version  # Should be 12 or later

# MSVC usually supports C++20 by default
```

## Build Output

### Library

Located in: `build/` directory

- **Linux**: `libbullet_detection.a` (static) or `libbullet_detection.so` (shared)
- **macOS**: `libbullet_detection.a` (static) or `libbullet_detection.dylib` (shared)
- **Windows**: `bullet_detection.lib` (static) or `bullet_detection.dll` (shared)

### Executable

- **Linux/macOS**: `build/BulletHoleDetection`
- **Windows**: `build/Release/BulletHoleDetection.exe`

## Performance Tips

1. **Use Release builds** for production:
   ```bash
   cmake .. -DCMAKE_BUILD_TYPE=Release
   ```

2. **Enable SIMD** optimizations:
   ```bash
   cmake .. -DWITH_SIMD=ON
   ```

3. **Use parallel build**:
   ```bash
   cmake --build . --parallel $(nproc)
   ```

4. **Link-time optimization** (LTO):
   ```bash
   cmake .. -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON
   ```

## CI/CD Integration

### GitHub Actions

Workflows are automatically triggered on:
- Push to main/develop branches
- Pull requests

Check `.github/workflows/build.yml` for details.

### Local CI (using CTest)

```bash
# Run tests with verbose output
ctest -V --output-on-failure

# Generate test report
ctest -D ExperimentalBuild
ctest -D ExperimentalTest
```

## Support & Debugging

### Enable verbose build

```bash
cmake --build . --verbose
```

### Run with AddressSanitizer

```bash
cmake .. -DENABLE_ASAN=ON
cmake --build .
./build/BulletHoleDetection
```

### Generate verbose CMake output

```bash
cmake .. --debug-output
```

## Documentation

Generate Doxygen documentation (if configured):

```bash
doxygen Doxyfile
open docs/html/index.html  # macOS
xdg-open docs/html/index.html  # Linux
```

