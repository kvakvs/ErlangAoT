get_target_property(sdk_dir erlang_llvm_sdk ERLANG_AOT_LLVM_CMAKE_DIR)
add_test(NAME codegen_dependency COMMAND "${CMAKE_COMMAND}"
    "-DSOURCE_ROOT=${PROJECT_SOURCE_DIR}"
    "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/dependency/$<CONFIG>"
    "-DSDK_DIR=${sdk_dir}"
    "-DHOST_SETTINGS=${PROJECT_BINARY_DIR}/TestHost.cmake"
    "-DHOST_CONFIG=$<CONFIG>"
    "-DHOST_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
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

if(ERLANG_AOT_BUILD_RUNTIME)
    add_executable(codegen_runtime_terms runtime_terms.cpp)
    erlang_aot_project_options(codegen_runtime_terms)
    target_include_directories(codegen_runtime_terms PRIVATE "${PROJECT_SOURCE_DIR}/compiler/src")
    target_link_libraries(codegen_runtime_terms PRIVATE erlang_codegen erlang_frontend erlang_llvm_sdk
        ErlangAoT::generated_program)
    add_test(NAME codegen_runtime_terms COMMAND codegen_runtime_terms)
endif()

# Real-source backend adapter remains private until CLI artifact integration (steps 35/36).
add_executable(codegen_specialization specialization.cpp)
erlang_aot_project_options(codegen_specialization)
target_include_directories(codegen_specialization PRIVATE "${PROJECT_SOURCE_DIR}/compiler/src")
target_link_libraries(codegen_specialization PRIVATE erlang_codegen erlang_semantic erlang_llvm_sdk)
target_compile_definitions(codegen_specialization PRIVATE LOWERING_FIXTURES="${PROJECT_SOURCE_DIR}/tests/fixtures/codegen")
add_test(NAME codegen_specialization COMMAND codegen_specialization)

add_executable(codegen_lowering lowering.cpp)
erlang_aot_project_options(codegen_lowering)
target_include_directories(codegen_lowering PRIVATE "${PROJECT_SOURCE_DIR}/compiler/src")
target_link_libraries(codegen_lowering PRIVATE erlang_codegen erlang_semantic erlang_llvm_sdk)
target_compile_definitions(codegen_lowering PRIVATE LOWERING_FIXTURES="${PROJECT_SOURCE_DIR}/tests/fixtures/codegen")
add_test(NAME codegen_lowering COMMAND codegen_lowering)

if("X86" IN_LIST test_backends)
    target_compile_definitions(codegen_lowering PRIVATE ERLANG_AOT_LLVM_X86=1)
endif()

if(ERLANG_AOT_BUILD_RUNTIME)
    add_executable(codegen_registration_emit registration_emit.cpp)
    erlang_aot_project_options(codegen_registration_emit)
    target_include_directories(codegen_registration_emit PRIVATE "${PROJECT_SOURCE_DIR}/compiler/src")
    target_link_libraries(codegen_registration_emit PRIVATE erlang_codegen erlang_semantic erlang_llvm_sdk)
    target_compile_definitions(codegen_registration_emit PRIVATE LOWERING_FIXTURES="${PROJECT_SOURCE_DIR}/tests/fixtures/codegen")
    foreach(level IN ITEMS O0 O2)
        add_test(NAME codegen_registration_${level} COMMAND "${CMAKE_COMMAND}"
            "-DEMITTER=$<TARGET_FILE:codegen_registration_emit>" "-DOPTIMIZATION=${level}"
            "-DSOURCE_ROOT=${PROJECT_SOURCE_DIR}" "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/registration-${level}/$<CONFIG>"
            "-DHOST_SETTINGS=${PROJECT_BINARY_DIR}/TestHost.cmake"
            "-DHOST_CONFIG=$<CONFIG>" "-DHOST_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/registration.cmake")
        set_tests_properties(codegen_registration_${level} PROPERTIES TIMEOUT 120)
    endforeach()

    add_executable(codegen_specialization_emit specialization_emit.cpp)
    erlang_aot_project_options(codegen_specialization_emit)
    target_include_directories(codegen_specialization_emit PRIVATE "${PROJECT_SOURCE_DIR}/compiler/src")
    target_link_libraries(codegen_specialization_emit PRIVATE erlang_codegen erlang_semantic erlang_llvm_sdk)
    target_compile_definitions(codegen_specialization_emit PRIVATE LOWERING_FIXTURES="${PROJECT_SOURCE_DIR}/tests/fixtures/codegen")
    if("X86" IN_LIST test_backends)
        target_compile_definitions(codegen_specialization_emit PRIVATE ERLANG_AOT_LLVM_X86=1)
    endif()
    add_test(NAME codegen_specialized_execution COMMAND "${CMAKE_COMMAND}"
        "-DEMITTER=$<TARGET_FILE:codegen_specialization_emit>"
        "-DCONSUMER=${CMAKE_CURRENT_SOURCE_DIR}/specialization_consumer.cpp"
        "-DSOURCE_ROOT=${PROJECT_SOURCE_DIR}" "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/specialization/$<CONFIG>"
        "-DHOST_SETTINGS=${PROJECT_BINARY_DIR}/TestHost.cmake"
        "-DHOST_CONFIG=$<CONFIG>" "-DHOST_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/registration.cmake")
    set_tests_properties(codegen_specialized_execution PROPERTIES TIMEOUT 120)
endif()

add_executable(codegen_serialization serialization.cpp)
add_executable(codegen_limits limits.cpp)
erlang_aot_project_options(codegen_limits)
target_include_directories(codegen_limits PRIVATE "${PROJECT_SOURCE_DIR}/compiler/src")
target_link_libraries(codegen_limits PRIVATE erlang_codegen erlang_semantic erlang_llvm_sdk)
target_compile_definitions(codegen_limits PRIVATE LOWERING_FIXTURES="${PROJECT_SOURCE_DIR}/tests/fixtures/codegen")
add_test(NAME codegen_limits COMMAND codegen_limits)
add_executable(codegen_write_failure write_failure.cpp)
erlang_aot_project_options(codegen_write_failure)
target_include_directories(codegen_write_failure PRIVATE "${PROJECT_SOURCE_DIR}/compiler/src")
target_link_libraries(codegen_write_failure PRIVATE erlang_artifacts)
add_test(NAME codegen_write_failure COMMAND codegen_write_failure "${CMAKE_CURRENT_BINARY_DIR}/write-failure/$<CONFIG>")
erlang_aot_project_options(codegen_serialization)
target_include_directories(codegen_serialization PRIVATE "${PROJECT_SOURCE_DIR}/compiler/src")
target_link_libraries(codegen_serialization PRIVATE erlang_codegen erlang_semantic erlang_llvm_sdk)
target_compile_definitions(codegen_serialization PRIVATE LOWERING_FIXTURES="${PROJECT_SOURCE_DIR}/tests/fixtures/codegen")
add_test(NAME codegen_serialization COMMAND codegen_serialization)

add_executable(codegen_artifacts artifacts.cpp)
erlang_aot_project_options(codegen_artifacts)
target_include_directories(codegen_artifacts PRIVATE "${PROJECT_SOURCE_DIR}/compiler/src")
target_link_libraries(codegen_artifacts PRIVATE erlang_artifacts erlang_project erlang_semantic)
add_test(NAME codegen_artifacts COMMAND codegen_artifacts "${CMAKE_CURRENT_BINARY_DIR}/artifact-files/$<CONFIG>")

add_test(NAME codegen_options COMMAND "${CMAKE_COMMAND}" "-DTOOL=$<TARGET_FILE:erlang_aot>"
    "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/options/$<CONFIG>" -P "${CMAKE_CURRENT_SOURCE_DIR}/options.cmake")

add_test(NAME codegen_positional COMMAND "${CMAKE_COMMAND}" "-DTOOL=$<TARGET_FILE:erlang_aot>"
    "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/positional/$<CONFIG>" "-DFIXTURES=${PROJECT_SOURCE_DIR}/tests/fixtures/codegen"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/positional.cmake")

add_test(NAME codegen_project COMMAND "${CMAKE_COMMAND}" "-DTOOL=$<TARGET_FILE:erlang_aot>"
    "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/project/$<CONFIG>" -P "${CMAKE_CURRENT_SOURCE_DIR}/project.cmake")

add_test(NAME codegen_progress COMMAND "${CMAKE_COMMAND}" "-DTOOL=$<TARGET_FILE:erlang_aot>"
    "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/progress/$<CONFIG>" -P "${CMAKE_CURRENT_SOURCE_DIR}/progress.cmake")

find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME codegen_resource_cli COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/resource_cli.py"
    "$<TARGET_FILE:erlang_aot>" "${CMAKE_CURRENT_BINARY_DIR}/resource-cli/$<CONFIG>")
get_filename_component(llvm_tools "${sdk_dir}/../../../bin" ABSOLUTE)
find_program(ERLANG_AOT_LLVM_AS NAMES llvm-as HINTS "${llvm_tools}" NO_DEFAULT_PATH REQUIRED)
find_program(ERLANG_AOT_LLVM_DIS NAMES llvm-dis HINTS "${llvm_tools}" NO_DEFAULT_PATH REQUIRED)
find_program(ERLANG_AOT_LLVM_READOBJ NAMES llvm-readobj HINTS "${llvm_tools}" NO_DEFAULT_PATH REQUIRED)
find_program(ERLANG_AOT_LLVM_NM NAMES llvm-nm HINTS "${llvm_tools}" NO_DEFAULT_PATH REQUIRED)
string(REPLACE ";" "," backend_list "${test_backends}")
add_test(NAME codegen_cross_targets COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/cross_targets.py"
    "$<TARGET_FILE:erlang_aot>" "${ERLANG_AOT_LLVM_READOBJ}" "${ERLANG_AOT_LLVM_NM}"
    "${CMAKE_CURRENT_BINARY_DIR}/cross-targets/$<CONFIG>" "${backend_list}")
set_tests_properties(codegen_cross_targets PROPERTIES TIMEOUT 120)
add_test(NAME codegen_placeholders COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/placeholders.py"
    "$<TARGET_FILE:erlang_aot>" "${CMAKE_CURRENT_BINARY_DIR}/placeholders/$<CONFIG>"
    "${PROJECT_SOURCE_DIR}/abi/include/erlang_aot/abi/features.hpp")
set_tests_properties(codegen_placeholders PROPERTIES TIMEOUT 120)
add_test(NAME codegen_inspection COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/inspection.py"
    "$<TARGET_FILE:erlang_aot>" "${ERLANG_AOT_LLVM_AS}" "${ERLANG_AOT_LLVM_DIS}"
    "${CMAKE_CURRENT_BINARY_DIR}/inspection/$<CONFIG>" "${PROJECT_SOURCE_DIR}/tests/fixtures/codegen")

add_test(NAME codegen_types COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/type_inspection.py"
    "$<TARGET_FILE:erlang_aot>" "${CMAKE_CURRENT_BINARY_DIR}/types/$<CONFIG>")

if(ERLANG_AOT_BUILD_RUNTIME)
    foreach(level IN ITEMS O0 O2)
        add_test(NAME codegen_native_${level} COMMAND "${CMAKE_COMMAND}"
            "-DTOOL=$<TARGET_FILE:erlang_aot>" "-DOPTIMIZATION=${level}"
            "-DSOURCE_ROOT=${PROJECT_SOURCE_DIR}" "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/native-${level}/$<CONFIG>"
            "-DHOST_SETTINGS=${PROJECT_BINARY_DIR}/TestHost.cmake"
            "-DHOST_CONFIG=$<CONFIG>" "-DHOST_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/native.cmake")
        set_tests_properties(codegen_native_${level} PROPERTIES TIMEOUT 180)
    endforeach()
    add_test(NAME codegen_differential COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/differential.py" "$<TARGET_FILE:erlang_aot>" "${CMAKE_COMMAND}"
        "${PROJECT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/differential/$<CONFIG>"
        "${PROJECT_BINARY_DIR}/TestHost.cmake" "$<CONFIG>" "${CMAKE_EXECUTABLE_SUFFIX}" "${ERLANG_AOT_ESCRIPT}")
    set_tests_properties(codegen_differential PROPERTIES TIMEOUT 240)
    add_test(NAME patternmatch_evidence COMMAND "${Python3_EXECUTABLE}"
        "${PROJECT_SOURCE_DIR}/tests/compiler/patternmatch/evidence.py"
        "$<TARGET_FILE:erlang_aot>" "${CMAKE_COMMAND}" "${PROJECT_SOURCE_DIR}"
        "${ERLANG_AOT_OTP_SOURCE_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/patternmatch/$<CONFIG>"
        "${PROJECT_BINARY_DIR}/TestHost.cmake" "$<CONFIG>" "${CMAKE_EXECUTABLE_SUFFIX}" "${ERLANG_AOT_ESCRIPT}")
    set_tests_properties(patternmatch_evidence PROPERTIES TIMEOUT 300)
    add_test(NAME codegen_measurements COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/measurements.py" "$<TARGET_FILE:erlang_aot>" "${CMAKE_COMMAND}"
        "${PROJECT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/measurements/$<CONFIG>"
        "${PROJECT_BINARY_DIR}/TestHost.cmake" "$<CONFIG>" "${CMAKE_EXECUTABLE_SUFFIX}"
        "$<TARGET_FILE:codegen_specialization_emit>")
    set_tests_properties(codegen_measurements PROPERTIES TIMEOUT 240)
    add_test(NAME codegen_examples COMMAND "${CMAKE_COMMAND}" "-DTOOL=$<TARGET_FILE:erlang_aot>"
        "-DSOURCE_ROOT=${PROJECT_SOURCE_DIR}" "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/examples/$<CONFIG>"
        "-DHOST_SETTINGS=${PROJECT_BINARY_DIR}/TestHost.cmake"
        "-DHOST_CONFIG=$<CONFIG>" "-DHOST_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/examples.cmake")
    set_tests_properties(codegen_examples PROPERTIES TIMEOUT 180)
endif()
