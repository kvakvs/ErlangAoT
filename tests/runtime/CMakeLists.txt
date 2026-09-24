add_executable(runtime_term_tag_tests term_tag.cpp)
erlang_aot_project_options(runtime_term_tag_tests)
target_link_libraries(runtime_term_tag_tests PRIVATE erlang_runtime)
add_test(NAME runtime_term_tag COMMAND runtime_term_tag_tests)

add_executable(runtime_term_layout_tests term_layout.cpp)
erlang_aot_project_options(runtime_term_layout_tests)
target_link_libraries(runtime_term_layout_tests PRIVATE erlang_runtime)
add_test(NAME runtime_term_layout COMMAND runtime_term_layout_tests)

add_executable(runtime_feature_tests features.cpp)
erlang_aot_project_options(runtime_feature_tests)
target_link_libraries(runtime_feature_tests PRIVATE erlang_runtime)
add_test(NAME runtime_features COMMAND runtime_feature_tests)
add_test(NAME runtime_feature_output COMMAND "${CMAKE_COMMAND}"
    "-DPROGRAM=$<TARGET_FILE:runtime_feature_tests>" "-DFEATURE_NAME=garbage collection"
    -P "${PROJECT_SOURCE_DIR}/tests/abi/feature_output.cmake")
