# Shared scope selection for check-complexity and check-clang-tidy scripts.
if(NOT DEFINED QUALITY_SCOPE)
    set(QUALITY_SCOPE all)
endif()
if(NOT QUALITY_SCOPE MATCHES "^(all|changed)$")
    message(FATAL_ERROR "QUALITY_SCOPE must be all or changed, not '${QUALITY_SCOPE}'.")
endif()
if(NOT DEFINED QUALITY_PYTHON)
    if(CMAKE_HOST_WIN32)
        set(QUALITY_PYTHON "${project_root}/.venv-quality/Scripts/python.exe")
    else()
        set(QUALITY_PYTHON "${project_root}/.venv-quality/bin/python")
    endif()
endif()

# Store the changed files of one check kind (tidy or lizard) in output, relative to the project root.
function(erlang_aot_quality_scope kind output)
    set(base HEAD)
    if(DEFINED ENV{ERLANG_AOT_QUALITY_BASE})
        set(base "$ENV{ERLANG_AOT_QUALITY_BASE}")
    endif()
    set(list_file "${QUALITY_BUILD_DIR}/quality/${kind}-scope.txt")
    file(MAKE_DIRECTORY "${QUALITY_BUILD_DIR}/quality")
    execute_process(
        COMMAND "${QUALITY_PYTHON}" "${project_root}/cmake/quality_scope.py" --kind ${kind}
            --root "${project_root}" --build "${QUALITY_BUILD_DIR}" "--ninja=${QUALITY_NINJA}"
            --base "${base}" --output "${list_file}"
        RESULT_VARIABLE scope_result
    )
    if(NOT scope_result STREQUAL "0")
        message(FATAL_ERROR "Quality scope selection failed; use the *-all quality targets to check everything.")
    endif()
    file(STRINGS "${list_file}" files)
    set(${output} "${files}" PARENT_SCOPE)
endfunction()
