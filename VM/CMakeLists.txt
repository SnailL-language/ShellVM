add_library(vm src/code.cpp src/runtime.cpp src/reader.cpp)
target_include_directories(vm PUBLIC include)

add_subdirectory(test)