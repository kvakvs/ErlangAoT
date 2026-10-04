# Executable golden runner (tests/fixtures/executables/README.md): every case directory holding a golden.json
# links under the fast/full policy matrix and runs against its OTP-generated stdout and exit status.
if(ERLANG_AOT_BUILD_RUNTIME)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    file(GLOB goldens CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/tests/fixtures/executables/*/golden.json")
    foreach(golden IN LISTS goldens)
        get_filename_component(case_dir "${golden}" DIRECTORY)
        get_filename_component(case "${case_dir}" NAME)
        add_test(NAME executables_${case} COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/run.py"
            "$<TARGET_FILE:erlang_aot>" "${CMAKE_CURRENT_BINARY_DIR}/${case}/$<CONFIG>" "${case_dir}"
            "--suffix=${CMAKE_EXECUTABLE_SUFFIX}")
        set_tests_properties(executables_${case} PROPERTIES TIMEOUT 600)
    endforeach()
    # A deliberately wrong golden must fail with a readable diff; a stale one before linking.
    add_test(NAME executables_selfcheck COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/selfcheck.py"
        "$<TARGET_FILE:erlang_aot>" "${CMAKE_CURRENT_BINARY_DIR}/selfcheck/$<CONFIG>"
        "--suffix=${CMAKE_EXECUTABLE_SUFFIX}")
    set_tests_properties(executables_selfcheck PROPERTIES TIMEOUT 600)
endif()

# Live OTP reproduction of every case golden; opt-in like other audits.
if(ERLANG_AOT_OTP_AUDITS)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_test(NAME executables_oracle COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/regenerate.py"
        --escript "${ERLANG_AOT_ESCRIPT}" --check)
    set_tests_properties(executables_oracle PROPERTIES TIMEOUT 600)
endif()
