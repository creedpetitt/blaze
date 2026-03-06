cmake_minimum_required(VERSION 3.20)
project(blaze)

set(CMAKE_CXX_STANDARD 20)

add_subdirectory(framework)

# Only default to building examples if building the framework directly
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    set(DEFAULT_BUILD_EXAMPLES ON)
else()
    set(DEFAULT_BUILD_EXAMPLES OFF)
endif()

option(BLAZE_BUILD_EXAMPLES "Build the educational examples" ${DEFAULT_BUILD_EXAMPLES})
if(BLAZE_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    enable_testing()
    add_subdirectory(tests)
endif()

# Fuzzing
option(BLAZE_BUILD_FUZZERS "Build LibFuzzer targets (Requires Clang)" OFF)
if(BLAZE_BUILD_FUZZERS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(STATUS "Fuzzing Enabled (Clang detected)")
        add_subdirectory(tests/fuzzers)
    else()
        message(WARNING "BLAZE_BUILD_FUZZERS is ON, but compiler is not Clang. Fuzz targets will be skipped.")
    endif()
endif()

