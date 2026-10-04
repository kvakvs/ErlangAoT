# Executable linking for positional inputs and a single project target (docs/executables.md#linking): -o drives Clang with the
# module and startup objects plus the runtime archive; failures publish nothing and keep existing outputs.
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
file(COPY "${SOURCE_ROOT}/tests/fixtures/linking/startup/" DESTINATION "${WORK}")
set(example "${SOURCE_ROOT}/examples/compile")

# Run the compiler in the work directory and require an exit status plus stderr pattern.
function(compile name code stderr)
    execute_process(COMMAND "${TOOL}" ${ARGN} WORKING_DIRECTORY "${WORK}"
        RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err TIMEOUT 120 ENCODING UTF-8)
    if(NOT result STREQUAL "${code}" OR NOT out STREQUAL "" OR NOT err MATCHES "${stderr}")
        message(FATAL_ERROR "${name}: exit=${result} stdout=[${out}] stderr=[${err}]")
    endif()
    file(GLOB staging "${WORK}/.erlangaot-link*")
    if(staging)
        message(FATAL_ERROR "${name}: link staging left behind: ${staging}")
    endif()
endfunction()

# Run a linked program and require its exit status and exact stdout.
function(run program code stdout)
    execute_process(COMMAND "${WORK}/${program}${HOST_SUFFIX}" ${ARGN}
        RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err TIMEOUT 30)
    string(REPLACE "\r\n" "\n" out "${out}")
    if(NOT result STREQUAL "${code}" OR NOT out STREQUAL "${stdout}")
        message(FATAL_ERROR "${program} ${ARGN}: exit=${result} stdout=[${out}] stderr=[${err}]")
    endif()
endfunction()

# The documented two-module example at both levels; an existing output is replaced by the new program.
file(WRITE "${WORK}/demo-O0${HOST_SUFFIX}" "old\n")
foreach(level IN ITEMS O0 O2)
    compile(demo-${level} 0 "^$" -${level} -o demo-${level} "${example}/answer.erl" "${example}/client.erl")
    run(demo-${level} 0 "42\n-7\n{record,map,binary,list,integer,other}\n")
endforeach()
# Arguments and exit statuses reach the linked startup; escripts keep exit 127.
file(MAKE_DIRECTORY "${WORK}/sub dir")
compile(app 0 "^$" -O2 -o "sub dir/app" app.erl helper.erl)
run("sub dir/app" 0 "[\"a b\"]\n" "a b")
run("sub dir/app" 3 "" halt)
compile(escript 0 "^$" -o boom boom.escript)
run(boom 127 "")
# A project target links with an explicit -o (exactly one selected target); manifest and CLI entries apply.
compile(project 0 "^$" --project project.toml -o project)
run(project 0 "[\"x\"]\n" x)
compile(project_entry 0 "^$" --project project.toml --target app --entry app -O2 -o "sub dir/project")
run("sub dir/project" 3 "" halt)
compile(project_failure 1 "\\[target app\\]: error: runtime library not found"
    --project project.toml -o kept.bin --runtime-library absent.lib)

# Every failure keeps an existing output byte-for-byte and leaves no staging directory.
set(kept "${WORK}/kept.bin")
file(WRITE "${kept}" "old\n")
function(failure name stderr)
    compile(${name} 1 "${stderr}" ${ARGN} app.erl helper.erl)
    file(READ "${WORK}/kept.bin" contents)
    if(NOT contents STREQUAL "old\n")
        message(FATAL_ERROR "${name}: existing output was modified")
    endif()
endfunction()
failure(absent_runtime "^error: runtime library not found: [^\n]*absent\\.lib; build the erlang_runtime target"
    --runtime-library absent.lib -o kept.bin)
failure(not_archive "^error: runtime library is not a static library: [^\n]*app\\.erl"
    --runtime-library app.erl -o kept.bin)
failure(wrong_target "^error: runtime library [^\n]* contains [^\n]* objects, but the executable targets armv7-unknown-linux-gnueabihf"
    --target-triple armv7-unknown-linux-gnueabihf -o kept.bin)
failure(absent_linker "^error: linker not found: [^\n]*no-such-clang" --linker no-such-clang -o kept.bin)
failure(link_error "^error: linking [^\n]*kept\\.bin failed: [^\n]* exited with status [1-9][0-9]*:\n.*[Uu]ndefined"
    --runtime-library "${OTHER_LIBRARY}" -o kept.bin)
failure(parent_is_file "^error: output directory does not exist: [^\n]*kept\\.bin" -o kept.bin/app)
file(MAKE_DIRECTORY "${WORK}/folder.d")
failure(output_is_directory "^error: artifact destination is not a regular file: [^\n]*folder\\.d" -o folder.d)
failure(output_is_input "^error: artifact destination aliases an input: [^\n]*app\\.erl" -o app.erl)
file(READ "${WORK}/app.erl" source)
file(READ "${SOURCE_ROOT}/tests/fixtures/linking/startup/app.erl" original)
if(NOT source STREQUAL original)
    message(FATAL_ERROR "output_is_input: input was modified")
endif()
compile(usage 2 "--linker and --runtime-library require --output" --linker clang app.erl)
