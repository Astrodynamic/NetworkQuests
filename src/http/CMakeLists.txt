# HTTP Protocol Implementation

# Create HTTP library
add_library(networkquests_http STATIC
    http_message.cpp
    http_utils.cpp
    http_client.cpp
    http_server.cpp
)

# Set target properties
target_include_directories(networkquests_http
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
)

# Link dependencies
target_link_libraries(networkquests_http
    PUBLIC
        networkquests_core
        networkquests_tcp
)

# Install targets
install(TARGETS networkquests_http
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)