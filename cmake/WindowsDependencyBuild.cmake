# Share configure-time builds for static Windows LLVM dependencies.
include_guard(GLOBAL)

# Isolate retained artifacts by dependency options, host toolchain and runtime library.
function(erlang_aot_build_windows_dependency output)
    cmake_parse_arguments(PARSE_ARGV 1 dependency ""
        "NAME;SOURCE;HEADER;RELEASE_LIBRARY;DEBUG_LIBRARY" "OPTIONS")
    if(CMAKE_CROSSCOMPILING)
        message(FATAL_ERROR "Automatic ${dependency_NAME} builds require a native Windows toolchain; supply an installed target dependency.")
    endif()
    set(crt "${CMAKE_MSVC_RUNTIME_LIBRARY}")
    if(ERLANG_AOT_DOWNLOADED_LLVM AND ERLANG_AOT_DEFAULT_MSVC_RUNTIME)
        set(crt MultiThreaded)
    endif()
    string(SHA256 identity "${dependency_NAME};${dependency_OPTIONS};${CMAKE_CXX_COMPILER};${CMAKE_C_COMPILER};${CMAKE_C_COMPILER_VERSION};${CMAKE_C_COMPILER_TARGET};${CMAKE_SIZEOF_VOID_P};${CMAKE_GENERATOR};${CMAKE_GENERATOR_PLATFORM};${CMAKE_GENERATOR_TOOLSET};${crt};${CMAKE_C_FLAGS};${CMAKE_C_FLAGS_DEBUG};${CMAKE_C_FLAGS_RELEASE}")
    string(SUBSTRING "${identity}" 0 16 identity)
    get_filename_component(base "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../thirdparty/${dependency_NAME}-build-${identity}" ABSOLUTE)
    set(prefix "${base}/install")
    file(MAKE_DIRECTORY "${base}")
    file(LOCK "${base}/.build.lock" GUARD FUNCTION TIMEOUT 1800)
    set(generator_args)
    foreach(pair IN ITEMS "PLATFORM;-A" "TOOLSET;-T")
        list(GET pair 0 variable)
        list(GET pair 1 flag)
        if(CMAKE_GENERATOR_${variable})
            list(APPEND generator_args "${flag}" "${CMAKE_GENERATOR_${variable}}")
        endif()
    endforeach()
    foreach(configuration IN ITEMS Debug Release)
        set(library "${dependency_RELEASE_LIBRARY}")
        if(configuration STREQUAL "Debug")
            set(library "${dependency_DEBUG_LIBRARY}")
        endif()
        if(EXISTS "${prefix}/.${configuration}-complete" AND EXISTS "${prefix}/lib/${library}"
            AND EXISTS "${prefix}/include/${dependency_HEADER}")
            continue()
        endif()
        message(STATUS "Building ${dependency_NAME} (${configuration}, CRT ${crt}) in ${base}")
        execute_process(COMMAND "${CMAKE_COMMAND}" -S "${dependency_SOURCE}" -B "${base}/${configuration}"
            -G "${CMAKE_GENERATOR}" ${generator_args}
            "-DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}"
            "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}"
            "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
            "-DCMAKE_POLICY_DEFAULT_CMP0091=NEW"
            "-DCMAKE_C_COMPILER_TARGET=${CMAKE_C_COMPILER_TARGET}"
            "-DCMAKE_C_FLAGS=${CMAKE_C_FLAGS}"
            "-DCMAKE_C_FLAGS_DEBUG=${CMAKE_C_FLAGS_DEBUG}"
            "-DCMAKE_C_FLAGS_RELEASE=${CMAKE_C_FLAGS_RELEASE}"
            "-DCMAKE_MSVC_RUNTIME_LIBRARY=${crt}"
            "-DCMAKE_BUILD_TYPE=${configuration}" "-DCMAKE_INSTALL_PREFIX=${prefix}"
            -DCMAKE_INSTALL_LIBDIR=lib ${dependency_OPTIONS}
            COMMAND_ERROR_IS_FATAL ANY)
        execute_process(COMMAND "${CMAKE_COMMAND}" --build "${base}/${configuration}"
            --config "${configuration}" --target install COMMAND_ERROR_IS_FATAL ANY)
        file(TOUCH "${prefix}/.${configuration}-complete")
    endforeach()
    set(${output} "${prefix}" PARENT_SCOPE)
endfunction()
