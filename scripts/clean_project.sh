#!/bin/bash

# NetworkQuests Project Cleanup Script
# Removes all old legacy files and unused directories

set -e

echo "=========================================="
echo "    NetworkQuests Project Cleanup"
echo "=========================================="
echo ""

# Check if we're in the right directory
if [ ! -f "CMakeLists.txt" ] || [ ! -d "src" ]; then
    echo "❌ Error: This script must be run from the NetworkQuests root directory"
    exit 1
fi

echo "🧹 Cleaning up NetworkQuests project..."
echo ""

# Remove old legacy directories from root
echo "🗑️  Removing old legacy directories from root:"
[ -d "flatbuffers" ] && echo "   - flatbuffers/ (moved to examples/flatbuffers/)" && rm -rf flatbuffers/
[ -d "tcp" ] && echo "   - tcp/ (moved to examples/flatbuffers/tcp/)" && rm -rf tcp/
[ -d "udp" ] && echo "   - udp/ (moved to examples/flatbuffers/udp/)" && rm -rf udp/

# Remove build directories
echo "🗑️  Removing build artifacts:"
[ -d "build" ] && echo "   - build/" && rm -rf build/
[ -d "Build" ] && echo "   - Build/" && rm -rf Build/
[ -d "BUILD" ] && echo "   - BUILD/" && rm -rf BUILD/
[ -d "_build" ] && echo "   - _build/" && rm -rf _build/

# Remove tests directory (unused)
echo "🗑️  Removing unused tests directory:"
[ -d "tests" ] && echo "   - tests/ (empty placeholder)" && rm -rf tests/

# Remove cleanup documentation files
echo "🗑️  Removing cleanup documentation files:"
[ -f "CLEANUP_INSTRUCTIONS.md" ] && echo "   - CLEANUP_INSTRUCTIONS.md" && rm -f CLEANUP_INSTRUCTIONS.md
[ -f "CLEANUP_INSTRUCTIONS_OLD.md" ] && echo "   - CLEANUP_INSTRUCTIONS_OLD.md" && rm -f CLEANUP_INSTRUCTIONS_OLD.md
[ -f "FINAL_CLEANUP_REQUIRED.md" ] && echo "   - FINAL_CLEANUP_REQUIRED.md" && rm -f FINAL_CLEANUP_REQUIRED.md
[ -f "PROJECT_COMPLETED.md" ] && echo "   - PROJECT_COMPLETED.md" && rm -f PROJECT_COMPLETED.md

# Remove CMake artifacts
echo "🗑️  Removing CMake artifacts:"
[ -f "CMakeCache.txt" ] && echo "   - CMakeCache.txt" && rm -f CMakeCache.txt
[ -f "cmake_install.cmake" ] && echo "   - cmake_install.cmake" && rm -f cmake_install.cmake
[ -f "compile_commands.json" ] && echo "   - compile_commands.json" && rm -f compile_commands.json
[ -d "CMakeFiles" ] && echo "   - CMakeFiles/" && rm -rf CMakeFiles/
[ -d "Testing" ] && echo "   - Testing/" && rm -rf Testing/

# Remove IDE files
echo "🗑️  Removing IDE configuration files:"
[ -d ".vscode" ] && echo "   - .vscode/" && rm -rf .vscode/
[ -d ".vs" ] && echo "   - .vs/" && rm -rf .vs/
[ -d ".idea" ] && echo "   - .idea/" && rm -rf .idea/
find . -name "*.vcxproj*" -type f -delete 2>/dev/null && echo "   - *.vcxproj files"
find . -name "*.sln" -type f -delete 2>/dev/null && echo "   - *.sln files"

# Remove temporary files
echo "🗑️  Removing temporary files:"
find . -name "*.tmp" -type f -delete 2>/dev/null && echo "   - *.tmp files"
find . -name "*.temp" -type f -delete 2>/dev/null && echo "   - *.temp files"
find . -name "*.log" -type f -delete 2>/dev/null && echo "   - *.log files"
find . -name "*~" -type f -delete 2>/dev/null && echo "   - *~ backup files"
find . -name ".DS_Store" -type f -delete 2>/dev/null && echo "   - .DS_Store files"
find . -name "Thumbs.db" -type f -delete 2>/dev/null && echo "   - Thumbs.db files"

# Remove old scripts that are no longer needed
echo "🗑️  Removing old cleanup scripts:"
[ -f "scripts/cleanup.sh" ] && echo "   - scripts/cleanup.sh" && rm -f scripts/cleanup.sh
[ -f "scripts/final_cleanup.sh" ] && echo "   - scripts/final_cleanup.sh" && rm -f scripts/final_cleanup.sh
[ -f "scripts/verify_build.sh" ] && echo "   - scripts/verify_build.sh" && rm -f scripts/verify_build.sh

echo ""
echo "✅ Project cleanup completed!"
echo ""
echo "📁 Final clean project structure:"
echo "   📁 include/networkquests/    # Public API headers"
echo "   📁 src/                      # Implementation code"
echo "   📁 examples/                 # Example applications"
echo "   📁 examples/flatbuffers/     # FlatBuffers examples (preserved)"
echo "   📁 docs/                     # Documentation"
echo "   📁 cmake/                    # CMake configuration"
echo "   📁 scripts/                  # Build scripts"
echo "   📄 README.md                 # Main documentation"
echo "   📄 PROGRESS.md               # Development progress"
echo "   📄 CMakeLists.txt            # Build configuration"
echo "   📄 LICENSE                   # MIT License"
echo ""
echo "🚀 NetworkQuests is now completely clean and ready to use!"
echo ""
echo "Next steps:"
echo "1. Build: mkdir build && cd build && cmake .. && make"
echo "2. Install: ./scripts/install.sh"
echo "3. Read docs: docs/GETTING_STARTED.md"