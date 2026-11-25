file(GLOB_RECURSE TEST_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/*.cpp"
)

# The conformance runner's tests need its library and program.
if(NOT ZUGODB_BUILD_SLT)
    list(FILTER TEST_SOURCES EXCLUDE REGEX "/unit/tools/")
endif()

add_executable(zugodb_tests ${TEST_SOURCES})

target_link_libraries(zugodb_tests PRIVATE zugodb)
if(ZUGODB_BUILD_SLT)
    target_link_libraries(zugodb_tests PRIVATE zugodb_slt_lib)
    add_dependencies(zugodb_tests zugodb_slt)
    target_compile_definitions(zugodb_tests PRIVATE ZUGODB_SLT_BINARY="$<TARGET_FILE:zugodb_slt>")
endif()

target_include_directories(zugodb_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party/doctest
)

# Where tests find committed fixtures and the engine's sources (the
# thread-start inventory in test_worker_errors.cpp reads src/ and include/).
target_compile_definitions(zugodb_tests PRIVATE
    ZUGODB_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

add_test(NAME zugodb_tests COMMAND zugodb_tests)
