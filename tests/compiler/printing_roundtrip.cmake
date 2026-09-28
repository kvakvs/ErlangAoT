# Compare decoded expanded tokens before and after the product's source printer.
file(MAKE_DIRECTORY "${WORK}")
file(GLOB inputs "${FIXTURES}/*.erl")
foreach(input IN LISTS inputs)
    get_filename_component(name "${input}" NAME)
    file(COPY_FILE "${input}" "${WORK}/${name}")
    execute_process(COMMAND "${DUMP}" "./${name}" WORKING_DIRECTORY "${WORK}"
        RESULT_VARIABLE before_status OUTPUT_VARIABLE before ERROR_VARIABLE before_error TIMEOUT 10 ENCODING UTF-8)
    execute_process(COMMAND "${TOOL}" --print-pp "./${name}" WORKING_DIRECTORY "${WORK}"
        RESULT_VARIABLE status OUTPUT_VARIABLE printed ERROR_VARIABLE error TIMEOUT 10 ENCODING UTF-8)
    if(NOT status STREQUAL "0" OR NOT before_status STREQUAL "0" OR NOT error STREQUAL "" OR NOT before_error STREQUAL "")
        message(FATAL_ERROR "Source printing failed: ${name}: ${error}${before_error}")
    endif()
    file(WRITE "${WORK}/printed.erl" "${printed}")
    execute_process(COMMAND "${DUMP}" ./printed.erl WORKING_DIRECTORY "${WORK}"
        RESULT_VARIABLE after_status OUTPUT_VARIABLE after ERROR_VARIABLE after_error TIMEOUT 10 ENCODING UTF-8)
    if(NOT after_status STREQUAL "0" OR NOT after_error STREQUAL "" OR NOT before STREQUAL after)
        message(FATAL_ERROR "Source printing changed decoded tokens: ${name}: ${after_error}")
    endif()
endforeach()
