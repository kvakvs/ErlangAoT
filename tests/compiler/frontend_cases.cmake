# Run source fixtures through the product CLI, comparing complete ASTs and diagnostics.
file(GLOB cases "${FIXTURES}/*.erl")
foreach(input IN LISTS cases)
    get_filename_component(name "${input}" NAME)
    file(READ "${input}.out" expected)
    file(READ "${input}.err" expected_error)
    file(READ "${input}.status" status)
    string(STRIP "${status}" status)
    set(arguments --print-ast)
    if(EXISTS "${input}.args")
        file(STRINGS "${input}.args" arguments)
    endif()
    execute_process(COMMAND "${TOOL}" ${arguments} "${name}" WORKING_DIRECTORY "${FIXTURES}"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 15 ENCODING UTF-8)
    if(NOT result STREQUAL status OR NOT output STREQUAL expected OR NOT error STREQUAL expected_error)
        file(MAKE_DIRECTORY "${WORK}")
        file(WRITE "${WORK}/${name}.actual" "${output}")
        file(WRITE "${WORK}/${name}.error" "${error}")
        message(FATAL_ERROR "CLI fixture ${name}: expected exit ${status}, got ${result}; see ${WORK}")
    endif()
endforeach()

# Bound deep output including aligned closing lines; exact depth/tail catch truncated success.
file(MAKE_DIRECTORY "${WORK}")
string(REPEAT "+1" 9000 chain)
file(WRITE "${WORK}/deep.erl" "f() -> 0${chain}.")
execute_process(COMMAND "${TOOL}" --print-ast deep.erl WORKING_DIRECTORY "${WORK}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 15)
string(LENGTH "${output}" length)
if(NOT result STREQUAL "0" OR NOT error STREQUAL "" OR NOT output MATCHES "\\[depth=9000\\]"
    OR NOT output MATCHES "right=\\(IntegerLiteral value=1\\)\n      \\)\n    \\)\n  \\)\n\\)\n$"
    OR length GREATER_EQUAL 5000000)
    message(FATAL_ERROR "Deep CLI AST printing failed: ${result}: ${error}")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/semantic/cases.cmake")
