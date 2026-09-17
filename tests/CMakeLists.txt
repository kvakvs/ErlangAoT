add_test(NAME cli COMMAND "${CMAKE_COMMAND}"
    "-DTOOL=$<TARGET_FILE:erlang_aot>"
    "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/cli.cmake"
)
