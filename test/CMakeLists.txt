file(GLOB_RECURSE TEST_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/*.cpp"
)

add_executable(slothdb_tests ${TEST_SOURCES})

target_link_libraries(slothdb_tests PRIVATE slothdb_lib)

target_include_directories(slothdb_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party/doctest
)

# Where tests find committed fixtures and the engine's sources (the
# thread-start inventory in test_worker_errors.cpp reads src/ and include/).
target_compile_definitions(slothdb_tests PRIVATE
    ZUGODB_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

add_test(NAME slothdb_tests COMMAND slothdb_tests)
