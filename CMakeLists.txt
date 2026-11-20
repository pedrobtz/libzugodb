cmake_minimum_required(VERSION 3.14)
project(libzugodb VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SLOTHDB_BUILD_TESTS "Build tests" ON)
option(SLOTHDB_BUILD_SHELL "Build CLI shell" OFF)
option(SLOTHDB_SANITIZERS "Enable ASan/UBSan" OFF)
option(SLOTHDB_FUZZ "Build the libFuzzer harnesses in fuzz/ (clang only)" OFF)
option(SLOTHDB_TSAN "Enable ThreadSanitizer" OFF)

if(SLOTHDB_TSAN AND (SLOTHDB_SANITIZERS OR SLOTHDB_FUZZ))
    # TSan cannot be combined with ASan in one binary.
    message(FATAL_ERROR "SLOTHDB_TSAN excludes SLOTHDB_SANITIZERS and SLOTHDB_FUZZ")
endif()

if(SLOTHDB_FUZZ)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "SLOTHDB_FUZZ needs clang (libFuzzer); the compiler is ${CMAKE_CXX_COMPILER_ID}")
    endif()
    # Everything is instrumented for the fuzzer and built with ASan and
    # UBSan, which stop at the first report.
    add_compile_options(-fsanitize=fuzzer-no-link,address,undefined
                        -fno-sanitize-recover=all -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=address,undefined)
endif()

include(cmake/CompilerOptions.cmake)
include(cmake/Platform.cmake)

add_subdirectory(src)

if(SLOTHDB_FUZZ)
    add_subdirectory(fuzz)
endif()

if(SLOTHDB_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()
