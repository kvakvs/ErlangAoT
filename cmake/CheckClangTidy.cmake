# Analyze project translation units using the configured compiler flags.
get_filename_component(project_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
if(NOT DEFINED QUALITY_BUILD_DIR)
    set(QUALITY_BUILD_DIR "${project_root}/build/debug")
endif()
set(database "${QUALITY_BUILD_DIR}/compile_commands.json")
if(NOT EXISTS "${database}")
    message(FATAL_ERROR "Missing ${database}. Configure a Makefiles or Ninja build first.")
endif()
find_program(CLANG_TIDY_EXECUTABLE NAMES clang-tidy
    HINTS "${project_root}/.venv-quality/bin" "${project_root}/.venv-quality/Scripts"
    REQUIRED
)
include("${QUALITY_BUILD_DIR}/QualityToolchain.cmake")
set(toolchain_args)
foreach(directory IN LISTS quality_implicit_includes)
    list(APPEND toolchain_args "--extra-arg=-isystem${directory}")
endforeach()
if(quality_apple_sysroot)
    list(APPEND toolchain_args --extra-arg=-isysroot "--extra-arg=${quality_apple_sysroot}")
endif()

file(READ "${database}" commands)
string(JSON command_count LENGTH "${commands}")
if(command_count EQUAL 0)
    message(FATAL_ERROR "The compilation database is empty.")
endif()
math(EXPR last_command "${command_count} - 1")
set(sources)
foreach(index RANGE ${last_command})
    string(JSON source GET "${commands}" ${index} file)
    string(JSON directory GET "${commands}" ${index} directory)
    get_filename_component(source "${source}" ABSOLUTE BASE_DIR "${directory}")
    file(RELATIVE_PATH relative "${project_root}" "${source}")
    if(relative MATCHES "^(compiler|runtime|abi)/")
        list(APPEND sources "${source}")
    endif()
endforeach()
list(REMOVE_DUPLICATES sources)
if(NOT sources)
    message(FATAL_ERROR "No project translation units found in ${database}.")
endif()

# Run all selected files and preserve failures instead of silently skipping them.
set(failed FALSE)
foreach(source IN LISTS sources)
    execute_process(
        COMMAND "${CLANG_TIDY_EXECUTABLE}" "-p=${QUALITY_BUILD_DIR}"
            "--config-file=${project_root}/.clang-tidy" ${toolchain_args} "${source}"
        WORKING_DIRECTORY "${project_root}"
        RESULT_VARIABLE tidy_result
    )
    if(NOT tidy_result STREQUAL "0")
        set(failed TRUE)
    endif()
endforeach()
if(failed)
    message(FATAL_ERROR "clang-tidy failed. Fix the diagnostics above before committing.")
endif()
