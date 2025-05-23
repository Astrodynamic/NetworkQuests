# SNMP Protocol Implementation
cmake_minimum_required(VERSION 3.16)

# SNMP source files
set(SNMP_SOURCES
    snmp_utils.cpp
    snmp_core.cpp
    snmp_message.cpp
    snmp_manager.cpp
    snmp_agent.cpp
)

# Create SNMP library
add_library(networkquests_snmp ${SNMP_SOURCES})

# Add alias for consistent naming
add_library(NetworkQuests::SNMP ALIAS networkquests_snmp)

# Include directories
target_include_directories(networkquests_snmp
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Link dependencies
target_link_libraries(networkquests_snmp
    PUBLIC
        NetworkQuests::UDP     # SNMP uses UDP for transport
        NetworkQuests::Common  # For Result<T>, logging, etc.
    PRIVATE
        Threads::Threads       # For multi-threading in agent
)

# Compiler features
target_compile_features(networkquests_snmp
    PUBLIC
        cxx_std_20
)

# Compiler options
if(ENABLE_WARNINGS)
    if(MSVC)
        target_compile_options(networkquests_snmp PRIVATE /W4)
    else()
        target_compile_options(networkquests_snmp PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()

if(ENABLE_SANITIZERS AND NOT MSVC)
    target_compile_options(networkquests_snmp PRIVATE -fsanitize=address,undefined)
    target_link_options(networkquests_snmp PRIVATE -fsanitize=address,undefined)
endif()

# Properties
set_target_properties(networkquests_snmp PROPERTIES
    OUTPUT_NAME "networkquests_snmp"
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    POSITION_INDEPENDENT_CODE ON
)

# Installation
if(INSTALL_NETWORKQUESTS)
    install(TARGETS networkquests_snmp
        EXPORT NetworkQuestsTargets
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
    )
endif()