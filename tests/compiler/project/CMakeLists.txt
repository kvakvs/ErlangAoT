# Normal project behavior is covered through real CLI processes; see docs/validation.md (test design).
# Keep only deterministic faults and resource limits unavailable through the CLI.
foreach(suite IN ITEMS limits creation_failure)
    add_executable(project_${suite} ${suite}.cpp)
    erlang_aot_project_options(project_${suite})
    target_link_libraries(project_${suite} PRIVATE erlang_project erlang_project_toml)
    add_test(NAME project_${suite} COMMAND project_${suite} "${CMAKE_CURRENT_BINARY_DIR}/project-${suite}")
endforeach()

add_test(NAME project_cli COMMAND "${CMAKE_COMMAND}"
    "-DTOOL=$<TARGET_FILE:erlang_aot>" "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/project-cli"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/cli.cmake")

add_test(NAME project_workflow COMMAND "${CMAKE_COMMAND}"
    "-DTOOL=$<TARGET_FILE:erlang_aot>" "-DFIXTURES=${PROJECT_SOURCE_DIR}/tests/fixtures/project/workflow"
    "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/project-workflow"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/workflow.cmake")

set_tests_properties(project_cli project_workflow PROPERTIES TIMEOUT 90)
