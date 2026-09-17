file(MAKE_DIRECTORY "${TEST_DIR}")
file(WRITE "${TEST_DIR}/source with spaces.erl" "-module(example).\n")
file(WRITE "${TEST_DIR}/-source.erl" "-module(example).\n")

function(check_cli name expected_exit expected_stdout expected_stderr)
    execute_process(COMMAND "${TOOL}" ${ARGN}
        WORKING_DIRECTORY "${TEST_DIR}"
        RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err
    )
    if(NOT "${result}" STREQUAL "${expected_exit}")
        message(FATAL_ERROR "${name}: expected exit ${expected_exit}, got ${result}: ${err}")
    endif()
    if(NOT out MATCHES "${expected_stdout}" OR NOT err MATCHES "${expected_stderr}")
        message(FATAL_ERROR "${name}: unexpected stdout=[${out}] stderr=[${err}]")
    endif()
endfunction()

check_cli(help 0 "Usage: erlangaot" "^$" --help)
check_cli(short_help 0 "Usage: erlangaot" "^$" -h)
check_cli(version 0 "^erlangaot [0-9]+\\.[0-9]+\\.[0-9]+\n$" "^$" --version)
check_cli(no_inputs 2 "^$" "no input files")
check_cli(unknown_option 2 "^$" "unknown option" --unknown)
check_cli(missing_output 2 "^$" "expected a path after" -o)
check_cli(duplicate_output 2 "^$" "more than once" -o first -o second source.erl)
check_cli(missing_input 1 "^$" "cannot access|not a regular file" missing.erl)
check_cli(directory_input 1 "^$" "not a regular file" .)
check_cli(unimplemented 1 "^$" "compilation is not implemented" "source with spaces.erl")
check_cli(end_of_options 1 "^$" "compilation is not implemented" -- -source.erl)
check_cli(multiple_inputs 1 "^$" "compilation is not implemented"
    "source with spaces.erl" ./-source.erl)

set(output "${TEST_DIR}/output file")
file(REMOVE "${output}")
check_cli(no_output_created 1 "^$" "compilation is not implemented"
    --output "${output}" "source with spaces.erl")
if(EXISTS "${output}")
    message(FATAL_ERROR "Unimplemented compilation created an output file")
endif()
file(WRITE "${output}" "preserve existing output\n")
check_cli(no_output_overwritten 1 "^$" "compilation is not implemented"
    -o "${output}" "source with spaces.erl")
file(READ "${output}" contents)
if(NOT contents STREQUAL "preserve existing output\n")
    message(FATAL_ERROR "Unimplemented compilation modified an existing output file")
endif()
