cmake_minimum_required(VERSION 3.24.0)

# Project definition
project(NetworkQuests 
    VERSION 1.0.0
    DESCRIPTION "Comprehensive C++20 Network Protocol Educational Library"
    HOMEPAGE_URL "https://github.com/your-username/NetworkQuests"
    LANGUAGES CXX
)

# Set C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Include modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Build options
option(ENABLE_EXAMPLES "Build example applications" ON)
option(ENABLE_TESTING "Build tests" ON)
option(ENABLE_BOOST "Enable Boost.Asio support" ON)
option(ENABLE_OPENSSL "Enable OpenSSL support" ON)
option(ENABLE_WARNINGS "Enable compiler warnings" ON)
option(ENABLE_SANITIZERS "Enable sanitizers (Debug builds)" OFF)
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
option(INSTALL_NETWORKQUESTS "Install NetworkQuests" ON)

# Set default build type
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

# Find required dependencies
find_package(Threads REQUIRED)

# Find optional dependencies
if(ENABLE_BOOST)
    find_package(Boost COMPONENTS system)
    if(Boost_FOUND)
        message(STATUS "Found Boost ${Boost_VERSION}")
    else()
        message(WARNING "Boost not found, disabling Boost support")
        set(ENABLE_BOOST OFF)
    endif()
endif()

if(ENABLE_OPENSSL)
    find_package(OpenSSL)
    if(OpenSSL_FOUND)
        message(STATUS "Found OpenSSL ${OPENSSL_VERSION}")
    else()
        message(WARNING "OpenSSL not found, disabling SSL/TLS support")
        set(ENABLE_OPENSSL OFF)
    endif()
endif()

# Testing framework
if(ENABLE_TESTING)
    enable_testing()
    find_package(Catch2 QUIET)
    if(NOT Catch2_FOUND)
        message(STATUS "Catch2 not found, will fetch from GitHub")
        include(FetchContent)
        FetchContent_Declare(
            Catch2
            GIT_REPOSITORY https://github.com/catchorg/Catch2.git
            GIT_TAG v3.4.0
        )
        FetchContent_MakeAvailable(Catch2)
    endif()
endif()

# Configure global compile definitions
if(ENABLE_BOOST)
    add_compile_definitions(NETWORKQUESTS_ENABLE_BOOST)
endif()

if(ENABLE_OPENSSL)
    add_compile_definitions(NETWORKQUESTS_ENABLE_OPENSSL)
endif()

# Platform-specific settings
if(WIN32)
    add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX)
    if(MSVC)
        add_compile_options(/utf-8)
    endif()
endif()

# Compiler-specific options
if(ENABLE_WARNINGS)
    if(MSVC)
        add_compile_options(/W4 /WX)
    else()
        add_compile_options(-Wall -Wextra -Wpedantic -Werror)
    endif()
endif()

if(ENABLE_SANITIZERS AND NOT MSVC AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Add subdirectories
add_subdirectory(src)

if(ENABLE_EXAMPLES)
    add_subdirectory(examples)
endif()

if(ENABLE_TESTING)
    add_subdirectory(tests)
endif()

# Create package configuration files
if(INSTALL_NETWORKQUESTS)
    include(CMakePackageConfigHelpers)
    
    # Generate config files
    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/NetworkQuestsConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/NetworkQuestsConfig.cmake"
        INSTALL_DESTINATION lib/cmake/NetworkQuests
    )
    
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/NetworkQuestsConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )
    
    # Install config files
    install(
        FILES
            "${CMAKE_CURRENT_BINARY_DIR}/NetworkQuestsConfig.cmake"
            "${CMAKE_CURRENT_BINARY_DIR}/NetworkQuestsConfigVersion.cmake"
        DESTINATION lib/cmake/NetworkQuests
    )
    
    # Install headers
    install(
        DIRECTORY include/
        DESTINATION include
        FILES_MATCHING PATTERN "*.hpp"
    )
    
    # Install documentation
    install(
        DIRECTORY docs/
        DESTINATION share/doc/NetworkQuests
        FILES_MATCHING PATTERN "*.md"
    )
    
    # Install license and readme
    install(
        FILES LICENSE README.md
        DESTINATION share/doc/NetworkQuests
    )
    
    # Install scripts
    install(
        PROGRAMS scripts/install.sh
        DESTINATION share/NetworkQuests/scripts
    )
    
    # Install legacy examples
    install(
        DIRECTORY examples/legacy/
        DESTINATION share/NetworkQuests/legacy
        FILES_MATCHING 
            PATTERN "*.cpp" 
            PATTERN "*.cc" 
            PATTERN "*.fbs" 
            PATTERN "*.cmake" 
            PATTERN "CMakeLists.txt"
            PATTERN "*.md"
    )
    
    # Create export targets
    install(
        EXPORT NetworkQuestsTargets
        FILE NetworkQuestsTargets.cmake
        NAMESPACE NetworkQuests::
        DESTINATION lib/cmake/NetworkQuests
    )
    
    # Export targets from build tree
    export(
        EXPORT NetworkQuestsTargets
        FILE "${CMAKE_CURRENT_BINARY_DIR}/NetworkQuestsTargets.cmake"
        NAMESPACE NetworkQuests::
    )
endif()

# Print configuration summary
message(STATUS "")
message(STATUS "==================== NetworkQuests Configuration ====================")
message(STATUS "Version:           ${PROJECT_VERSION}")
message(STATUS "Build type:        ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard:      C++${CMAKE_CXX_STANDARD}")
message(STATUS "Compiler:          ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "System:            ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_VERSION}")
message(STATUS "")
message(STATUS "Features:")
message(STATUS "  Examples:        ${ENABLE_EXAMPLES}")
message(STATUS "  Testing:         ${ENABLE_TESTING}")
message(STATUS "  Boost support:   ${ENABLE_BOOST}")
message(STATUS "  OpenSSL support: ${ENABLE_OPENSSL}")
message(STATUS "  Warnings:        ${ENABLE_WARNINGS}")
message(STATUS "  Sanitizers:      ${ENABLE_SANITIZERS}")
message(STATUS "  Shared libs:     ${BUILD_SHARED_LIBS}")
message(STATUS "  Installation:    ${INSTALL_NETWORKQUESTS}")
message(STATUS "")
message(STATUS "Protocols implemented:")
message(STATUS "  ✓ TCP (Transmission Control Protocol)")
message(STATUS "  ✓ UDP (User Datagram Protocol)")
message(STATUS "  ✓ HTTP (HyperText Transfer Protocol)")
message(STATUS "  ✓ DNS (Domain Name System)")
message(STATUS "  ✓ FTP (File Transfer Protocol)")
message(STATUS "  ✓ WebSocket (RFC 6455)")
message(STATUS "  ✓ SMTP (Simple Mail Transfer Protocol)")
message(STATUS "  ✓ SNMP (Simple Network Management Protocol)")
message(STATUS "")
message(STATUS "Install prefix:    ${CMAKE_INSTALL_PREFIX}")
message(STATUS "====================================================================")
message(STATUS "")
