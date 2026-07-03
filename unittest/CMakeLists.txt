file(GLOB_RECURSE MEMMY_TEST_SOURCES CONFIGURE_DEPENDS *.c)

add_executable(memmy_test ${MEMMY_TEST_SOURCES})
target_compile_definitions(memmy_test PRIVATE BUILD_TEST)
target_include_directories(memmy_test PRIVATE . ../cmd/memmy)
target_sources(memmy_test PRIVATE ../cmd/memmy/memmy_cli.c)
target_link_libraries(memmy_test PRIVATE memmy base)

add_test(
    NAME unit
    COMMAND $<TARGET_FILE:memmy_test>
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
