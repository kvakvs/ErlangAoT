# Only a verified local checkout may supply the optional real-source/coverage tests.
find_package(Git REQUIRED)
execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${OTP_ROOT}" rev-parse HEAD
    OUTPUT_VARIABLE revision OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE status)
if(NOT status STREQUAL "0" OR NOT revision STREQUAL "751f87b703fe5948607d08e82599ce644b772e76")
    message(FATAL_ERROR "Parser corpus requires the pinned OTP 29.1 checkout")
endif()
execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${OTP_ROOT}" diff --quiet HEAD --
    lib/stdlib/src lib/compiler/src lib/stdlib/include lib/kernel/include
    RESULT_VARIABLE status)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Pinned OTP grammar/corpus/dependencies have local changes")
endif()
file(STRINGS "${FIXTURES}/phase6/otp.tsv" corpus)
list(REMOVE_AT corpus 0)
foreach(row IN LISTS corpus)
    string(REPLACE "\t" ";" columns "${row}")
    list(GET columns 0 expected)
    list(GET columns 1 path)
    file(SHA256 "${OTP_ROOT}/${path}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Pinned corpus checksum changed: ${path}")
    endif()
endforeach()
