# Native compiler tests use the installed host Erlang, not the reference checkout.
set(erlang_hints)
if(APPLE)
    find_program(ERLANG_AOT_BREW_EXECUTABLE brew HINTS /opt/homebrew/bin /usr/local/bin)
    if(ERLANG_AOT_BREW_EXECUTABLE)
        execute_process(COMMAND "${ERLANG_AOT_BREW_EXECUTABLE}" --prefix erlang
            OUTPUT_VARIABLE erlang_prefix OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE erlang_brew_result ERROR_QUIET TIMEOUT 10)
        if(erlang_brew_result STREQUAL "0")
            list(APPEND erlang_hints "${erlang_prefix}/bin")
        endif()
    endif()
endif()
find_program(ERLANG_AOT_ESCRIPT escript HINTS ${erlang_hints}
    DOC "Host Erlang/OTP 29 or newer escript for compiler tests" REQUIRED)
execute_process(COMMAND "${ERLANG_AOT_ESCRIPT}" "${CMAKE_CURRENT_LIST_DIR}/ErlangVersion.escript"
    OUTPUT_VARIABLE erlang_version_output OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_VARIABLE erlang_version_error RESULT_VARIABLE erlang_version_result TIMEOUT 15)
string(REPLACE "\r\n" "\n" erlang_version_output "${erlang_version_output}")
if(NOT erlang_version_result STREQUAL "0" OR NOT erlang_version_output MATCHES "^([0-9]+)\n([^\n]+)$")
    message(FATAL_ERROR "Cannot determine Erlang/OTP version using ${ERLANG_AOT_ESCRIPT}: ${erlang_version_result}\n${erlang_version_error}\nSet ERLANG_AOT_ESCRIPT to a working OTP 29+ escript.")
endif()
set(ERLANG_AOT_OTP_RELEASE "${CMAKE_MATCH_1}")
set(ERLANG_AOT_OTP_VERSION "${CMAKE_MATCH_2}")
if(ERLANG_AOT_OTP_RELEASE LESS 29)
    message(FATAL_ERROR "Erlang/OTP 29 or newer is required; found ${ERLANG_AOT_OTP_VERSION} using ${ERLANG_AOT_ESCRIPT}. Set ERLANG_AOT_ESCRIPT to an OTP 29+ installation, or clear its cached value to rediscover it.")
endif()
message(STATUS "Erlang/OTP ${ERLANG_AOT_OTP_VERSION}: ${ERLANG_AOT_ESCRIPT}")
