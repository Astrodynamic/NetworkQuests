# TCP Protocol Implementation
set(TCP_SOURCES
    tcp_connection.cpp
    tcp_client.cpp
    tcp_server.cpp
    tcp_utils.cpp
)

add_library(networkquests_tcp ${TCP_SOURCES})

target_link_libraries(networkquests_tcp 
    PUBLIC 
        networkquests_core
)

target_include_directories(networkquests_tcp PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)