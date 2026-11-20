# The conformance runner (design §5): a library, which the doctest suite
# also links, and the zugodb_slt program. POSIX only: the driver forks a
# child process per file.
add_library(zugodb_slt_lib STATIC
    parser.cpp
    format.cpp
    md5.cpp
    runner.cpp
    driver.cpp
)
target_include_directories(zugodb_slt_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(zugodb_slt_lib PUBLIC slothdb_lib)

add_executable(zugodb_slt main.cpp)
target_link_libraries(zugodb_slt PRIVATE zugodb_slt_lib)
