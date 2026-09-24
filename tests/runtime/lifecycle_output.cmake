execute_process(COMMAND "${PROGRAM}" RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE errors)
if(NOT status STREQUAL "0" OR NOT output STREQUAL "" OR NOT errors STREQUAL "")
    message(FATAL_ERROR "Lifecycle must succeed silently: ${status}: ${output}${errors}")
endif()
