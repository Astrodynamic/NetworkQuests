# FTP protocol implementation
set(FTP_SOURCES
    ftp_utils.cpp
    ftp_message.cpp
    ftp_data_connection.cpp
    ftp_client.cpp
    ftp_session.cpp
    ftp_server.cpp
)

add_library(networkquests_ftp ${FTP_SOURCES})
target_include_directories(networkquests_ftp PUBLIC 
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(networkquests_ftp 
    PUBLIC 
        networkquests_core
        networkquests_tcp
        Threads::Threads
)

# Platform-specific libraries
if(WIN32)
    target_link_libraries(networkquests_ftp PUBLIC ws2_32 wsock32)
endif()

# Set properties
set_target_properties(networkquests_ftp PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)