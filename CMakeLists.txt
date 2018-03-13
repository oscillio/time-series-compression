cmake_minimum_required(VERSION 3.0)

project(tscompress)

############################################################
# Enable C++11 Tests
############################################################
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


############################################################
# Create the Time Series Compression Library
############################################################

# Source files to be used in the library
set(ts-compress_SOURCES
    lib/TimeSeriesCompression.cpp
)

#Generate the static library from the library sources
add_library(ts-compress STATIC ${ts-compress_SOURCES})

target_include_directories(ts-compress
    PUBLIC ${PROJECT_SOURCE_DIR}/lib
)


############################################################
# Create tests
############################################################

# Source fles for the binary
set(test_SOURCES
    test/tests.cpp
)

# Add an executable with the above sources
add_executable(ts-compress-test ${test_SOURCES})

# link the new hello_library target with the hello_binary target
target_link_libraries( ts-compress-test
    PRIVATE ts-compress
)
