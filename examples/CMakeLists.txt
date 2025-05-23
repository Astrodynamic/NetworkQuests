# NetworkQuests Examples

# FlatBuffers Examples (optional)
option(BUILD_FLATBUFFERS_EXAMPLES "Build FlatBuffers network examples" OFF)
if(BUILD_FLATBUFFERS_EXAMPLES)
    add_subdirectory(flatbuffers)
endif()

# TCP Examples
add_executable(tcp_echo_server tcp_echo_server.cpp)
target_link_libraries(tcp_echo_server PRIVATE networkquests)

add_executable(tcp_echo_client tcp_echo_client.cpp)
target_link_libraries(tcp_echo_client PRIVATE networkquests)

# UDP Examples
add_executable(udp_echo_server udp_echo_server.cpp)
target_link_libraries(udp_echo_server PRIVATE networkquests)

add_executable(udp_echo_client udp_echo_client.cpp)
target_link_libraries(udp_echo_client PRIVATE networkquests)

# HTTP Examples
add_executable(http_server http_server.cpp)
target_link_libraries(http_server PRIVATE networkquests)

add_executable(http_client http_client.cpp)
target_link_libraries(http_client PRIVATE networkquests)

# DNS Examples
add_executable(dns_server dns_server.cpp)
target_link_libraries(dns_server PRIVATE networkquests)

add_executable(dns_client dns_client.cpp)
target_link_libraries(dns_client PRIVATE networkquests)

# FTP Examples
add_executable(ftp_server ftp_server.cpp)
target_link_libraries(ftp_server PRIVATE networkquests)

add_executable(ftp_client ftp_client.cpp)
target_link_libraries(ftp_client PRIVATE networkquests)

# WebSocket Examples
add_executable(websocket_server websocket_server.cpp)
target_link_libraries(websocket_server PRIVATE networkquests)

add_executable(websocket_client websocket_client.cpp)
target_link_libraries(websocket_client PRIVATE networkquests)

# SMTP Examples
add_executable(smtp_server smtp_server.cpp)
target_link_libraries(smtp_server PRIVATE networkquests)

add_executable(smtp_client smtp_client.cpp)
target_link_libraries(smtp_client PRIVATE networkquests)

# SNMP Examples
add_executable(snmp_agent snmp_agent.cpp)
target_link_libraries(snmp_agent PRIVATE networkquests)

add_executable(snmp_client snmp_client.cpp)
target_link_libraries(snmp_client PRIVATE networkquests)

# Install main examples
install(TARGETS 
    tcp_echo_server tcp_echo_client 
    udp_echo_server udp_echo_client
    http_server http_client
    dns_server dns_client
    ftp_server ftp_client
    websocket_server websocket_client
    smtp_server smtp_client
    snmp_agent snmp_client
    DESTINATION bin/examples
)