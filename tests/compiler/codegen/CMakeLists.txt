add_executable(codegen_sdk sdk.cpp)
erlang_aot_project_options(codegen_sdk)
target_include_directories(codegen_sdk PRIVATE "${PROJECT_SOURCE_DIR}/compiler/src")
target_link_libraries(codegen_sdk PRIVATE erlang_codegen erlang_llvm_sdk)
add_test(NAME codegen_sdk COMMAND codegen_sdk)

get_target_property(sdk_dir erlang_llvm_sdk ERLANG_AOT_LLVM_CMAKE_DIR)
add_test(NAME codegen_dependency COMMAND "${CMAKE_COMMAND}"
    "-DSOURCE_ROOT=${PROJECT_SOURCE_DIR}"
    "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/dependency"
    "-DSDK_DIR=${sdk_dir}"
    "-DHOST_CXX=${CMAKE_CXX_COMPILER}"
    "-DHOST_GENERATOR=${CMAKE_GENERATOR}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/dependency.cmake")
set_tests_properties(codegen_dependency PROPERTIES TIMEOUT 120)
