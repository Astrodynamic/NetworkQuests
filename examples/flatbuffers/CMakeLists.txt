cmake_minimum_required(VERSION 3.24.0)

project(FlatBuffersNetworkExamples VERSION 1.0.0 LANGUAGES CXX)

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

# Custom target to build all FlatBuffers examples
add_custom_target(flatbuffers_examples
    DEPENDS
        tcp_server
        tcp_client
        udp_server
        udp_client
    COMMENT "Building all FlatBuffers network examples"
)