cmake_minimum_required(VERSION 3.24.1)

project(NetworkQuests 
    VERSION 1.0.0 
    DESCRIPTION "Educational Network Programming Library and Applications in Modern C++"
    LANGUAGES CXX
)

# Set C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set build type if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler-specific options
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-g -O0 -fsanitize=address,undefined)
        add_link_options(-fsanitize=address,undefined)
    endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    add_compile_options(/W4)
endif()

# Include directories
include_directories(include)

# Find required packages
find_package(Threads REQUIRED)

# Optional dependencies
option(ENABLE_BOOST "Enable Boost.Asio for advanced networking" ON)
option(ENABLE_OPENSSL "Enable OpenSSL for secure communications" ON)
option(ENABLE_TESTING "Enable building tests" ON)
option(ENABLE_EXAMPLES "Enable building examples" ON)

# Boost
if(ENABLE_BOOST)
    find_package(Boost COMPONENTS system QUIET)
    if(Boost_FOUND)
        message(STATUS "Boost found, enabling advanced networking features")
        add_compile_definitions(NETWORKQUESTS_HAS_BOOST)
    else()
        message(STATUS "Boost not found, using standard library networking only")
    endif()
endif()

# OpenSSL
if(ENABLE_OPENSSL)
    find_package(OpenSSL QUIET)
    if(OpenSSL_FOUND)
        message(STATUS "OpenSSL found, enabling secure communications")
        add_compile_definitions(NETWORKQUESTS_HAS_OPENSSL)
    else()
        message(STATUS "OpenSSL not found, secure features disabled")
    endif()
endif()

# Core library
add_subdirectory(src)

# Testing
if(ENABLE_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

# Examples
if(ENABLE_EXAMPLES)
    add_subdirectory(examples)
endif()

# Documentation
add_custom_target(docs
    COMMAND echo "Documentation is available in the docs/ directory"
    COMMENT "Building documentation"
)

# Install configuration
include(GNUInstallDirs)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
