# Added for parse transforms: term codec invariants against OTP-generated samples (docs/transforms.md).
add_executable(transforms_terms terms.cpp)
clause_project_options(transforms_terms)
target_include_directories(transforms_terms PRIVATE "${PROJECT_SOURCE_DIR}/compiler/src")
target_link_libraries(transforms_terms PRIVATE clause_transforms)
add_test(NAME transforms_terms
    COMMAND transforms_terms "${PROJECT_SOURCE_DIR}/tests/fixtures/transforms/terms")
set_tests_properties(transforms_terms PROPERTIES TIMEOUT 60 PASS_REGULAR_EXPRESSION "transforms_terms: ok")
