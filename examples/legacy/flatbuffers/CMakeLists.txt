cmake_minimum_required(VERSION 3.24.2)

project(SchemaLib VERSION 0.0.1 LANGUAGES CXX)

find_package(Flatbuffers REQUIRED)

set(GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/include)
file(MAKE_DIRECTORY ${GENERATED_DIR})

set(FBS_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/robot/common.fbs
    ${CMAKE_CURRENT_SOURCE_DIR}/robot/commands.fbs
    ${CMAKE_CURRENT_SOURCE_DIR}/robot/rtd.fbs
)

foreach(FBS_FILE ${FBS_FILES})
    get_filename_component(FBS_FILENAME ${FBS_FILE} NAME_WE)
    set(GENERATED_HEADER ${GENERATED_DIR}/${FBS_FILENAME}_generated.h)

    add_custom_command(
        OUTPUT ${GENERATED_HEADER}
        COMMAND flatc --cpp --gen-mutable -o ${GENERATED_DIR} ${FBS_FILE}
        DEPENDS ${FBS_FILE}
        COMMENT "Generating FlatBuffers header for ${FBS_FILE}"
    )

    list(APPEND GENERATED_HEADERS ${GENERATED_HEADER})
endforeach()

add_custom_target(SchemaGenerate ALL DEPENDS ${GENERATED_HEADERS})

add_library(${PROJECT_NAME} INTERFACE)
target_include_directories(${PROJECT_NAME} INTERFACE ${GENERATED_DIR})
target_link_libraries(${PROJECT_NAME} INTERFACE flatbuffers)
add_dependencies(${PROJECT_NAME} SchemaGenerate)