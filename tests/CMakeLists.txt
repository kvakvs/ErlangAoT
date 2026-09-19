include(${PROJECT_SOURCE_DIR}/cmake/ErlangDependencies.cmake)
set(ERLANG_AOT_OTP_SOURCE_ROOT "${PROJECT_SOURCE_DIR}/references/otp" CACHE PATH
    "Optional pinned OTP 29.1 source checkout for parser reduction and real-source tests")
foreach(name IN ITEMS coverage corpus)
    add_test(NAME parser_${name} COMMAND "${CMAKE_COMMAND}"
        "-DOTP_ROOT=${ERLANG_AOT_OTP_SOURCE_ROOT}" "-DTOOL=$<TARGET_FILE:erlang_aot>"
        "-DFIXTURES=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/parser" "-DESCRIPT=${ERLANG_AOT_ESCRIPT}"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/${name}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/compiler/parser/${name}.cmake")
    set_tests_properties(parser_${name} PROPERTIES TIMEOUT 120
        SKIP_REGULAR_EXPRESSION "SKIP: pinned OTP source checkout unavailable")
endforeach()
foreach(mode IN ITEMS golden oracle)
    if(mode STREQUAL "oracle")
        set(live TRUE)
    else()
        set(live FALSE)
    endif()
    foreach(suite IN ITEMS historical phase6)
        set(script historical.cmake)
        if(suite STREQUAL "phase6")
            set(script phase2.cmake)
        endif()
        add_test(NAME parser_${suite}_${mode} COMMAND "${CMAKE_COMMAND}"
            "-DPHASE=phase6" "-DFIXTURES=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/parser"
            "-DDUMP=$<TARGET_FILE:parser_dump>" "-DESCRIPT=${ERLANG_AOT_ESCRIPT}"
            "-DHARNESS=${CMAKE_CURRENT_SOURCE_DIR}/compiler/parser/oracle.escript" "-DLIVE=${live}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/compiler/parser/${script}")
        set_tests_properties(parser_${suite}_${mode} PROPERTIES TIMEOUT 90)
    endforeach()
endforeach()
foreach(name IN ITEMS hardening mutations consumer)
    add_executable(parser_${name} compiler/parser/${name}.cpp)
    erlang_aot_project_options(parser_${name})
    target_link_libraries(parser_${name} PRIVATE erlang_frontend)
    add_test(NAME parser_${name} COMMAND parser_${name})
    set_tests_properties(parser_${name} PROPERTIES TIMEOUT 60)
endforeach()

add_test(NAME cli COMMAND "${CMAKE_COMMAND}"
    "-DTOOL=$<TARGET_FILE:erlang_aot>"
    "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/cli.cmake"
)

add_executable(printing_tests compiler/printing.cpp)
erlang_aot_project_options(printing_tests)
target_link_libraries(printing_tests PRIVATE erlang_frontend)
add_test(NAME printing COMMAND printing_tests)

add_executable(printing_ast_tests compiler/printing_ast.cpp)
erlang_aot_project_options(printing_ast_tests)
target_link_libraries(printing_ast_tests PRIVATE erlang_frontend)
add_test(NAME printing_ast COMMAND printing_ast_tests)
add_executable(parser_control compiler/parser/control.cpp)
erlang_aot_project_options(parser_control)
target_include_directories(parser_control PRIVATE "${PROJECT_SOURCE_DIR}/compiler/src")
target_link_libraries(parser_control PRIVATE erlang_frontend)
add_test(NAME parser_control COMMAND parser_control)
add_executable(parser_exceptions compiler/parser/exceptions.cpp)
erlang_aot_project_options(parser_exceptions)
target_include_directories(parser_exceptions PRIVATE "${PROJECT_SOURCE_DIR}/compiler/src")
target_link_libraries(parser_exceptions PRIVATE erlang_frontend)
add_test(NAME parser_exceptions COMMAND parser_exceptions)
add_executable(parser_comprehensions compiler/parser/comprehensions.cpp)
add_executable(parser_attributes compiler/parser/attributes.cpp)
add_executable(parser_types compiler/parser/types.cpp)
add_executable(parser_specifications compiler/parser/specifications.cpp)
erlang_aot_project_options(parser_specifications)
target_include_directories(parser_specifications PRIVATE "${PROJECT_SOURCE_DIR}/compiler/src")
target_link_libraries(parser_specifications PRIVATE erlang_frontend)
add_test(NAME parser_specifications COMMAND parser_specifications)
erlang_aot_project_options(parser_types)
target_include_directories(parser_types PRIVATE "${PROJECT_SOURCE_DIR}/compiler/src")
target_link_libraries(parser_types PRIVATE erlang_frontend)
add_test(NAME parser_types COMMAND parser_types)
erlang_aot_project_options(parser_attributes)
target_include_directories(parser_attributes PRIVATE "${PROJECT_SOURCE_DIR}/compiler/src")
target_link_libraries(parser_attributes PRIVATE erlang_frontend)
add_test(NAME parser_attributes COMMAND parser_attributes)
erlang_aot_project_options(parser_comprehensions)
target_include_directories(parser_comprehensions PRIVATE "${PROJECT_SOURCE_DIR}/compiler/src")
target_link_libraries(parser_comprehensions PRIVATE erlang_frontend)
add_test(NAME parser_comprehensions COMMAND parser_comprehensions)
set_tests_properties(parser_comprehensions PROPERTIES TIMEOUT 20)
set_tests_properties(parser_exceptions PROPERTIES TIMEOUT 20)
set_tests_properties(parser_control PROPERTIES TIMEOUT 20)
foreach(mode IN ITEMS golden oracle)
    if(mode STREQUAL "oracle")
        set(live TRUE)
    else()
        set(live FALSE)
    endif()
    add_test(NAME parser_phase4_${mode} COMMAND "${CMAKE_COMMAND}"
        "-DPHASE=phase4" "-DFIXTURES=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/parser"
        "-DDUMP=$<TARGET_FILE:parser_dump>" "-DESCRIPT=${ERLANG_AOT_ESCRIPT}"
        "-DHARNESS=${CMAKE_CURRENT_SOURCE_DIR}/compiler/parser/oracle.escript"
        "-DLIVE=${live}" -P "${CMAKE_CURRENT_SOURCE_DIR}/compiler/parser/phase2.cmake")
    add_test(NAME parser_phase5_${mode} COMMAND "${CMAKE_COMMAND}"
        "-DPHASE=phase5" "-DFIXTURES=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/parser"
        "-DDUMP=$<TARGET_FILE:parser_dump>" "-DESCRIPT=${ERLANG_AOT_ESCRIPT}"
        "-DHARNESS=${CMAKE_CURRENT_SOURCE_DIR}/compiler/parser/oracle.escript"
        "-DLIVE=${live}" -P "${CMAKE_CURRENT_SOURCE_DIR}/compiler/parser/phase2.cmake")
endforeach()
set_tests_properties(parser_phase4_oracle PROPERTIES TIMEOUT 180)
set_tests_properties(printing_ast PROPERTIES TIMEOUT 20)

add_executable(parser_probe compiler/probe.cpp)
erlang_aot_project_options(parser_probe)
target_link_libraries(parser_probe PRIVATE erlang_frontend)
add_test(NAME parser_probe COMMAND parser_probe)
add_test(NAME otp_oracle COMMAND "${CMAKE_COMMAND}"
    "-DESCRIPT=${ERLANG_AOT_ESCRIPT}"
    "-DHARNESS=${CMAKE_CURRENT_SOURCE_DIR}/compiler/preprocessor/oracle.escript"
    "-DFIXTURES=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/preprocessor"
    "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/oracle"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/compiler/preprocessor/oracle.cmake")

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
set_tests_properties(preprocessor_oracle PROPERTIES TIMEOUT 60)

add_test(NAME parser_reference COMMAND "${CMAKE_COMMAND}"
    "-DFIXTURES=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/parser"
    "-DDUMP=$<TARGET_FILE:lexer_dump>"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/compiler/parser/reference.cmake")
add_test(NAME parser_oracle COMMAND "${CMAKE_COMMAND}"
    "-DFIXTURES=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/parser"
    "-DESCRIPT=${ERLANG_AOT_ESCRIPT}"
    "-DHARNESS=${CMAKE_CURRENT_SOURCE_DIR}/compiler/parser/oracle.escript"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/compiler/parser/oracle.cmake")
set_tests_properties(parser_oracle PROPERTIES TIMEOUT 180)

add_executable(parser_tokens compiler/parser/tokens.cpp)
erlang_aot_project_options(parser_tokens)
target_include_directories(parser_tokens PRIVATE "${PROJECT_SOURCE_DIR}/compiler/src")
target_link_libraries(parser_tokens PRIVATE erlang_frontend)
add_test(NAME parser_tokens COMMAND parser_tokens)

add_executable(parser_ast compiler/parser/ast.cpp)
erlang_aot_project_options(parser_ast)
target_include_directories(parser_ast PRIVATE "${PROJECT_SOURCE_DIR}/compiler/src")
target_link_libraries(parser_ast PRIVATE erlang_frontend)
add_test(NAME parser_ast COMMAND parser_ast)

add_executable(parser_forms compiler/parser/forms.cpp)
erlang_aot_project_options(parser_forms)
target_link_libraries(parser_forms PRIVATE erlang_frontend)
add_test(NAME parser_forms COMMAND parser_forms)
set_tests_properties(parser_forms PROPERTIES TIMEOUT 20)

add_executable(parser_dump compiler/parser/dump.cpp)
erlang_aot_project_options(parser_dump)
target_link_libraries(parser_dump PRIVATE erlang_frontend)
foreach(mode IN ITEMS golden oracle)
    if(mode STREQUAL "oracle")
        set(live TRUE)
    else()
        set(live FALSE)
    endif()
    add_test(NAME parser_phase1_${mode} COMMAND "${CMAKE_COMMAND}"
        "-DFIXTURES=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/parser"
        "-DDUMP=$<TARGET_FILE:parser_dump>"
        "-DESCRIPT=${ERLANG_AOT_ESCRIPT}"
        "-DHARNESS=${CMAKE_CURRENT_SOURCE_DIR}/compiler/parser/oracle.escript"
        "-DLIVE=${live}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/compiler/parser/phase1.cmake")
endforeach()
set_tests_properties(parser_phase1_oracle PROPERTIES TIMEOUT 60)

foreach(mode IN ITEMS golden oracle)
    if(mode STREQUAL "oracle")
        set(live TRUE)
    else()
        set(live FALSE)
    endif()
    add_test(NAME parser_phase2_${mode} COMMAND "${CMAKE_COMMAND}"
        "-DFIXTURES=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/parser"
        "-DDUMP=$<TARGET_FILE:parser_dump>"
        "-DESCRIPT=${ERLANG_AOT_ESCRIPT}"
        "-DHARNESS=${CMAKE_CURRENT_SOURCE_DIR}/compiler/parser/oracle.escript"
        "-DLIVE=${live}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/compiler/parser/phase2.cmake")
endforeach()
set_tests_properties(parser_phase2_oracle PROPERTIES TIMEOUT 180)

add_executable(parser_expressions compiler/parser/expressions.cpp)
erlang_aot_project_options(parser_expressions)
target_link_libraries(parser_expressions PRIVATE erlang_frontend)
add_test(NAME parser_expressions COMMAND parser_expressions)
set_tests_properties(parser_expressions PROPERTIES TIMEOUT 20)

add_executable(parser_clauses compiler/parser/clauses.cpp)
erlang_aot_project_options(parser_clauses)
target_include_directories(parser_clauses PRIVATE "${PROJECT_SOURCE_DIR}/compiler/src")
target_link_libraries(parser_clauses PRIVATE erlang_frontend)
add_test(NAME parser_clauses COMMAND parser_clauses)
set_tests_properties(parser_clauses PROPERTIES TIMEOUT 20)

foreach(mode IN ITEMS golden oracle)
    if(mode STREQUAL "oracle")
        set(live TRUE)
    else()
        set(live FALSE)
    endif()
    add_test(NAME parser_phase3_${mode} COMMAND "${CMAKE_COMMAND}"
        "-DPHASE=phase3"
        "-DFIXTURES=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/parser"
        "-DDUMP=$<TARGET_FILE:parser_dump>"
        "-DESCRIPT=${ERLANG_AOT_ESCRIPT}"
        "-DHARNESS=${CMAKE_CURRENT_SOURCE_DIR}/compiler/parser/oracle.escript"
        "-DLIVE=${live}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/compiler/parser/phase2.cmake")
endforeach()
set_tests_properties(parser_phase3_oracle PROPERTIES TIMEOUT 180)

add_executable(parser_structural compiler/parser/structural.cpp)
erlang_aot_project_options(parser_structural)
target_include_directories(parser_structural PRIVATE "${PROJECT_SOURCE_DIR}/compiler/src")
target_link_libraries(parser_structural PRIVATE erlang_frontend)
add_test(NAME parser_structural COMMAND parser_structural)
set_tests_properties(parser_structural PROPERTIES TIMEOUT 20)

add_executable(parser_binaries compiler/parser/binaries.cpp)
erlang_aot_project_options(parser_binaries)
target_include_directories(parser_binaries PRIVATE "${PROJECT_SOURCE_DIR}/compiler/src")
target_link_libraries(parser_binaries PRIVATE erlang_frontend)
add_test(NAME parser_binaries COMMAND parser_binaries)
set_tests_properties(parser_binaries PROPERTIES TIMEOUT 20)

add_executable(project_dependency compiler/project/dependency.cpp)
erlang_aot_project_options(project_dependency)
target_link_libraries(project_dependency PRIVATE erlang_project_toml)
add_test(NAME project_dependency COMMAND project_dependency)

add_executable(project_model compiler/project/model.cpp)
erlang_aot_project_options(project_model)
target_link_libraries(project_model PRIVATE erlang_project)
add_test(NAME project_model COMMAND project_model)
