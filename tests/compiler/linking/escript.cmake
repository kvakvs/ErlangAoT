# Escript compile mode (docs/executables.md#escripts): a "#!" first line selects escript rules,
# checked through the public CLI. Expected acceptance was compared once with OTP 29 escript.
file(REMOVE_RECURSE "${WORK}")
file(COPY "${FIXTURES}/escript/" "${FIXTURES}/entry/app.erl" DESTINATION "${WORK}")
file(WRITE "${WORK}/crlf" "#!/usr/bin/env escript\r\n%% CRLF header\r\nmain(_Args) -> ok.\r\n")
file(WRITE "${WORK}/late_error" "#!/usr/bin/env escript\n\n\n\nmain(_Args) -> undefined_function().\n")

# Run the compiler and require exit status, stdout and stderr patterns; no executable may appear.
function(check name code stdout stderr)
    execute_process(COMMAND "${TOOL}" ${ARGN} WORKING_DIRECTORY "${WORK}"
        RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err TIMEOUT 30 ENCODING UTF-8)
    if(NOT "${result}" STREQUAL "${code}" OR NOT out MATCHES "${stdout}" OR NOT err MATCHES "${stderr}")
        message(FATAL_ERROR "${name}: exit=${result} stdout=[${out}] stderr=[${err}]")
    endif()
    if(EXISTS "${WORK}/out" OR EXISTS "${WORK}/out.exe")
        message(FATAL_ERROR "${name}: created an executable output")
    endif()
endfunction()

set(ignored "warning: greet:3:1: escript emulator arguments \\(%%!\\) are ignored by compiled executables\n")
# Executable requests stop at an absent runtime library, checked only after the entry resolved.
set(linking "error: runtime library not found")
set(absent --runtime-library absent.lib)

# Header handling: synthesized module, implicit export, -mode accepted, line numbers kept.
check(synthesized_module 0 "- module \\( greet__escript \\) \\.\n- mode \\( compile \\) \\.\nmain \\( _Args \\) -> greet__escript \\." "^${ignored}$"
    --print-pp greet)
check(parse_check 0 "^$" "^${ignored}$" --parse-check greet)
check(compile_in_memory 0 "^$" "^${ignored}$" greet)
check(emit_ir 0 "^$" "^${ignored}$" --emit llvm-ir --artifact-dir ir greet)
check(explicit_module 0 "- module \\( named \\) \\." "^$" --print-pp named.escript)
check(crlf_header 0 "^$" "^$" crlf)
check(line_numbers 1 "^$" "late_error:5:34: undefined function late_error__escript:undefined_function/0" late_error)

# Entries: the escript's main/1 is the default and is preferred over ordinary main/1 exporters.
check(escript_entry 1 "^$" "^${ignored}${linking}" -o out ${absent} greet)
check(named_entry 1 "^$" "^${linking}" -o out ${absent} named.escript)
check(preferred_over_module 1 "^$" "${linking}" -o out ${absent} greet app.erl)
check(explicit_module_entry 1 "^$" "${linking}" -o out ${absent} --entry app greet app.erl)
check(ambiguous_escripts 1 "^$" "ambiguous entry point: main/1 is exported by greet__escript, named" -o out greet named.escript)
check(only_main_exported 1 "^$" "named.escript:7:1: entry function named:helper/1 is not defined; found helper/0"
    --entry named:helper named.escript)
check(plain_module 1 "^$" "no entry point: no module exports main/1" -o out plain.erl)

# Escript rules that OTP also enforces.
check(missing_main 1 "^$" "^error: nomain:1:1: escript does not define main/1\n$" nomain)
check(illegal_mode 1 "^$" "^error: badmode:3:1: illegal escript mode attribute" badmode)
file(WRITE "${WORK}/mode.erl" "-module(mode).\n-mode(compile).\n")
check(mode_outside_escript 1 "^$" "mode.erl:2:1: \\[behavior-changing attributes\\] notimpl" mode.erl)
