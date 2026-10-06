file(REMOVE_RECURSE "${TEST_DIR}")
file(MAKE_DIRECTORY "${TEST_DIR}/project" "${TEST_DIR}/outside")
file(WRITE "${TEST_DIR}/project/shared.erl" "-module(shared). -export([value/0]). value() -> ?VALUE.\n")
file(WRITE "${TEST_DIR}/project/client.erl" "-module(client). -export([value/0]). value() -> shared:value().\n")
file(WRITE "${TEST_DIR}/project/project.toml" [=[schema_version=1
[[targets]]
name="one"
sources=["client.erl", "shared.erl"]
output="reserved"
[targets.options]
defines=["VALUE=1"]
[[targets]]
name="two"
sources=["shared.erl", "client.erl"]
output="reserved"
[targets.options]
defines=["VALUE=2"]
]=])
# Compile selected targets through the same product command, from outside the manifest directory.
function(check code pattern)
    execute_process(COMMAND "${TOOL}" --project ../project/project.toml ${ARGN}
        WORKING_DIRECTORY "${TEST_DIR}/outside" RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err TIMEOUT 30)
    if(NOT "${result}" STREQUAL "${code}" OR NOT out STREQUAL "" OR NOT err MATCHES "${pattern}")
        message(FATAL_ERROR "${ARGN}: exit=${result}, stdout=[${out}], stderr=[${err}]")
    endif()
endfunction()
# TOML executable destinations must not control or collide with module artifact emission.
check(0 "^$" --emit llvm-ir -O2)
set(one "${TEST_DIR}/project/build/aot/eav1_6f6e65__0/eav1_736861726564__0.ll")
set(two "${TEST_DIR}/project/build/aot/eav1_74776f__0/eav1_736861726564__0.ll")
file(READ "${one}" one_ir)
file(READ "${two}" two_ir)
if(NOT one_ir MATCHES "store i64 31" OR NOT two_ir MATCHES "store i64 47")
    message(FATAL_ERROR "Project macro settings leaked between compilation targets")
endif()
check(0 "^$" --target two --emit llvm-bc --artifact-dir explicit)
if(NOT EXISTS "${TEST_DIR}/outside/explicit/eav1_74776f__0/eav1_736861726564__0.bc" OR EXISTS "${TEST_DIR}/outside/explicit/eav1_6f6e65__0")
    message(FATAL_ERROR "Explicit artifact root/target selection is incorrect")
endif()
# Without --emit, targets with an output link; no module exports main/1, so nothing is written.
check(1 "\\[target two\\]: error: no entry point" --target two -O0)
check(1 "\\[target two\\]: error: no entry point" --target two -O2 --no-type-specialization)
if(EXISTS "${TEST_DIR}/outside/build" OR EXISTS "${TEST_DIR}/project/reserved")
    message(FATAL_ERROR "Failed linking wrote executable outputs")
endif()
# A later target failure must prevent earlier successful targets from publishing their new bytes.
file(WRITE "${TEST_DIR}/project/shared.erl" "-module(shared). -export([value/0]).\n-if(?VALUE == 2).\nvalue() -> receive X -> X end.\n-else.\nvalue() -> 99.\n-endif.\n")
check(1 "target two.*notimpl" --emit llvm-ir)
file(READ "${one}" after_one)
file(READ "${two}" after_two)
if(NOT one_ir STREQUAL after_one OR NOT two_ir STREQUAL after_two)
    message(FATAL_ERROR "Failed selected target published another target's new artifacts")
endif()
check(0 "^$" --target one --emit llvm-ir --artifact-dir selected)
file(WRITE "${TEST_DIR}/project/shared.erl" "-module(shared). -export([value/0]). value() -> 1.\n")
file(WRITE "${TEST_DIR}/project/project.toml" [=[schema_version=1
[[targets]]
name="one"
sources=["client.erl"]
[[targets]]
name="two"
sources=["shared.erl"]
]=])
check(1 "target one.*unknown module" --emit obj --artifact-dir isolated)
if(EXISTS "${TEST_DIR}/outside/isolated")
    message(FATAL_ERROR "Failed batch isolation created an artifact root")
endif()
