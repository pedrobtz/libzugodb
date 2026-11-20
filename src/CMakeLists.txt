# Sources are globbed recursively; a fresh checkout or a newly added .cpp
# (e.g. execution/count_group_agg.cpp) is picked up on the next CMake reconfigure.
file(GLOB_RECURSE ZUGODB_SOURCES
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

add_library(zugodb STATIC ${ZUGODB_SOURCES})

# AVX2 codegen for the analytics core (the planner). AVX2 has been
# the practical x86 baseline since 2013 (Haswell+). Every intrinsic path is
# guarded (ZUGODB_PP_HAS_AVX2 / ZUGODB_HAS_SSE2), so a target built
# without the flag falls back to scalar.
#
# The flag is dropped when it cannot apply: Emscripten, and any build with
# an arm64 slice (native Apple Silicon, or the universal2 macOS wheel),
# where the compiler rejects -mavx2 / /arch:AVX2.
set(ZUGODB_X86_AVX2 ON)
if(EMSCRIPTEN)
    set(ZUGODB_X86_AVX2 OFF)
elseif(CMAKE_OSX_ARCHITECTURES)
    if(CMAKE_OSX_ARCHITECTURES MATCHES "arm64")
        set(ZUGODB_X86_AVX2 OFF)
    endif()
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm|aarch64|ARM64)")
    set(ZUGODB_X86_AVX2 OFF)
endif()

if(ZUGODB_X86_AVX2 AND MSVC)
    set(ZUGODB_AVX2_FLAGS /arch:AVX2)
elseif(ZUGODB_X86_AVX2)
    set(ZUGODB_AVX2_FLAGS -mavx2)
else()
    set(ZUGODB_AVX2_FLAGS "")
endif()

if(ZUGODB_AVX2_FLAGS)
    target_compile_options(zugodb PRIVATE ${ZUGODB_AVX2_FLAGS})
endif()

target_include_directories(zugodb PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

# CLI
if(ZUGODB_BUILD_SHELL)
    set(SHELL_SOURCES ${CMAKE_SOURCE_DIR}/tools/shell/shell.cpp)
    # Vendor linenoise for arrow-key line editing, history, UTF-8 handling
    # on POSIX. Windows uses std::getline + cmd.exe's native line editing.
    if(NOT WIN32)
        list(APPEND SHELL_SOURCES ${CMAKE_SOURCE_DIR}/third_party/linenoise/linenoise.c)
    endif()
    add_executable(zugodb_shell ${SHELL_SOURCES})
    # The target is zugodb_shell, the library being zugodb; the executable is zugodb.
    set_target_properties(zugodb_shell PROPERTIES OUTPUT_NAME "zugodb")
    target_link_libraries(zugodb_shell PRIVATE zugodb)
    target_include_directories(zugodb_shell PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/third_party/linenoise)
    target_compile_definitions(zugodb_shell PRIVATE
        ZUGODB_SHELL_VERSION="${PROJECT_VERSION}")
    if(NOT WIN32)
        target_compile_definitions(zugodb_shell PRIVATE ZUGODB_HAS_LINENOISE=1)
    endif()
endif()
