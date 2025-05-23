#!/bin/bash

# NetworkQuests Cleanup Script
# This script removes old modules and unnecessary directories

set -e

echo "=========================================="
echo "     NetworkQuests Cleanup Script"
echo "=========================================="
echo "This script will remove old modules and build artifacts"
echo ""

# Check if we're in the right directory
if [ ! -f "CMakeLists.txt" ] || [ ! -d "src" ]; then
    echo "Error: This script must be run from the NetworkQuests root directory"
    exit 1
fi

# Function to safely remove directory
remove_directory() {
    local dir="$1"
    if [ -d "$dir" ]; then
        echo "Removing directory: $dir"
        rm -rf "$dir"
        echo "✓ Removed $dir"
    else
        echo "⚠ Directory $dir not found (already cleaned?)"
    fi
}

# Function to safely remove file
remove_file() {
    local file="$1"
    if [ -f "$file" ]; then
        echo "Removing file: $file"
        rm -f "$file"
        echo "✓ Removed $file"
    else
        echo "⚠ File $file not found (already cleaned?)"
    fi
}

echo "Starting cleanup..."
echo ""

# Remove old legacy directories from root
echo "1. Removing old legacy directories..."
remove_directory "flatbuffers"
remove_directory "tcp"
remove_directory "udp"
echo ""

# Remove build artifacts
echo "2. Removing build artifacts..."
remove_directory "build"
remove_directory "Build"
remove_directory "BUILD"
remove_directory "cmake-build-debug"
remove_directory "cmake-build-release"
remove_file "CMakeCache.txt"
remove_file "Makefile"
remove_file "cmake_install.cmake"
remove_file "compile_commands.json"
remove_directory "CMakeFiles"
remove_directory "Testing"
echo ""

# Remove IDE files
echo "3. Removing IDE files..."
remove_directory ".vscode"
remove_directory ".vs"
remove_directory ".idea"
remove_file "*.vcxproj"
remove_file "*.sln"
echo ""

# Remove temporary files
echo "4. Removing temporary files..."
find . -name "*.tmp" -type f -delete 2>/dev/null || true
find . -name "*.temp" -type f -delete 2>/dev/null || true
find . -name "*.log" -type f -delete 2>/dev/null || true
find . -name "*~" -type f -delete 2>/dev/null || true
find . -name ".DS_Store" -type f -delete 2>/dev/null || true
find . -name "Thumbs.db" -type f -delete 2>/dev/null || true
echo "✓ Cleaned temporary files"
echo ""

# Remove empty directories (except important ones)
echo "5. Removing empty directories..."
find . -type d -empty -not -path "./.git/*" -not -path "./src/*" -not -path "./include/*" -not -path "./examples/*" -not -path "./docs/*" -not -path "./tests/*" -not -path "./cmake/*" -not -path "./scripts/*" -delete 2>/dev/null || true
echo "✓ Cleaned empty directories"
echo ""

echo "=========================================="
echo "              Cleanup Complete!"
echo "=========================================="
echo ""
echo "Removed items:"
echo "  ✓ Old legacy directories (flatbuffers/, tcp/, udp/)"
echo "  ✓ Build artifacts and generated files"
echo "  ✓ IDE configuration files"
echo "  ✓ Temporary and cache files"
echo "  ✓ Empty directories"
echo ""
echo "Current clean project structure:"
echo "  📁 include/networkquests/    # Public headers"
echo "  📁 src/                      # Implementation"
echo "  📁 examples/                 # Examples (including legacy/)"
echo "  📁 docs/                     # Documentation"
echo "  📁 tests/                    # Test suites"
echo "  📁 cmake/                    # CMake modules"
echo "  📁 scripts/                  # Build scripts"
echo ""
echo "The project is now clean and ready for development!"
echo ""
echo "To build the project:"
echo "  mkdir build && cd build"
echo "  cmake .."
echo "  make -j\$(nproc)"
echo ""
echo "Or use the installation script:"
echo "  ./scripts/install.sh"