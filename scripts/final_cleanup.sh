#!/bin/bash

# NetworkQuests Final Cleanup Script
# This script removes ALL old legacy directories and files

set -e

echo "=========================================="
echo "    NetworkQuests Final Cleanup"
echo "=========================================="
echo "This will remove old legacy directories from root"
echo ""

# Check if we're in the right directory
if [ ! -f "CMakeLists.txt" ] || [ ! -d "src" ]; then
    echo "Error: This script must be run from the NetworkQuests root directory"
    exit 1
fi

echo "Removing old legacy directories..."

# Remove old directories from root
echo "🗑️  Removing: flatbuffers/ (moved to examples/flatbuffers/)"
rm -rf flatbuffers/

echo "🗑️  Removing: tcp/ (moved to examples/flatbuffers/tcp/)"
rm -rf tcp/

echo "🗑️  Removing: udp/ (moved to examples/flatbuffers/udp/)"
rm -rf udp/

echo "🗑️  Removing: build/ (build artifacts)"
rm -rf build/

echo "🗑️  Removing: Build/ (build artifacts)"
rm -rf Build/

echo "🗑️  Removing: BUILD/ (build artifacts)"
rm -rf BUILD/

# Remove old documentation files
echo "🗑️  Removing: CLEANUP_INSTRUCTIONS.md (no longer needed)"
rm -f CLEANUP_INSTRUCTIONS.md

echo "🗑️  Removing: CLEANUP_INSTRUCTIONS_OLD.md (no longer needed)"
rm -f CLEANUP_INSTRUCTIONS_OLD.md

# Remove CMake artifacts
echo "🗑️  Cleaning CMake artifacts..."
rm -f CMakeCache.txt
rm -f cmake_install.cmake
rm -f compile_commands.json
rm -rf CMakeFiles/
rm -rf Testing/

# Remove IDE files
echo "🗑️  Cleaning IDE files..."
rm -rf .vscode/
rm -rf .vs/
rm -rf .idea/
rm -f *.vcxproj*
rm -f *.sln

# Remove temporary files
echo "🗑️  Cleaning temporary files..."
find . -name "*.tmp" -type f -delete 2>/dev/null || true
find . -name "*.temp" -type f -delete 2>/dev/null || true
find . -name "*.log" -type f -delete 2>/dev/null || true
find . -name "*~" -type f -delete 2>/dev/null || true
find . -name ".DS_Store" -type f -delete 2>/dev/null || true
find . -name "Thumbs.db" -type f -delete 2>/dev/null || true

echo ""
echo "✅ Cleanup completed!"
echo ""
echo "Final clean project structure:"
echo "  📁 include/networkquests/    # Public headers"
echo "  📁 src/                      # Implementation"
echo "  📁 examples/                 # Examples"
echo "  📁 examples/flatbuffers/     # FlatBuffers examples"
echo "  📁 docs/                     # Documentation"
echo "  📁 tests/                    # Test framework"
echo "  📁 cmake/                    # CMake modules"
echo "  📁 scripts/                  # Build scripts"
echo ""
echo "Project is now completely clean and ready!"
echo ""
echo "Next steps:"
echo "1. Run verification: ./scripts/verify_build.sh"
echo "2. Build the project: mkdir build && cd build && cmake .. && make"
echo "3. Or use installer: ./scripts/install.sh"