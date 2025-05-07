cmake_minimum_required(VERSION 3.29)
project(not_your_gimp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build main executable
add_executable(not-your-gimp
        src/main.cpp
        src/ProxyServer.h
        src/ProxyServer.cpp
)

target_include_directories(not-your-gimp PRIVATE src)

# Enable tests
enable_testing()
add_subdirectory(tests)
