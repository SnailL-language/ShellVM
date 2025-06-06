add_library(vm src/vm.cpp)
target_include_directories(vm PUBLIC include)

add_subdirectory(test)