# Exercise SDK selection/failure in fresh child configurations, without installing anything.
file(REMOVE_RECURSE "${TEST_DIR}")
file(MAKE_DIRECTORY "${TEST_DIR}/source" "${TEST_DIR}/private-sdk/lib/cmake/llvm")
file(WRITE "${TEST_DIR}/private-sdk/lib/cmake/llvm/LLVMConfig.cmake"
    "message(FATAL_ERROR \"PRIVATE_PACKAGE_EXECUTED\")\n")
file(WRITE "${TEST_DIR}/source/CMakeLists.txt" [=[
cmake_minimum_required(VERSION 3.28)
project(LLVMDependencyProbe LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
include("${SOURCE_ROOT}/cmake/LLVMPolicy.cmake")
if(CASE STREQUAL "version")
    erlang_aot_check_llvm_version("${TEST_VERSION}")
    return()
endif()
if(CASE STREQUAL "absent" OR CASE STREQUAL "private_only")
    erlang_aot_llvm_roots(CMAKE_IGNORE_PREFIX_PATH)
    erlang_aot_llvm_search_paths(CMAKE_IGNORE_PATH)
endif()
include("${SOURCE_ROOT}/cmake/LLVMDependencies.cmake")
add_executable(smoke "${SOURCE_ROOT}/cmake/probes/llvm.cpp")
target_link_libraries(smoke PRIVATE erlang_llvm_sdk)
]=])

# Preserve child diagnostics and check the expected reason, rather than only its exit code.
function(configure_case name expected)
    execute_process(COMMAND "${CMAKE_COMMAND}" -E env --unset=LLVM_DIR "CXXFLAGS="
        "${CMAKE_COMMAND}" -S "${TEST_DIR}/source" -B "${TEST_DIR}/${name}"
        -G "${HOST_GENERATOR}" "-DCMAKE_CXX_COMPILER=${HOST_CXX}"
        "-DSOURCE_ROOT=${SOURCE_ROOT}" "-DCASE=${name}" ${ARGN}
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    file(WRITE "${TEST_DIR}/${name}.log" "${output}\n${error}")
    if(expected STREQUAL "success")
        if(NOT result EQUAL 0)
            message(FATAL_ERROR "${name}: ${output}\n${error}")
        endif()
    elseif(result EQUAL 0 OR NOT "${output}${error}" MATCHES "${expected}")
        message(FATAL_ERROR "${name}: expected ${expected}, got ${result}: ${output}\n${error}")
    endif()
    if("${output}${error}" MATCHES "PRIVATE_PACKAGE_EXECUTED")
        message(FATAL_ERROR "Private SDK package was executed")
    endif()
endfunction()

# Exercise explicit selection through a second canonical path to the installed SDK.
configure_case(automatic success)
file(REAL_PATH "${SDK_DIR}" canonical_sdk)
configure_case(selected success "-DLLVM_DIR=${canonical_sdk}")
execute_process(COMMAND "${CMAKE_COMMAND}" --build "${TEST_DIR}/selected" --config Debug
    RESULT_VARIABLE built OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT built EQUAL 0)
    message(FATAL_ERROR "Selected SDK smoke build failed: ${output}\n${error}")
endif()
configure_case(absent "Could not find a package configuration file provided by .LLVM.")
configure_case(private "private (copy|path) rejected" "-DLLVM_DIR=${TEST_DIR}/private-sdk/lib/cmake/llvm")
configure_case(private_only "Could not find a package configuration file provided by .LLVM."
    "-DCMAKE_PREFIX_PATH=${TEST_DIR}/private-sdk")
configure_case(missing "directory does not exist" "-DLLVM_DIR=${TEST_DIR}/nonexistent-sdk")
foreach(version IN ITEMS 23.1.0 22.1.1 24.1.1 23.1.1git)
    configure_case("version_${version}" "stable.*is required"
        "-DCASE=version" "-DTEST_VERSION=${version}")
endforeach()
configure_case(version_success success "-DCASE=version" "-DTEST_VERSION=23.1.2")

execute_process(COMMAND "${CMAKE_COMMAND}" -E env "CXXFLAGS=" "${CMAKE_COMMAND}"
    -S "${SOURCE_ROOT}" -B "${TEST_DIR}/runtime" -G "${HOST_GENERATOR}"
    "-DCMAKE_CXX_COMPILER=${HOST_CXX}" -DERLANG_AOT_BUILD_COMPILER=OFF
    -DERLANG_AOT_BUILD_RUNTIME=ON -DBUILD_TESTING=OFF
    "-DLLVM_DIR=${TEST_DIR}/private-sdk/lib/cmake/llvm" -DCMAKE_DISABLE_FIND_PACKAGE_LLVM=TRUE
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 0 OR "${output}${error}" MATCHES "LLVM .*global SDK search")
    message(FATAL_ERROR "Runtime-only configuration depended on LLVM: ${output}\n${error}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" --build "${TEST_DIR}/runtime" --config Debug
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Runtime-only build failed: ${output}\n${error}")
endif()
file(GLOB_RECURSE downloaded "${TEST_DIR}/*-populate*" "${TEST_DIR}/_deps/*")
if(downloaded)
    message(FATAL_ERROR "Dependency discovery created download/bootstrap artifacts: ${downloaded}")
endif()
