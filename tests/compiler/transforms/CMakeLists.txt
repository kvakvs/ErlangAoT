# Added for parse transforms: term codec invariants against OTP-generated samples (docs/transforms.md).
add_executable(transforms_terms terms.cpp)
clause_project_options(transforms_terms)
target_include_directories(transforms_terms PRIVATE "${PROJECT_SOURCE_DIR}/compiler/src")
target_link_libraries(transforms_terms PRIVATE clause_transforms)
add_test(NAME transforms_terms
    COMMAND transforms_terms "${PROJECT_SOURCE_DIR}/tests/fixtures/transforms/terms")
set_tests_properties(transforms_terms PROPERTIES TIMEOUT 60 PASS_REGULAR_EXPRESSION "transforms_terms: ok")

# Added for parse transforms: --print-abstr forms against OTP epp goldens, compared as terms.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_executable(transforms_compare compare.cpp)
clause_project_options(transforms_compare)
target_include_directories(transforms_compare PRIVATE "${PROJECT_SOURCE_DIR}/compiler/src")
target_link_libraries(transforms_compare PRIVATE clause_transforms)
add_test(NAME transforms_abstract COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/abstract.py"
    "$<TARGET_FILE:clau>" "$<TARGET_FILE:transforms_compare>" "${PROJECT_SOURCE_DIR}/tests/fixtures/transforms/abstract"
    "${CMAKE_CURRENT_BINARY_DIR}/abstract/$<CONFIG>")
set_tests_properties(transforms_abstract PROPERTIES TIMEOUT 120 PASS_REGULAR_EXPRESSION "transforms_abstract: ok")

# Added for parse transforms: .abstr inputs linked into programs, in a project, and malformed forms.
add_test(NAME transforms_inputs COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/inputs.py"
    "$<TARGET_FILE:clau>" "${PROJECT_SOURCE_DIR}/tests/fixtures/transforms/inputs" "${CMAKE_CURRENT_BINARY_DIR}/inputs/$<CONFIG>")
set_tests_properties(transforms_inputs PROPERTIES TIMEOUT 300 PASS_REGULAR_EXPRESSION "transforms_inputs: ok")
