cmake_minimum_required(VERSION 3.16)
project(Topomap LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_library(topo_io
    src/topo_io.cpp
    src/cost.cpp
    src/mapping.cpp
    )
target_include_directories(topo_io PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(topomap app/main.cpp)
target_link_libraries(topomap PRIVATE topo_io)
target_compile_definitions(topomap PRIVATE TESTS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests")
