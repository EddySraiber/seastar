cmake_minimum_required(VERSION 3.5)
project(kv_server)

# Set C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Seastar
find_package(Seastar REQUIRED)

# Create executable
add_executable(kv_server
    main.cc
    kv_store.cc)

# Link with Seastar
target_link_libraries(kv_server
    Seastar::seastar)

# Set output directory
set_target_properties(kv_server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Compiler-specific options
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(kv_server PRIVATE
        -Wall
        -Wextra
        -Wno-unused-parameter
        -Wno-missing-field-initializers)
endif()

# Install target
install(TARGETS kv_server
    RUNTIME DESTINATION bin)