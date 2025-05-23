# Examples for NetworkQuests

# TCP Examples
add_executable(tcp_echo_server tcp_echo_server.cpp)
target_link_libraries(tcp_echo_server networkquests)

add_executable(tcp_echo_client tcp_echo_client.cpp)
target_link_libraries(tcp_echo_client networkquests)

# UDP Examples
add_executable(udp_echo_server udp_echo_server.cpp)
target_link_libraries(udp_echo_server networkquests)

add_executable(udp_echo_client udp_echo_client.cpp)
target_link_libraries(udp_echo_client networkquests)

# Install examples
install(TARGETS 
    tcp_echo_server 
    tcp_echo_client 
    udp_echo_server
    udp_echo_client
    DESTINATION ${CMAKE_INSTALL_BINDIR}/examples
)