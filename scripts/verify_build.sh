#!/bin/bash

# NetworkQuests Build Verification Script
# This script verifies that the project builds correctly after cleanup

set -e

echo "=========================================="
echo "   NetworkQuests Build Verification"
echo "=========================================="
echo ""

# Check if we're in the right directory
if [ ! -f "CMakeLists.txt" ] || [ ! -d "src" ]; then
    echo "Error: This script must be run from the NetworkQuests root directory"
    exit 1
fi

echo "1. Checking project structure..."

# Check required directories
required_dirs=("src" "include" "examples" "docs" "cmake" "scripts")
for dir in "${required_dirs[@]}"; do
    if [ ! -d "$dir" ]; then
        echo "❌ Missing required directory: $dir"
        exit 1
    else
        echo "✅ Found: $dir/"
    fi
done

# Check that old legacy directories are gone
old_dirs=("flatbuffers" "tcp" "udp")
for dir in "${old_dirs[@]}"; do
    if [ -d "$dir" ]; then
        echo "❌ Old directory still exists: $dir"
        echo "   Please run: rm -rf $dir"
        exit 1
    else
        echo "✅ Cleaned: $dir/ (removed)"
    fi
done

# Check that FlatBuffers examples are in the right place
if [ ! -d "examples/flatbuffers" ]; then
    echo "❌ FlatBuffers examples not found in examples/flatbuffers"
    exit 1
else
    echo "✅ Found: examples/flatbuffers/"
fi

echo ""
echo "2. Verifying CMake configuration..."

# Create clean build directory
BUILD_DIR="build_verify"
if [ -d "$BUILD_DIR" ]; then
    echo "Removing existing verification build directory..."
    rm -rf "$BUILD_DIR"
fi

mkdir "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure the project
echo "Configuring project..."
cmake .. -DCMAKE_BUILD_TYPE=Release -DENABLE_EXAMPLES=ON -DENABLE_TESTING=ON || {
    echo "❌ CMake configuration failed"
    cd ..
    rm -rf "$BUILD_DIR"
    exit 1
}

echo "✅ CMake configuration successful"

echo ""
echo "3. Building the project..."

# Build the project
cmake --build . --parallel $(nproc) || {
    echo "❌ Build failed"
    cd ..
    rm -rf "$BUILD_DIR"
    exit 1
}

echo "✅ Build successful"

echo ""
echo "4. Verifying example executables..."

# Check that examples were built
example_binaries=(
    "examples/tcp_echo_server"
    "examples/tcp_echo_client"
    "examples/udp_echo_server"
    "examples/udp_echo_client"
    "examples/http_server"
    "examples/http_client"
    "examples/dns_server"
    "examples/dns_client"
    "examples/ftp_server"
    "examples/ftp_client"
    "examples/websocket_server"
    "examples/websocket_client"
    "examples/smtp_server"
    "examples/smtp_client"
    "examples/snmp_agent"
    "examples/snmp_client"
)

for binary in "${example_binaries[@]}"; do
    if [ ! -f "$binary" ]; then
        echo "❌ Missing example binary: $binary"
        cd ..
        rm -rf "$BUILD_DIR"
        exit 1
    else
        echo "✅ Built: $binary"
    fi
done

echo ""
echo "5. Running basic tests..."

# Run tests
ctest --output-on-failure || {
    echo "❌ Tests failed"
    cd ..
    rm -rf "$BUILD_DIR"
    exit 1
}

echo "✅ Tests passed"

# Return to main directory
cd ..

echo ""
echo "6. Testing FlatBuffers examples (optional)..."

if command -v flatc &> /dev/null; then
    echo "FlatBuffers compiler found, testing FlatBuffers examples..."
    
    cd examples/flatbuffers
    if [ -d "build" ]; then
        rm -rf build
    fi
    
    mkdir build && cd build
    
    if cmake .. && make; then
        echo "✅ FlatBuffers examples built successfully"
    else
        echo "⚠️  FlatBuffers examples build failed (this is optional)"
    fi
    
    cd ../../..
else
    echo "⚠️  FlatBuffers compiler not found, skipping FlatBuffers examples"
    echo "   Install FlatBuffers to test these examples"
fi

# Clean up verification build
rm -rf "$BUILD_DIR"

echo ""
echo "=========================================="
echo "     Verification Complete! ✅"
echo "=========================================="
echo ""
echo "Project structure verified:"
echo "  ✅ Clean directory structure"
echo "  ✅ All required components present"
echo "  ✅ Old legacy directories removed"
echo "  ✅ CMake configuration works"
echo "  ✅ Project builds successfully"
echo "  ✅ All examples compile"
echo "  ✅ Tests framework works"
echo ""
echo "NetworkQuests is ready for use!"
echo ""
echo "Quick start:"
echo "  mkdir build && cd build"
echo "  cmake .. -DCMAKE_BUILD_TYPE=Release"
echo "  make -j\$(nproc)"
echo "  ./examples/tcp_echo_server"
echo ""
echo "Or use the installation script:"
echo "  ./scripts/install.sh"