add_executable(runtime_term_tag_tests term_tag.cpp)
erlang_aot_project_options(runtime_term_tag_tests)
target_link_libraries(runtime_term_tag_tests PRIVATE erlang_runtime)
add_test(NAME runtime_term_tag COMMAND runtime_term_tag_tests)

add_library(runtime_term_layout_tests OBJECT term_layout.cpp)
erlang_aot_project_options(runtime_term_layout_tests)
target_link_libraries(runtime_term_layout_tests PRIVATE erlang_runtime)
target_include_directories(runtime_term_layout_tests PRIVATE "${PROJECT_SOURCE_DIR}/runtime/src/terms")

add_executable(runtime_feature_tests features.cpp)
erlang_aot_project_options(runtime_feature_tests)
target_link_libraries(runtime_feature_tests PRIVATE erlang_runtime)
add_test(NAME runtime_features COMMAND runtime_feature_tests)
add_test(NAME runtime_feature_output COMMAND "${CMAKE_COMMAND}"
    "-DPROGRAM=$<TARGET_FILE:runtime_feature_tests>" "-DFEATURE_NAME=garbage collection"
    "-DCONTEXT_FIXTURES=${PROJECT_SOURCE_DIR}/tests/fixtures/runtime/diagnostics"
    -P "${PROJECT_SOURCE_DIR}/tests/abi/feature_output.cmake")

foreach(suite IN ITEMS lifecycle_failure memory roots containers integers floats maps bitstrings admission heap_fragments scheduler registration startup)
    add_executable(runtime_${suite} ${suite}.cpp)
    erlang_aot_project_options(runtime_${suite})
    target_link_libraries(runtime_${suite} PRIVATE ErlangAoT::generated_program)
    add_test(NAME runtime_${suite} COMMAND runtime_${suite})
endforeach()
add_test(NAME runtime_generated_link COMMAND "${CMAKE_COMMAND}"
    "-DSOURCE_ROOT=${PROJECT_SOURCE_DIR}" "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/consumer/$<CONFIG>"
    "-DHOST_SETTINGS=${PROJECT_BINARY_DIR}/TestHost.cmake"
    "-DHOST_CONFIG=$<CONFIG>" "-DHOST_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/link.cmake")
set_tests_properties(runtime_generated_link PROPERTIES TIMEOUT 120)

# Off-heap binary ownership needs private layout headers; source cannot observe buffer references.
add_executable(runtime_off_heap off_heap.cpp)
erlang_aot_project_options(runtime_off_heap)
target_link_libraries(runtime_off_heap PRIVATE ErlangAoT::generated_program)
target_include_directories(runtime_off_heap PRIVATE "${PROJECT_SOURCE_DIR}/runtime/src")
add_test(NAME runtime_off_heap COMMAND runtime_off_heap)

# Heap walker and verifier invariants over every admitted layout and malformed synthetic areas.
add_executable(runtime_heap_walk heap_walk.cpp)
erlang_aot_project_options(runtime_heap_walk)
target_link_libraries(runtime_heap_walk PRIVATE ErlangAoT::generated_program)
target_include_directories(runtime_heap_walk PRIVATE "${PROJECT_SOURCE_DIR}/runtime/src")
add_test(NAME runtime_heap_walk COMMAND runtime_heap_walk)

# Descriptive heap kernel/footprint numbers for docs/runtime-heap.md; printed, not gated.
add_executable(runtime_heap_measurements heap_measurements.cpp)
erlang_aot_project_options(runtime_heap_measurements)
target_link_libraries(runtime_heap_measurements PRIVATE ErlangAoT::generated_program)
add_test(NAME runtime_heap_measurements COMMAND runtime_heap_measurements)
set_tests_properties(runtime_heap_measurements PROPERTIES TIMEOUT 240 LABELS full_only)

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

# OTP-generated ~w/erlang:display/1 goldens over every corpus result value, plus bounded-work checks.
add_executable(runtime_printing printing.cpp)
erlang_aot_project_options(runtime_printing)
target_link_libraries(runtime_printing PRIVATE ErlangAoT::generated_program)
add_test(NAME runtime_printing COMMAND runtime_printing
    "${PROJECT_SOURCE_DIR}/tests/fixtures/printing/values.txt"
    "${PROJECT_SOURCE_DIR}/tests/fixtures/printing/write.txt"
    "${PROJECT_SOURCE_DIR}/tests/fixtures/printing/display.txt")
set_tests_properties(runtime_printing PROPERTIES TIMEOUT 120)

add_executable(runtime_services services.cpp)
erlang_aot_project_options(runtime_services)
target_link_libraries(runtime_services PRIVATE ErlangAoT::generated_program)
add_test(NAME runtime_services COMMAND runtime_services)
add_test(NAME runtime_service_output COMMAND "${CMAKE_COMMAND}"
    "-DPROGRAM=$<TARGET_FILE:runtime_services>"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/service_output.cmake")
