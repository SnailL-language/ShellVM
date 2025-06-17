cmake_minimum_required(VERSION 4.0.0)

project(
    ShellVM
    VERSION 1.0.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD_REQUIRED 20)
set(CMAKE_CXX_STANDARD 20)

set(VM_SOURCE_DIR ${PROJECT_SOURCE_DIR}/VM)

if (CMAKE_BUILD_TYPE STREQUAL "Debug") 
    add_compile_options(-Wall -fsanitize=address)
    add_link_options(-fsanitize=address)

    enable_testing()
endif()

add_library(vm src/code.cpp src/runtime.cpp src/reader.cpp src/allocator.cpp src/process.cpp)
target_include_directories(vm PUBLIC include)

if (CMAKE_BUILD_TYPE STREQUAL "Debug") 
    add_subdirectory(test)
endif()

add_executable(${PROJECT_NAME} main.cpp)
target_include_directories(${PROJECT_NAME} PUBLIC include)
target_link_libraries(${PROJECT_NAME} PRIVATE vm)