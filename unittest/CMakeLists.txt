file(GLOB_RECURSE memmy_test_sources CONFIGURE_DEPENDS *.c)

add_executable(memmy_test ${memmy_test_sources})
target_compile_definitions(memmy_test PRIVATE BUILD_TEST)
target_include_directories(memmy_test PRIVATE .)
target_link_libraries(memmy_test PRIVATE memmy_cli memmy_eval memmy_ast memmy base)

if(CMAKE_CONFIGURATION_TYPES)
    set(memmy_test_ctest_cases ${CMAKE_CURRENT_BINARY_DIR}/memmy_ctest_cases_$<CONFIG>.cmake)
    set(memmy_test_ctest_include ${CMAKE_CURRENT_BINARY_DIR}/memmy_ctest_include.cmake)
    file(WRITE ${memmy_test_ctest_include}
        "if(DEFINED CTEST_CONFIGURATION_TYPE AND NOT CTEST_CONFIGURATION_TYPE STREQUAL \"\")\n"
        "    include(\"${CMAKE_CURRENT_BINARY_DIR}/memmy_ctest_cases_\${CTEST_CONFIGURATION_TYPE}.cmake\" OPTIONAL)\n"
        "elseif(EXISTS \"${CMAKE_CURRENT_BINARY_DIR}/memmy_ctest_cases_Debug.cmake\")\n"
        "    include(\"${CMAKE_CURRENT_BINARY_DIR}/memmy_ctest_cases_Debug.cmake\")\n"
        "elseif(EXISTS \"${CMAKE_CURRENT_BINARY_DIR}/memmy_ctest_cases_Release.cmake\")\n"
        "    include(\"${CMAKE_CURRENT_BINARY_DIR}/memmy_ctest_cases_Release.cmake\")\n"
        "endif()\n"
    )
else()
    set(memmy_test_ctest_cases ${CMAKE_CURRENT_BINARY_DIR}/memmy_ctest_cases.cmake)
    set(memmy_test_ctest_include ${memmy_test_ctest_cases})
    file(WRITE ${memmy_test_ctest_cases} "# Generated after memmy_test builds\n")
endif()

add_custom_target(
    memmy_test_discover ALL
    COMMAND
        ${CMAKE_COMMAND}
        -DTEST_EXECUTABLE=$<TARGET_FILE:memmy_test>
        -DWORKING_DIRECTORY=${CMAKE_SOURCE_DIR}
        -DOUTPUT_FILE=${memmy_test_ctest_cases}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/discover_memmy_tests.cmake
    DEPENDS memmy_test
    VERBATIM
)

set_property(DIRECTORY APPEND PROPERTY TEST_INCLUDE_FILES ${memmy_test_ctest_include})
