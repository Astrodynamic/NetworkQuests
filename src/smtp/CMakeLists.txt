# SMTP Protocol Implementation

# Create the SMTP library
add_library(networkquests_smtp
    smtp_utils.cpp
    smtp_message.cpp
    smtp_client.cpp
    smtp_server.cpp
)

# Add alias for consistent naming
add_library(NetworkQuests::SMTP ALIAS networkquests_smtp)

# Include directories
target_include_directories(networkquests_smtp
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Link required libraries
target_link_libraries(networkquests_smtp
    PUBLIC
        NetworkQuests::Common
        NetworkQuests::TCP
    PRIVATE
        ${CMAKE_THREAD_LIBS_INIT}
)

# Set target properties
set_target_properties(networkquests_smtp PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
)

# Enable warnings
if(MSVC)
    target_compile_options(networkquests_smtp PRIVATE /W4)
else()
    target_compile_options(networkquests_smtp PRIVATE 
        -Wall -Wextra -Wpedantic -Wno-unused-parameter
    )
endif()

# Platform-specific settings
if(WIN32)
    target_link_libraries(networkquests_smtp PRIVATE ws2_32)
endif()

# Install targets
install(TARGETS networkquests_smtp
    EXPORT NetworkQuestsTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)