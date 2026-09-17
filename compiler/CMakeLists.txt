add_executable(erlang_aot src/main.cpp)
set_target_properties(erlang_aot PROPERTIES
    OUTPUT_NAME erlangaot
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
erlang_aot_project_options(erlang_aot)
target_compile_definitions(erlang_aot PRIVATE ERLANG_AOT_VERSION="${PROJECT_VERSION}")
target_link_libraries(erlang_aot PRIVATE erlang_aot_abi)
