# Executable entry selection, startup and linking workflows (F01) through the public CLI.
add_test(NAME linking_entry COMMAND "${CMAKE_COMMAND}"
    "-DTOOL=$<TARGET_FILE:clau>" "-DFIXTURES=${PROJECT_SOURCE_DIR}/tests/fixtures/linking/entry"
    "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/entry/$<CONFIG>"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/entry.cmake")
set_tests_properties(linking_entry PROPERTIES TIMEOUT 120)
add_test(NAME linking_escript COMMAND "${CMAKE_COMMAND}"
    "-DTOOL=$<TARGET_FILE:clau>" "-DFIXTURES=${PROJECT_SOURCE_DIR}/tests/fixtures/linking"
    "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/escript/$<CONFIG>"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/escript.cmake")
set_tests_properties(linking_escript PROPERTIES TIMEOUT 120)
# Startup objects linked with the runtime by the native harness recipe; exit paths and startup IR.
if(CLAUSE_BUILD_RUNTIME)
    add_test(NAME linking_startup COMMAND "${CMAKE_COMMAND}"
        "-DTOOL=$<TARGET_FILE:clau>" "-DSOURCE_ROOT=${PROJECT_SOURCE_DIR}"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/startup/$<CONFIG>" "-DHOST_SETTINGS=${PROJECT_BINARY_DIR}/TestHost.cmake"
        "-DHOST_CONFIG=$<CONFIG>" "-DHOST_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/startup.cmake")
    set_tests_properties(linking_startup PROPERTIES TIMEOUT 600)
    # Executables linked by clau -o; clause_artifacts serves as a same-target archive lacking runtime symbols.
    add_test(NAME linking_executable COMMAND "${CMAKE_COMMAND}"
        "-DTOOL=$<TARGET_FILE:clau>" "-DSOURCE_ROOT=${PROJECT_SOURCE_DIR}"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/executable/$<CONFIG>" "-DHOST_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        "-DOTHER_LIBRARY=$<TARGET_FILE:clause_artifacts>"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/executable.cmake")
    set_tests_properties(linking_executable PROPERTIES TIMEOUT 300)
    # Project targets linked to their manifest outputs: selection, -o/--entry rules, aliasing, deferred publication.
    add_test(NAME linking_project COMMAND "${CMAKE_COMMAND}"
        "-DTOOL=$<TARGET_FILE:clau>" "-DSOURCE_ROOT=${PROJECT_SOURCE_DIR}"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/project/$<CONFIG>" "-DHOST_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/project.cmake")
    set_tests_properties(linking_project PROPERTIES TIMEOUT 300)
endif()
