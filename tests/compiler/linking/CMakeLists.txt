# Executable entry selection, startup and linking workflows (F01) through the public CLI.
add_test(NAME linking_entry COMMAND "${CMAKE_COMMAND}"
    "-DTOOL=$<TARGET_FILE:erlang_aot>" "-DFIXTURES=${PROJECT_SOURCE_DIR}/tests/fixtures/linking/entry"
    "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/entry/$<CONFIG>"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/entry.cmake")
set_tests_properties(linking_entry PROPERTIES TIMEOUT 120)
add_test(NAME linking_escript COMMAND "${CMAKE_COMMAND}"
    "-DTOOL=$<TARGET_FILE:erlang_aot>" "-DFIXTURES=${PROJECT_SOURCE_DIR}/tests/fixtures/linking"
    "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/escript/$<CONFIG>"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/escript.cmake")
set_tests_properties(linking_escript PROPERTIES TIMEOUT 120)
# Startup objects linked with the runtime by the native harness recipe; exit paths and startup IR.
if(ERLANG_AOT_BUILD_RUNTIME)
    add_test(NAME linking_startup COMMAND "${CMAKE_COMMAND}"
        "-DTOOL=$<TARGET_FILE:erlang_aot>" "-DSOURCE_ROOT=${PROJECT_SOURCE_DIR}"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/startup/$<CONFIG>" "-DHOST_SETTINGS=${PROJECT_BINARY_DIR}/TestHost.cmake"
        "-DHOST_CONFIG=$<CONFIG>" "-DHOST_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/startup.cmake")
    set_tests_properties(linking_startup PROPERTIES TIMEOUT 600)
endif()
