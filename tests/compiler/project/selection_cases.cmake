# Observe target/file order and independent preprocessing through emitted ASTs.
file(WRITE "${TEST_DIR}/shared.erl" "-module(shared). value() -> ?VALUE.\n")
file(WRITE "${TEST_DIR}/later.erl" "-module(later). value() -> ?VALUE.\n")
file(WRITE "${TEST_DIR}/execution.toml" [=[schema_version=1
[[targets]]
name='app'
sources=['shared.erl','later.erl']
[targets.options]
defines=['VALUE=1']
[[targets]]
name='tests'
sources=['shared.erl']
[targets.options]
defines=['VALUE=2']
]=])
check(execution_order 0 "name=shared.*value=1.*name=later.*value=1.*name=shared.*value=2" "^$"
    --project execution.toml --print-ast)
file(WRITE "${TEST_DIR}/later.erl" "-error(later_file_failure).\n")
check(execution_failure 1 "value=1.*value=2" "target app.*later.erl.*later_file_failure"
    --project execution.toml --print-ast)
check(execution_default_failure 1 "^$" "target app.*later.erl.*later_file_failure"
    --project execution.toml)
check(execution_check_failure 1 "^$" "target app.*later.erl.*later_file_failure"
    --project execution.toml --parse-check)

# Default output validation detects lexical aliases before processing any source.
file(WRITE "${TEST_DIR}/collision.toml" [=[schema_version=1
[[targets]]
name='app'
sources=['shared.erl']
output='out/../same'
[[targets]]
name='tests'
sources=['shared.erl']
output='same'
]=])
check(output_collision 1 "^$" "output" --project collision.toml)
check(frontend_ignores_outputs 0 "value=7.*value=7" "^$"
    --project collision.toml --print-ast -DVALUE=7)
check(single_output_override 1 "^$" "no entry point: no module exports main/1" --project collision.toml --target app --target app -o requested -DVALUE=7)
if(EXISTS "${TEST_DIR}/out" OR EXISTS "${TEST_DIR}/requested" OR EXISTS "${TEST_DIR}/same")
    message(FATAL_ERROR "Planning created output destinations")
endif()

# Generated templates have deterministic bytes and remain usable without options.
check(template_second 0 "Created" "^$" --new-project second)
file(READ "${TEST_DIR}/second.toml" second)
if(NOT second STREQUAL starter OR NOT second MATCHES "\n$")
    message(FATAL_ERROR "Starter template is nondeterministic or lacks its final newline")
endif()
foreach(annotation IN ITEMS "--parse-check" "--target" "Erlang literal terms" "CLI -I" "include_lib" "Create/populate src")
    string(FIND "${second}" "${annotation}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "Starter template lost guidance: ${annotation}")
    endif()
endforeach()
set(expected_output "build/app")
if(WIN32)
    string(APPEND expected_output ".exe")
endif()
string(FIND "${second}" "output = \"${expected_output}\"" position)
if(position EQUAL -1)
    message(FATAL_ERROR "Starter template has the wrong native output path")
endif()
check(template_defaults 0 "ModuleAttribute name=main" "^$" --project second --target app --print-ast)
check(new_upper_suffix 0 "Created.*upper.TOML" "^$" --new-project upper.TOML)
check(new_dot_name 0 "Created.*.toml" "^$" --new-project .toml)
check(new_other_suffix 0 "Created.*app.config.toml" "^$" --new-project app.config)
check(new_parent_missing 1 "^$" "cannot create manifest exclusively" --new-project missing/app)
file(MAKE_DIRECTORY "${TEST_DIR}/directory.toml")
check(new_directory 1 "^$" "cannot create manifest exclusively" --new-project directory)
foreach(name IN ITEMS ".." "dir/")
    check(new_invalid_path 2 "^$" "requires a filename" --new-project "${name}")
endforeach()
if(EXISTS "${TEST_DIR}/missing" OR EXISTS "${TEST_DIR}/upper.TOML.toml")
    message(FATAL_ERROR "Project creation changed path semantics")
endif()
