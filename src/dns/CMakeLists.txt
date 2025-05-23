# DNS Protocol Implementation (placeholder)
# TODO: Implement DNS resolver classes

# For now, create an empty library to satisfy CMake dependencies
add_library(networkquests_dns INTERFACE)
target_link_libraries(networkquests_dns INTERFACE networkquests_core)

# DNS Protocol Implementation
add_library(networkquests_dns
    dns_utils.cpp
    dns_message.cpp
    dns_records.cpp
    dns_client.cpp
    dns_server.cpp
)

target_link_libraries(networkquests_dns 
    networkquests_core
    networkquests_tcp
    networkquests_udp
)