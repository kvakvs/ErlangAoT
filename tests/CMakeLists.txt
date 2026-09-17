add_test(NAME cli COMMAND "${CMAKE_COMMAND}"
    "-DTOOL=$<TARGET_FILE:erlang_aot>"
    "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/cli.cmake"
)

add_executable(parser_probe compiler/probe.cpp)
erlang_aot_project_options(parser_probe)
target_link_libraries(parser_probe PRIVATE erlang_frontend)
add_test(NAME parser_probe COMMAND parser_probe)
find_program(ERLANG_AOT_ESCRIPT escript DOC "OTP 29.1 escript for optional oracle tests")
add_test(NAME otp_oracle COMMAND "${CMAKE_COMMAND}"
    "-DESCRIPT=${ERLANG_AOT_ESCRIPT}"
    "-DHARNESS=${CMAKE_CURRENT_SOURCE_DIR}/compiler/preprocessor/oracle.escript"
    "-DFIXTURES=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/preprocessor"
    "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/oracle"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/compiler/preprocessor/oracle.cmake")
set_tests_properties(otp_oracle PROPERTIES SKIP_REGULAR_EXPRESSION "SKIP:")
