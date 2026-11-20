file(GLOB_RECURSE TEST_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/*.cpp"
)

# The conformance runner's tests need its library and program.
if(NOT SLOTHDB_BUILD_SLT)
    list(FILTER TEST_SOURCES EXCLUDE REGEX "/unit/tools/")
endif()

add_executable(slothdb_tests ${TEST_SOURCES})

target_link_libraries(slothdb_tests PRIVATE slothdb_lib)
if(SLOTHDB_BUILD_SLT)
    target_link_libraries(slothdb_tests PRIVATE zugodb_slt_lib)
    add_dependencies(slothdb_tests zugodb_slt)
    target_compile_definitions(slothdb_tests PRIVATE ZUGODB_SLT_BINARY="$<TARGET_FILE:zugodb_slt>")
endif()

target_include_directories(slothdb_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party/doctest
)

# Where tests find committed fixtures and the engine's sources (the
# thread-start inventory in test_worker_errors.cpp reads src/ and include/).
target_compile_definitions(slothdb_tests PRIVATE
    ZUGODB_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

add_test(NAME slothdb_tests COMMAND slothdb_tests)
