if(NOT EXISTS "${OTP_ROOT}/lib/stdlib/src/erl_parse.yrl")
    message(STATUS "SKIP: pinned OTP source checkout unavailable; real-source corpus remains pending")
    return()
endif()
include("${CMAKE_CURRENT_LIST_DIR}/pinned.cmake")
set(options -I "${OTP_ROOT}/lib/compiler/src"
    --app-dir "stdlib=${OTP_ROOT}/lib/stdlib" --app-dir "kernel=${OTP_ROOT}/lib/kernel"
    --enable-feature maybe_expr --disable-feature compr_assign
    "-DCOMPILER_VSN=\"parser-compatibility\"")
file(MAKE_DIRECTORY "${WORK}")
file(WRITE "${WORK}/corpus.tsv" "path\tpreprocessing\tparsing\tast_sha256\n")
foreach(row IN LISTS corpus)
    string(REPLACE "\t" ";" columns "${row}")
    list(GET columns 1 path)
    set(input "${OTP_ROOT}/${path}")
    execute_process(COMMAND "${TOOL}" --preprocess-check ${options} "${input}"
        RESULT_VARIABLE pp OUTPUT_VARIABLE output ERROR_VARIABLE errors TIMEOUT 30)
    if(NOT pp STREQUAL "0" OR NOT output STREQUAL "" OR NOT errors STREQUAL "")
        message(FATAL_ERROR "OTP preprocessing failed: ${path}: ${errors}")
    endif()
    execute_process(COMMAND "${TOOL}" --print-ast ${options} "${input}"
        RESULT_VARIABLE parsed OUTPUT_VARIABLE tree ERROR_VARIABLE errors TIMEOUT 30)
    if(NOT parsed STREQUAL "0" OR NOT errors STREQUAL "")
        message(FATAL_ERROR "OTP parsing failed after preprocessing succeeded: ${path}: ${errors}")
    endif()
    execute_process(COMMAND "${TOOL}" --print-ast ${options} "${input}"
        RESULT_VARIABLE repeated OUTPUT_VARIABLE again ERROR_VARIABLE errors TIMEOUT 30)
    if(NOT repeated STREQUAL "0" OR NOT tree STREQUAL again OR NOT errors STREQUAL "")
        message(FATAL_ERROR "Nondeterministic OTP AST: ${path}: ${errors}")
    endif()
    string(SHA256 digest "${tree}")
    file(APPEND "${WORK}/corpus.tsv" "${path}\tpassed\tpassed\t${digest}\n")
    message(STATUS "OTP preprocessing/parsing/deterministic AST passed: ${path}")
endforeach()
