cmake_minimum_required(VERSION 3.14)
project(NotYourGimpTests)

set(CMAKE_CXX_STANDARD 17)

include(FetchContent)

# Fetch GoogleTest
FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
)
FetchContent_MakeAvailable(googletest)

# Find curl
find_package(CURL REQUIRED)

enable_testing()

# Add the test executable
add_executable(tests
        test_main.cpp
        test_proxy.cpp
        ../src/ProxyServer.cpp
)

# Include src headers
target_include_directories(tests PRIVATE ../src)

# Link against GTest
target_link_libraries(tests GTest::gtest GTest::gtest_main pthread CURL::libcurl)

# Automatically discover tests
include(GoogleTest)
gtest_discover_tests(tests)
