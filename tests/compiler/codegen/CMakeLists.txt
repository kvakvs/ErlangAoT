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

foreach(suite IN ITEMS results ownership target verification emission term_abi features)
    add_executable(codegen_${suite} ${suite}.cpp)
    erlang_aot_project_options(codegen_${suite})
    target_include_directories(codegen_${suite} PRIVATE "${PROJECT_SOURCE_DIR}/compiler/src")
    target_link_libraries(codegen_${suite} PRIVATE erlang_codegen erlang_frontend)
    add_test(NAME codegen_${suite} COMMAND codegen_${suite})
endforeach()
# Only backend-internal tests see LLVM headers; results.cpp verifies the opaque consumer boundary.
target_link_libraries(codegen_ownership PRIVATE erlang_llvm_sdk)
target_link_libraries(codegen_target PRIVATE erlang_llvm_sdk)
target_link_libraries(codegen_verification PRIVATE erlang_llvm_sdk)
target_link_libraries(codegen_emission PRIVATE erlang_llvm_sdk)
target_link_libraries(codegen_term_abi PRIVATE erlang_llvm_sdk erlang_aot_abi)
get_target_property(test_backends erlang_llvm_sdk ERLANG_AOT_LLVM_BACKENDS)
foreach(backend IN LISTS test_backends)
    target_compile_definitions(codegen_target PRIVATE "ERLANG_AOT_LLVM_${backend}=1")
    target_compile_definitions(codegen_emission PRIVATE "ERLANG_AOT_LLVM_${backend}=1")
    target_compile_definitions(codegen_term_abi PRIVATE "ERLANG_AOT_LLVM_${backend}=1")
endforeach()

target_link_libraries(codegen_features PRIVATE erlang_aot_abi)
add_test(NAME codegen_feature_output COMMAND "${CMAKE_COMMAND}"
    "-DPROGRAM=$<TARGET_FILE:codegen_features>" "-DFEATURE_NAME=pattern matching"
    -P "${PROJECT_SOURCE_DIR}/tests/abi/feature_output.cmake")
