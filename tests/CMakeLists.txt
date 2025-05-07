# tests/CMakeLists.txt

include(FetchContent)

FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(tests
        test_proxy.cpp
        ../src/ProxyServer.cpp
)

target_include_directories(tests PRIVATE ../src)
target_link_libraries(tests gtest gtest_main)

include(GoogleTest)
gtest_discover_tests(tests)
