# Type inference expectations: every module in tests/fixtures/inference becomes inference_<module>, checking the
# `%% expect:`/`%% today:` signatures above its functions against --print-types.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
file(GLOB fixtures CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/tests/fixtures/inference/*.erl")
foreach(fixture IN LISTS fixtures)
    get_filename_component(name "${fixture}" NAME_WE)
    add_test(NAME inference_${name} COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/expectations.py"
        "$<TARGET_FILE:erlang_aot>" "${fixture}")
    set_tests_properties(inference_${name} PROPERTIES TIMEOUT 120)
endforeach()
