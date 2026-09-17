add_library(erlang_runtime STATIC src/runtime.cpp)
set_target_properties(erlang_runtime PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)
erlang_aot_project_options(erlang_runtime)
target_link_libraries(erlang_runtime PRIVATE erlang_aot_abi)
