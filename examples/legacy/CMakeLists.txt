cmake_minimum_required(VERSION 3.24.2)

project(L4NetworkQuests VERSION 0.0.1 LANGUAGES CXX)

# Set C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find required dependencies
find_package(Flatbuffers REQUIRED)

# Add subdirectories
add_subdirectory(flatbuffers)
add_subdirectory(tcp)
add_subdirectory(udp)

# Custom target to build all legacy examples
add_custom_target(legacy_all
    DEPENDS
        tcp_server
        tcp_client
        udp_server
        udp_client
    COMMENT "Building all legacy L4NetworkQuests examples"
)