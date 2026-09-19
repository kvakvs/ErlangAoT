# Compare reference records using the configured OTP 29+ installation.
if(NOT EXISTS "${ESCRIPT}")
    message(FATAL_ERROR "Configured escript is unavailable: ${ESCRIPT}")
endif()
file(GLOB fixtures "${FIXTURES}/lexical/*.erl")
foreach(fixture IN LISTS fixtures)
    execute_process(COMMAND "${ESCRIPT}" "${HARNESS}" "${fixture}"
        RESULT_VARIABLE expected_result OUTPUT_VARIABLE expected ERROR_VARIABLE oracle_error)
    execute_process(COMMAND "${DUMP}" "${fixture}"
        RESULT_VARIABLE actual_result OUTPUT_VARIABLE actual ERROR_VARIABLE actual_error)
    if(NOT expected_result STREQUAL "0" OR NOT actual_result STREQUAL "0")
        message(FATAL_ERROR "Scanner failed for ${fixture}: OTP=${oracle_error}, C++=${actual_error}")
    endif()
    if(NOT actual STREQUAL expected)
        file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/scanner-expected.txt" "${expected}")
        file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/scanner-actual.txt" "${actual}")
        message(FATAL_ERROR "Scanner mismatch for ${fixture}; see scanner-{expected,actual}.txt")
    endif()
endforeach()
