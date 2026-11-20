# Sources are globbed recursively; a fresh checkout or a newly added .cpp
# (e.g. execution/count_group_agg.cpp) is picked up on the next CMake reconfigure.
file(GLOB_RECURSE SLOTHDB_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/common/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/catalog/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/storage/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/main/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/parser/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/binder/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/planner/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/execution/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/transaction/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/optimizer/*.cpp"
)

add_library(slothdb_lib STATIC ${SLOTHDB_SOURCES})
set_target_properties(slothdb_lib PROPERTIES OUTPUT_NAME "slothdb")

# AVX2 codegen for the analytics core (the planner). AVX2 has been
# the practical x86 baseline since 2013 (Haswell+). Every intrinsic path is
# guarded (SLOTHDB_PP_HAS_AVX2 / SLOTHDB_HAS_SSE2), so a target built
# without the flag falls back to scalar.
#
# The flag is dropped when it cannot apply: Emscripten, and any build with
# an arm64 slice (native Apple Silicon, or the universal2 macOS wheel),
# where the compiler rejects -mavx2 / /arch:AVX2.
set(SLOTHDB_X86_AVX2 ON)
if(EMSCRIPTEN)
    set(SLOTHDB_X86_AVX2 OFF)
elseif(CMAKE_OSX_ARCHITECTURES)
    if(CMAKE_OSX_ARCHITECTURES MATCHES "arm64")
        set(SLOTHDB_X86_AVX2 OFF)
    endif()
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm|aarch64|ARM64)")
    set(SLOTHDB_X86_AVX2 OFF)
endif()

if(SLOTHDB_X86_AVX2 AND MSVC)
    set(SLOTHDB_AVX2_FLAGS /arch:AVX2)
elseif(SLOTHDB_X86_AVX2)
    set(SLOTHDB_AVX2_FLAGS -mavx2)
else()
    set(SLOTHDB_AVX2_FLAGS "")
endif()

if(SLOTHDB_AVX2_FLAGS)
    target_compile_options(slothdb_lib PRIVATE ${SLOTHDB_AVX2_FLAGS})
endif()

target_include_directories(slothdb_lib PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

# CLI
if(SLOTHDB_BUILD_SHELL)
    set(SHELL_SOURCES ${CMAKE_SOURCE_DIR}/tools/shell/shell.cpp)
    # Vendor linenoise for arrow-key line editing, history, UTF-8 handling
    # on POSIX. Windows uses std::getline + cmd.exe's native line editing.
    if(NOT WIN32)
        list(APPEND SHELL_SOURCES ${CMAKE_SOURCE_DIR}/third_party/linenoise/linenoise.c)
    endif()
    add_executable(slothdb ${SHELL_SOURCES})
    target_link_libraries(slothdb PRIVATE slothdb_lib)
    target_include_directories(slothdb PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/third_party/linenoise)
    target_compile_definitions(slothdb PRIVATE
        SLOTHDB_SHELL_VERSION="${PROJECT_VERSION}")
    if(NOT WIN32)
        target_compile_definitions(slothdb PRIVATE SLOTHDB_HAS_LINENOISE=1)
    endif()
endif()
