cmake_minimum_required(VERSION 4.0.0)

project(
    ShellVM
    VERSION 1.0.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD_REQUIRED 20)
set(CMAKE_CXX_STANDARD 20)

enable_testing()

add_subdirectory(VM)

add_executable(${PROJECT_NAME} main.cpp)
target_include_directories(${PROJECT_NAME} PUBLIC VM/include)
target_link_libraries(${PROJECT_NAME} PRIVATE vm)