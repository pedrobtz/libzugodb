# Sources are globbed recursively; a fresh checkout or a newly added .cpp
# (e.g. execution/int_group_agg.cpp) is picked up on the next CMake reconfigure.
file(GLOB_RECURSE SLOTHDB_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/common/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/catalog/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/storage/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/storage/*.c"
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

# AVX2 codegen for the analytics core (parquet + planner). AVX2 has been
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

# miniz is third-party C; suppress warnings-as-errors for it only.
if(MSVC)
    set_source_files_properties(
        ${CMAKE_CURRENT_SOURCE_DIR}/storage/miniz.c
        PROPERTIES COMPILE_FLAGS "/w"
    )
else()
    set_source_files_properties(
        ${CMAKE_CURRENT_SOURCE_DIR}/storage/miniz.c
        PROPERTIES COMPILE_FLAGS "-w"
    )
endif()

# Vendored zstd: decompression-only subset for Parquet ZSTD codec (#7).
# Only lib/common/ and lib/decompress/ are compiled. Compress, dictBuilder,
# legacy, deprecated are excluded. The amd64 .S asm (huf_decompress_amd64.S)
# is also excluded; ZSTD_DISABLE_ASM is defined so the C fallback runs.
set(SLOTHDB_ZSTD_DIR ${CMAKE_SOURCE_DIR}/third_party/zstd/lib)
set(SLOTHDB_ZSTD_SOURCES
    ${SLOTHDB_ZSTD_DIR}/common/debug.c
    ${SLOTHDB_ZSTD_DIR}/common/entropy_common.c
    ${SLOTHDB_ZSTD_DIR}/common/error_private.c
    ${SLOTHDB_ZSTD_DIR}/common/fse_decompress.c
    ${SLOTHDB_ZSTD_DIR}/common/pool.c
    ${SLOTHDB_ZSTD_DIR}/common/threading.c
    ${SLOTHDB_ZSTD_DIR}/common/xxhash.c
    ${SLOTHDB_ZSTD_DIR}/common/zstd_common.c
    ${SLOTHDB_ZSTD_DIR}/decompress/huf_decompress.c
    ${SLOTHDB_ZSTD_DIR}/decompress/zstd_ddict.c
    ${SLOTHDB_ZSTD_DIR}/decompress/zstd_decompress.c
    ${SLOTHDB_ZSTD_DIR}/decompress/zstd_decompress_block.c
)
add_library(slothdb_zstd STATIC ${SLOTHDB_ZSTD_SOURCES})
target_include_directories(slothdb_zstd PUBLIC ${SLOTHDB_ZSTD_DIR})
target_compile_definitions(slothdb_zstd PRIVATE
    ZSTD_DISABLE_ASM=1
    ZSTD_MULTITHREAD=0
)
if(MSVC)
    target_compile_options(slothdb_zstd PRIVATE /w)
else()
    target_compile_options(slothdb_zstd PRIVATE -w)
endif()
set_target_properties(slothdb_zstd PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(slothdb_lib PRIVATE slothdb_zstd)
target_include_directories(slothdb_lib PRIVATE ${SLOTHDB_ZSTD_DIR})

# FastPFor bitpacking: per-bit-width specialized 32-element unpackers.
# Replaces the scalar bit-pack loop in RleDecoder::NextBatch with a
# vectorizable fixed-shift unrolled body. Licensed Apache 2.0, vendored
# from DuckDB's third_party/fastpforlib.
add_library(slothdb_fastpfor STATIC
    ${CMAKE_SOURCE_DIR}/third_party/fastpforlib/bitpacking.cpp)
target_include_directories(slothdb_fastpfor PUBLIC
    ${CMAKE_SOURCE_DIR}/third_party/fastpforlib)
if(MSVC)
    target_compile_options(slothdb_fastpfor PRIVATE /w)
else()
    target_compile_options(slothdb_fastpfor PRIVATE -w)
endif()
set_target_properties(slothdb_fastpfor PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(slothdb_lib PRIVATE slothdb_fastpfor)
target_include_directories(slothdb_lib PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party/fastpforlib)

# Vendored google/snappy 1.2.1 — fast back-ref + literal copy (16-byte SSSE3
# shuffle + IncrementalCopy slop buffer). Replaces the hand-rolled byte-by-byte
# back-ref loop in SnappyDecompressBody for VARCHAR-heavy ClickBench queries
# (Q15/Q22/Q23/Q26 — URL/Title/SearchPhrase with high back-ref density).
set(SLOTHDB_SNAPPY_DIR ${CMAKE_SOURCE_DIR}/third_party/snappy)
add_library(slothdb_snappy STATIC
    ${SLOTHDB_SNAPPY_DIR}/snappy.cc
    ${SLOTHDB_SNAPPY_DIR}/snappy-sinksource.cc
    ${SLOTHDB_SNAPPY_DIR}/snappy-stubs-internal.cc
    ${SLOTHDB_SNAPPY_DIR}/snappy-c.cc)
target_include_directories(slothdb_snappy PUBLIC ${SLOTHDB_SNAPPY_DIR})
# AVX2 enables BMI2 (PEXT) for the back-ref offset extraction and SSSE3 for
# the 16-byte vector-byte-shuffle in literal copy. Dropped on non-x86
# builds; snappy's portable path covers them.
if(MSVC)
    target_compile_options(slothdb_snappy PRIVATE /w ${SLOTHDB_AVX2_FLAGS})
elseif(SLOTHDB_X86_AVX2)
    target_compile_options(slothdb_snappy PRIVATE -w -mavx2 -mbmi2)
else()
    target_compile_options(slothdb_snappy PRIVATE -w)
endif()
set_target_properties(slothdb_snappy PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(slothdb_lib PRIVATE slothdb_snappy)
target_include_directories(slothdb_lib PRIVATE ${SLOTHDB_SNAPPY_DIR})

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
