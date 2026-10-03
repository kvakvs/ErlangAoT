# End-goal program fixtures: offline golden hash checks plus today's compile diagnostics.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME programs_compile COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/programs.py"
    "$<TARGET_FILE:erlang_aot>" "${CMAKE_CURRENT_BINARY_DIR}/compile/$<CONFIG>")
set_tests_properties(programs_compile PROPERTIES TIMEOUT 120)

# Live OTP reproduction of every stdout/exit-status golden; opt-in like other audits.
if(ERLANG_AOT_OTP_AUDITS)
    add_test(NAME programs_oracle COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/regenerate.py"
        --escript "${ERLANG_AOT_ESCRIPT}" --check)
    set_tests_properties(programs_oracle PROPERTIES TIMEOUT 600)
endif()
