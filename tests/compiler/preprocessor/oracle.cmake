# Compare reference records using the configured OTP 29+ installation.
if(NOT EXISTS "${ESCRIPT}")
    message(FATAL_ERROR "Configured escript is unavailable: ${ESCRIPT}")
endif()
file(MAKE_DIRECTORY "${OUTPUT_DIR}")
foreach(fixture IN ITEMS good bad)
    execute_process(COMMAND "${ESCRIPT}" "${HARNESS}"
        "${FIXTURES}/${fixture}.erl" "${OUTPUT_DIR}/${fixture}.terms"
        RESULT_VARIABLE result)
    if(NOT result STREQUAL "0")
        message(FATAL_ERROR "OTP oracle failed: ${result}")
    endif()
endforeach()
file(READ "${OUTPUT_DIR}/good.terms" good)
file(READ "${OUTPUT_DIR}/bad.terms" bad)
if(NOT good MATCHES "integer,.*42" OR NOT bad MATCHES "undefined,.*MISSING")
    message(FATAL_ERROR "Oracle fixtures did not produce the expected events")
endif()
