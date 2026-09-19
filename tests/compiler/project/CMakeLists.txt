add_executable(project_dependency dependency.cpp)
erlang_aot_project_options(project_dependency)
target_link_libraries(project_dependency PRIVATE erlang_project_toml)
add_test(NAME project_dependency COMMAND project_dependency)

add_executable(project_model model.cpp)
erlang_aot_project_options(project_model)
target_link_libraries(project_model PRIVATE erlang_project)
add_test(NAME project_model COMMAND project_model)

add_executable(project_loader loader.cpp)
erlang_aot_project_options(project_loader)
target_link_libraries(project_loader PRIVATE erlang_project erlang_project_toml)
add_test(NAME project_loader COMMAND project_loader "${CMAKE_CURRENT_BINARY_DIR}/project-loader")

add_executable(project_decode decode.cpp)
erlang_aot_project_options(project_decode)
target_link_libraries(project_decode PRIVATE erlang_project erlang_project_toml)
add_test(NAME project_decode COMMAND project_decode)

add_executable(project_decode_options decode_options.cpp)
erlang_aot_project_options(project_decode_options)
target_link_libraries(project_decode_options PRIVATE erlang_project erlang_project_toml)
add_test(NAME project_decode_options COMMAND project_decode_options)

add_executable(project_paths paths.cpp)
erlang_aot_project_options(project_paths)
target_link_libraries(project_paths PRIVATE erlang_project)
add_test(NAME project_paths COMMAND project_paths "${CMAKE_CURRENT_BINARY_DIR}/project-paths")

add_executable(project_glob glob.cpp)
erlang_aot_project_options(project_glob)
target_link_libraries(project_glob PRIVATE erlang_project)
add_test(NAME project_glob COMMAND project_glob)

add_executable(project_discovery discovery.cpp)
erlang_aot_project_options(project_discovery)
target_link_libraries(project_discovery PRIVATE erlang_project)
add_test(NAME project_discovery COMMAND project_discovery "${CMAKE_CURRENT_BINARY_DIR}/project-discovery")

add_executable(project_sources sources.cpp)
erlang_aot_project_options(project_sources)
target_link_libraries(project_sources PRIVATE erlang_project)
add_test(NAME project_sources COMMAND project_sources "${CMAKE_CURRENT_BINARY_DIR}/project-sources")

add_executable(project_selection selection.cpp)
erlang_aot_project_options(project_selection)
target_link_libraries(project_selection PRIVATE erlang_project)
add_test(NAME project_selection COMMAND project_selection)

add_executable(project_options options.cpp)
erlang_aot_project_options(project_options)
target_link_libraries(project_options PRIVATE erlang_project)
add_test(NAME project_options COMMAND project_options)

add_executable(project_plan plan.cpp)
erlang_aot_project_options(project_plan)
target_link_libraries(project_plan PRIVATE erlang_project)
add_test(NAME project_plan COMMAND project_plan "${CMAKE_CURRENT_BINARY_DIR}/project-plan")

add_executable(project_execution execution.cpp)
erlang_aot_project_options(project_execution)
target_link_libraries(project_execution PRIVATE erlang_project)
add_test(NAME project_execution COMMAND project_execution)

add_test(NAME project_cli COMMAND "${CMAKE_COMMAND}"
    "-DTOOL=$<TARGET_FILE:erlang_aot>" "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/project-cli"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/cli.cmake")
