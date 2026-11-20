# libFuzzer harnesses (SLOTHDB_FUZZ=ON, clang only). The engine itself is
# built with -fsanitize=fuzzer-no-link,address,undefined by the top-level
# CMakeLists.txt, so the fuzzer sees its coverage.

add_executable(checkpoint_load_fuzzer checkpoint_load.cpp)
target_link_libraries(checkpoint_load_fuzzer PRIVATE slothdb_lib)
target_link_options(checkpoint_load_fuzzer PRIVATE -fsanitize=fuzzer,address,undefined)
