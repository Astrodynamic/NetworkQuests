#!/bin/bash

# NetworkQuests Installation Script
# This script builds and installs NetworkQuests with all protocols

set -e

# Default values
BUILD_TYPE="Release"
INSTALL_PREFIX="/usr/local"
BUILD_EXAMPLES="ON"
BUILD_TESTS="ON"
ENABLE_BOOST="ON"
ENABLE_OPENSSL="ON"
PARALLEL_JOBS=$(nproc)

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --build-type)
            BUILD_TYPE="$2"
            shift 2
            ;;
        --prefix)
            INSTALL_PREFIX="$2"
            shift 2
            ;;
        --no-examples)
            BUILD_EXAMPLES="OFF"
            shift
            ;;
        --no-tests)
            BUILD_TESTS="OFF"
            shift
            ;;
        --no-boost)
            ENABLE_BOOST="OFF"
            shift
            ;;
        --no-openssl)
            ENABLE_OPENSSL="OFF"
            shift
            ;;
        --jobs)
            PARALLEL_JOBS="$2"
            shift 2
            ;;
        --help)
            echo "NetworkQuests Installation Script"
            echo ""
            echo "Usage: $0 [options]"
            echo ""
            echo "Options:"
            echo "  --build-type TYPE     Build type (Debug/Release/RelWithDebInfo) [default: Release]"
            echo "  --prefix PATH         Installation prefix [default: /usr/local]"
            echo "  --no-examples         Don't build examples"
            echo "  --no-tests            Don't build tests"
            echo "  --no-boost            Disable Boost support"
            echo "  --no-openssl          Disable OpenSSL support"
            echo "  --jobs N              Number of parallel jobs [default: $(nproc)]"
            echo "  --help                Show this help message"
            echo ""
            echo "Examples:"
            echo "  $0                                    # Install with defaults"
            echo "  $0 --prefix ~/networkquests          # Install to home directory"
            echo "  $0 --build-type Debug --no-examples  # Debug build without examples"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

echo "=========================================="
echo "     NetworkQuests Installation"
echo "=========================================="
echo "Build type:        $BUILD_TYPE"
echo "Install prefix:    $INSTALL_PREFIX"
echo "Build examples:    $BUILD_EXAMPLES"
echo "Build tests:       $BUILD_TESTS"
echo "Enable Boost:      $ENABLE_BOOST"
echo "Enable OpenSSL:    $ENABLE_OPENSSL"
echo "Parallel jobs:     $PARALLEL_JOBS"
echo "=========================================="

# Check dependencies
echo "Checking dependencies..."

# Check CMake
if ! command -v cmake &> /dev/null; then
    echo "Error: CMake is required but not installed"
    echo "Please install CMake 3.24+ and try again"
    exit 1
fi

CMAKE_VERSION=$(cmake --version | head -n1 | cut -d' ' -f3)
CMAKE_MAJOR=$(echo $CMAKE_VERSION | cut -d'.' -f1)
CMAKE_MINOR=$(echo $CMAKE_VERSION | cut -d'.' -f2)

if [ "$CMAKE_MAJOR" -lt 3 ] || [ "$CMAKE_MAJOR" -eq 3 -a "$CMAKE_MINOR" -lt 24 ]; then
    echo "Error: CMake 3.24+ is required, found $CMAKE_VERSION"
    exit 1
fi

echo "Found CMake $CMAKE_VERSION ✓"

# Check compiler
if ! command -v g++ &> /dev/null && ! command -v clang++ &> /dev/null; then
    echo "Error: No C++ compiler found"
    echo "Please install GCC 10+ or Clang 11+ and try again"
    exit 1
fi

if command -v g++ &> /dev/null; then
    GCC_VERSION=$(g++ --version | head -n1 | grep -oE '[0-9]+\.[0-9]+' | head -n1)
    echo "Found GCC $GCC_VERSION ✓"
elif command -v clang++ &> /dev/null; then
    CLANG_VERSION=$(clang++ --version | head -n1 | grep -oE '[0-9]+\.[0-9]+' | head -n1)
    echo "Found Clang $CLANG_VERSION ✓"
fi

# Create build directory
echo "Creating build directory..."
BUILD_DIR="build"
if [ -d "$BUILD_DIR" ]; then
    echo "Removing existing build directory..."
    rm -rf "$BUILD_DIR"
fi
mkdir -p "$BUILD_DIR"

# Configure project
echo "Configuring project..."
cd "$BUILD_DIR"

cmake .. \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" \
    -DENABLE_EXAMPLES="$BUILD_EXAMPLES" \
    -DENABLE_TESTING="$BUILD_TESTS" \
    -DENABLE_BOOST="$ENABLE_BOOST" \
    -DENABLE_OPENSSL="$ENABLE_OPENSSL" \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# Build project
echo "Building project..."
cmake --build . --config "$BUILD_TYPE" --parallel "$PARALLEL_JOBS"

# Run tests if enabled
if [ "$BUILD_TESTS" = "ON" ]; then
    echo "Running tests..."
    ctest --output-on-failure --parallel "$PARALLEL_JOBS"
fi

# Install project
echo "Installing project..."
if [ "$INSTALL_PREFIX" != "/usr/local" ] || [ "$EUID" -eq 0 ]; then
    # Install directly if custom prefix or running as root
    cmake --install . --config "$BUILD_TYPE"
else
    # Use sudo for system installation
    echo "Installing to system directory requires root privileges..."
    sudo cmake --install . --config "$BUILD_TYPE"
fi

cd ..

echo ""
echo "=========================================="
echo "     Installation Complete!"
echo "=========================================="
echo "NetworkQuests has been successfully installed to: $INSTALL_PREFIX"
echo ""
echo "To use NetworkQuests in your CMake project:"
echo "  find_package(NetworkQuests REQUIRED)"
echo "  target_link_libraries(your_target PRIVATE networkquests)"
echo ""

if [ "$BUILD_EXAMPLES" = "ON" ]; then
    echo "Examples have been installed to: $INSTALL_PREFIX/bin/examples/"
    echo ""
    echo "Try running an example:"
    if [ "$INSTALL_PREFIX" = "/usr/local" ]; then
        echo "  /usr/local/bin/examples/tcp_echo_server"
    else
        echo "  $INSTALL_PREFIX/bin/examples/tcp_echo_server"
    fi
fi

echo ""
echo "For more information, see the documentation at:"
echo "  https://github.com/your-username/NetworkQuests"
echo ""
echo "Happy network programming! 🌐"