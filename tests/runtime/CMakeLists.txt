add_executable(runtime_term_tag_tests term_tag.cpp)
erlang_aot_project_options(runtime_term_tag_tests)
target_link_libraries(runtime_term_tag_tests PRIVATE erlang_runtime)
add_test(NAME runtime_term_tag COMMAND runtime_term_tag_tests)

add_executable(runtime_term_layout_tests term_layout.cpp)
erlang_aot_project_options(runtime_term_layout_tests)
target_link_libraries(runtime_term_layout_tests PRIVATE erlang_runtime)
target_include_directories(runtime_term_layout_tests PRIVATE "${PROJECT_SOURCE_DIR}/runtime/src/terms")
add_test(NAME runtime_term_layout COMMAND runtime_term_layout_tests)

add_executable(runtime_feature_tests features.cpp)
erlang_aot_project_options(runtime_feature_tests)
target_link_libraries(runtime_feature_tests PRIVATE erlang_runtime)
add_test(NAME runtime_features COMMAND runtime_feature_tests)
add_test(NAME runtime_feature_output COMMAND "${CMAKE_COMMAND}"
    "-DPROGRAM=$<TARGET_FILE:runtime_feature_tests>" "-DFEATURE_NAME=garbage collection"
    -P "${PROJECT_SOURCE_DIR}/tests/abi/feature_output.cmake")

foreach(suite IN ITEMS lifecycle lifecycle_failure memory scheduler)
    add_executable(runtime_${suite} ${suite}.cpp)
    erlang_aot_project_options(runtime_${suite})
    target_link_libraries(runtime_${suite} PRIVATE ErlangAoT::generated_program)
    add_test(NAME runtime_${suite} COMMAND runtime_${suite})
endforeach()
add_test(NAME runtime_lifecycle_output COMMAND "${CMAKE_COMMAND}"
    "-DPROGRAM=$<TARGET_FILE:runtime_lifecycle>"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/lifecycle_output.cmake")

add_test(NAME runtime_generated_link COMMAND "${CMAKE_COMMAND}"
    "-DSOURCE_ROOT=${PROJECT_SOURCE_DIR}" "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/consumer"
    "-DHOST_CXX=${CMAKE_CXX_COMPILER}" "-DHOST_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/link.cmake")
set_tests_properties(runtime_generated_link PROPERTIES TIMEOUT 120)

add_executable(runtime_immediate_tests immediate.cpp)
erlang_aot_project_options(runtime_immediate_tests)
target_link_libraries(runtime_immediate_tests PRIVATE ErlangAoT::generated_program)
add_test(NAME runtime_immediate COMMAND runtime_immediate_tests)

foreach(suite IN ITEMS builtins builtin_bridge)
    add_executable(runtime_${suite} ${suite}.cpp)
    erlang_aot_project_options(runtime_${suite})
    target_link_libraries(runtime_${suite} PRIVATE ErlangAoT::generated_program)
    add_test(NAME runtime_${suite} COMMAND runtime_${suite})
endforeach()
add_test(NAME runtime_builtin_output COMMAND "${CMAKE_COMMAND}"
    "-DPROGRAM=$<TARGET_FILE:runtime_builtin_bridge>"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/builtin_output.cmake")
