# Compare reference records using the configured OTP 29+ installation.
if(LIVE AND NOT EXISTS "${ESCRIPT}")
    message(FATAL_ERROR "Configured escript is unavailable: ${ESCRIPT}")
endif()
foreach(stem IN ITEMS minimal expanded phase1)
    file(READ "${FIXTURES}/${stem}.phase1" expected)
    execute_process(COMMAND "${DUMP}" "${FIXTURES}/${stem}.erl"
        RESULT_VARIABLE result OUTPUT_VARIABLE actual ERROR_VARIABLE errors)
    if(NOT result STREQUAL "0" OR NOT actual STREQUAL expected)
        message(FATAL_ERROR "Native Phase I AST mismatch: ${stem}: ${errors}\n${actual}")
    endif()
    if(LIVE)
        execute_process(COMMAND "${ESCRIPT}" "${HARNESS}" phase1 "${FIXTURES}/${stem}.erl"
            RESULT_VARIABLE result OUTPUT_VARIABLE actual ERROR_VARIABLE errors TIMEOUT 10)
        if(NOT result STREQUAL "0" OR NOT actual STREQUAL expected)
            message(FATAL_ERROR "OTP Phase I AST mismatch: ${stem}: ${errors}\n${actual}")
        endif()
    endif()
endforeach()
