# Explicit live audits use host Erlang; normal builds/tests consume retained project fixtures.
set(erlang_hints)
if(APPLE)
    find_program(CLAUSE_BREW_EXECUTABLE brew HINTS /opt/homebrew/bin /usr/local/bin)
    if(CLAUSE_BREW_EXECUTABLE)
        execute_process(COMMAND "${CLAUSE_BREW_EXECUTABLE}" --prefix erlang
            OUTPUT_VARIABLE erlang_prefix OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE erlang_brew_result ERROR_QUIET TIMEOUT 10)
        if(erlang_brew_result STREQUAL "0")
            list(APPEND erlang_hints "${erlang_prefix}/bin")
        endif()
    endif()
endif()
find_program(CLAUSE_ESCRIPT escript HINTS ${erlang_hints}
    DOC "Host Erlang/OTP 29 or newer escript for optional live audits" REQUIRED)
execute_process(COMMAND "${CLAUSE_ESCRIPT}" "${CMAKE_CURRENT_LIST_DIR}/ErlangVersion.escript"
    OUTPUT_VARIABLE erlang_version_output OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_VARIABLE erlang_version_error RESULT_VARIABLE erlang_version_result TIMEOUT 15)
string(REPLACE "\r\n" "\n" erlang_version_output "${erlang_version_output}")
if(NOT erlang_version_result STREQUAL "0" OR NOT erlang_version_output MATCHES "^([0-9]+)\n([^\n]+)$")
    message(FATAL_ERROR "Cannot determine Erlang/OTP version using ${CLAUSE_ESCRIPT}: ${erlang_version_result}\n${erlang_version_error}\nSet CLAUSE_ESCRIPT to a working OTP 29+ escript.")
endif()
set(CLAUSE_OTP_RELEASE "${CMAKE_MATCH_1}")
set(CLAUSE_OTP_VERSION "${CMAKE_MATCH_2}")
if(CLAUSE_OTP_RELEASE LESS 29)
    message(FATAL_ERROR "Erlang/OTP 29 or newer is required; found ${CLAUSE_OTP_VERSION} using ${CLAUSE_ESCRIPT}. Set CLAUSE_ESCRIPT to an OTP 29+ installation, or clear its cached value to rediscover it.")
endif()
message(STATUS "Erlang/OTP ${CLAUSE_OTP_VERSION}: ${CLAUSE_ESCRIPT}")
