# UDP Protocol Implementation

# Create UDP library
add_library(networkquests_udp STATIC
    udp_socket.cpp
    udp_client.cpp
    udp_server.cpp
    udp_utils.cpp
)

# Set target properties
target_include_directories(networkquests_udp
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
)

# Link dependencies
target_link_libraries(networkquests_udp
    PUBLIC
        networkquests_core
)

# Install targets
install(TARGETS networkquests_udp
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)