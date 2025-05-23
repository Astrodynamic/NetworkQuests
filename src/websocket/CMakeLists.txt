# WebSocket Protocol Implementation

# Create WebSocket library
add_library(networkquests_websocket
    websocket_utils.cpp
    websocket_frame.cpp
    websocket_client.cpp
    websocket_connection.cpp
    websocket_server.cpp
)

# Set target properties
set_target_properties(networkquests_websocket PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# Include directories
target_include_directories(networkquests_websocket
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link dependencies
target_link_libraries(networkquests_websocket
    PUBLIC
        networkquests_common
        networkquests_tcp
        networkquests_http
    PRIVATE
        ${CMAKE_THREAD_LIBS_INIT}
)

# Find required threads package
find_package(Threads REQUIRED)

# Platform-specific settings
if(WIN32)
    target_link_libraries(networkquests_websocket PRIVATE ws2_32)
    target_compile_definitions(networkquests_websocket PRIVATE 
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
endif()

# Compiler-specific warnings
if(MSVC)
    target_compile_options(networkquests_websocket PRIVATE 
        /W4 
        /permissive-
        /wd4267  # Disable size_t conversion warnings
        /wd4996  # Disable deprecated function warnings
    )
else()
    target_compile_options(networkquests_websocket PRIVATE 
        -Wall 
        -Wextra 
        -Wpedantic
        -Wno-unused-parameter
    )
endif()

# Install rules
install(TARGETS networkquests_websocket
    EXPORT NetworkQuestsTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)