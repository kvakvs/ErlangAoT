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

add_executable(lexer_tests compiler/lexer.cpp)
erlang_aot_project_options(lexer_tests)
target_link_libraries(lexer_tests PRIVATE erlang_frontend)
add_test(NAME lexer COMMAND lexer_tests)
add_executable(lexer_dump compiler/lexer_dump.cpp)
erlang_aot_project_options(lexer_dump)
target_link_libraries(lexer_dump PRIVATE erlang_frontend)
add_test(NAME scanner_oracle COMMAND "${CMAKE_COMMAND}"
    "-DESCRIPT=${ERLANG_AOT_ESCRIPT}"
    "-DHARNESS=${CMAKE_CURRENT_SOURCE_DIR}/compiler/preprocessor/scan.escript"
    "-DFIXTURES=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/preprocessor"
    "-DDUMP=$<TARGET_FILE:lexer_dump>"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/compiler/preprocessor/scan.cmake")
set_tests_properties(scanner_oracle PROPERTIES SKIP_REGULAR_EXPRESSION "SKIP:")
add_test(NAME lexer_golden COMMAND "${CMAKE_COMMAND}"
    "-DFIXTURES=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/preprocessor"
    "-DDUMP=$<TARGET_FILE:lexer_dump>"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/compiler/preprocessor/golden.cmake")

add_executable(preprocessor_forms compiler/preprocessor/forms.cpp)
erlang_aot_project_options(preprocessor_forms)
target_link_libraries(preprocessor_forms PRIVATE erlang_frontend)
add_test(NAME preprocessor_forms COMMAND preprocessor_forms)
set_tests_properties(preprocessor_forms PROPERTIES TIMEOUT 10)

add_executable(preprocessor_semantics compiler/preprocessor/semantics.cpp)
erlang_aot_project_options(preprocessor_semantics)
target_link_libraries(preprocessor_semantics PRIVATE erlang_frontend)
add_test(NAME preprocessor_semantics COMMAND preprocessor_semantics)
set_tests_properties(preprocessor_semantics PROPERTIES TIMEOUT 20)

add_executable(preprocessor_dump compiler/preprocessor/dump.cpp)
erlang_aot_project_options(preprocessor_dump)
target_link_libraries(preprocessor_dump PRIVATE erlang_frontend)
foreach(mode IN ITEMS golden oracle)
    if(mode STREQUAL "oracle")
        set(live TRUE)
    else()
        set(live FALSE)
    endif()
    add_test(NAME preprocessor_${mode} COMMAND "${CMAKE_COMMAND}"
        "-DDUMP=$<TARGET_FILE:preprocessor_dump>"
        "-DFIXTURES=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/preprocessor"
        "-DESCRIPT=${ERLANG_AOT_ESCRIPT}"
        "-DHARNESS=${CMAKE_CURRENT_SOURCE_DIR}/compiler/preprocessor/epp_scan.escript"
        "-DLIVE=${live}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/compiler/preprocessor/semantic.cmake")
endforeach()
set_tests_properties(preprocessor_oracle PROPERTIES SKIP_REGULAR_EXPRESSION "SKIP:" TIMEOUT 60)
