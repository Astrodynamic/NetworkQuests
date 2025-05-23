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

# HTTP Examples
add_executable(http_server http_server.cpp)
target_link_libraries(http_server networkquests)

add_executable(http_client http_client.cpp)
target_link_libraries(http_client networkquests)

# DNS Examples
add_executable(dns_server dns_server.cpp)
target_link_libraries(dns_server networkquests)

add_executable(dns_client dns_client.cpp)
target_link_libraries(dns_client networkquests)

# FTP Examples
add_executable(ftp_server ftp_server.cpp)
target_link_libraries(ftp_server networkquests)

add_executable(ftp_client ftp_client.cpp)
target_link_libraries(ftp_client networkquests)

# WebSocket Examples
add_executable(websocket_server websocket_server.cpp)
target_link_libraries(websocket_server networkquests)

add_executable(websocket_client websocket_client.cpp)
target_link_libraries(websocket_client networkquests)

# SMTP Examples
add_executable(smtp_server smtp_server.cpp)
target_link_libraries(smtp_server networkquests)

add_executable(smtp_client smtp_client.cpp)
target_link_libraries(smtp_client networkquests)

# Install examples
install(TARGETS 
    tcp_echo_server 
    tcp_echo_client 
    udp_echo_server
    udp_echo_client
    http_server
    http_client
    dns_server
    dns_client
    ftp_server
    ftp_client
    websocket_server
    websocket_client
    smtp_server
    smtp_client
    DESTINATION ${CMAKE_INSTALL_BINDIR}/examples
)