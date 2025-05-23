# Core library sources
set(CORE_SOURCES
    utils/common.cpp
    utils/logger.cpp
    utils/socket.cpp
)

# Create the core library
add_library(networkquests_core ${CORE_SOURCES})
target_include_directories(networkquests_core PUBLIC 
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(networkquests_core 
    PUBLIC 
        Threads::Threads
)

# Platform-specific libraries
if(WIN32)
    target_link_libraries(networkquests_core PUBLIC ws2_32 wsock32)
endif()

# Optional dependencies
if(Boost_FOUND)
    target_link_libraries(networkquests_core PUBLIC Boost::system)
endif()

if(OpenSSL_FOUND)
    target_link_libraries(networkquests_core PUBLIC OpenSSL::SSL OpenSSL::Crypto)
endif()

# Protocol implementations
add_subdirectory(tcp)
add_subdirectory(udp)
add_subdirectory(http)
add_subdirectory(dns)

# Create main library that includes all protocols
add_library(networkquests INTERFACE)
target_link_libraries(networkquests INTERFACE
    networkquests_core
    networkquests_tcp
    networkquests_udp
    networkquests_http
    networkquests_dns
)

# Export library
export(TARGETS networkquests networkquests_core networkquests_tcp networkquests_udp networkquests_http networkquests_dns
    FILE "${CMAKE_BINARY_DIR}/NetworkQuestsTargets.cmake"
)