# tests/CMakeLists.txt

enable_testing()

# Add GoogleTest from local folder
add_subdirectory(lib/googletest)

# Test executable
add_executable(tests
        test_proxy.cpp
        ../src/ProxyServer.cpp
)

target_include_directories(tests PRIVATE ../src)
target_link_libraries(tests gtest gtest_main pthread)

include(GoogleTest)
gtest_discover_tests(tests)
