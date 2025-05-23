project(tcp_server VERSION 1.0.0 LANGUAGES CXX)

add_executable(${PROJECT_NAME} main.cc)

add_dependencies(${PROJECT_NAME} SchemaLib)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

target_link_libraries(${PROJECT_NAME} PRIVATE SchemaLib)