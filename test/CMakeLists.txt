include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.17.0
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include_directories(reader_tests PUBLIC ../include)
link_libraries(
    vm
    GTest::gtest_main
)

add_executable(
    reader_tests
    reader_tests.cpp
)

add_executable(
    object_tests
    object_tests.cpp
)

add_executable(
    allocator_tests
    allocator_tests.cpp
)

add_executable(
    link_tests
    link_tests.cpp
)

add_custom_command(TARGET reader_tests PRE_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_directory
                        ${PROJECT_SOURCE_DIR}/test/test_data/ $<TARGET_FILE_DIR:reader_tests>/test_data)

include(GoogleTest)

gtest_discover_tests(reader_tests)
gtest_discover_tests(object_tests)
gtest_discover_tests(allocator_tests)
gtest_discover_tests(link_tests)