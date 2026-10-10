# Compile-time diagnostics against OTP (tests/fixtures/lint/README.md): every case directory holding a golden.json
# compiles as one batch and must report the errors and warnings OTP's compiler reported for it.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME lint_diagnostics COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/lint.py"
    "$<TARGET_FILE:clau>")
set_tests_properties(lint_diagnostics PROPERTIES TIMEOUT 300)

# Live OTP reproduction of every case golden; opt-in like other audits.
if(CLAUSE_OTP_AUDITS)
    add_test(NAME lint_oracle COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/regenerate.py"
        --escript "${CLAUSE_ESCRIPT}" --check)
    set_tests_properties(lint_oracle PROPERTIES TIMEOUT 300)
endif()
