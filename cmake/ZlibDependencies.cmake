# Build the Windows fallback before LLVM's configure-time link probe needs it.
include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/ThirdPartyDependencies.cmake")
option(ERLANG_AOT_DOWNLOAD_ZLIB "Download and build missing Windows zlib in thirdparty" ON)

# Retain separate installations for each compiler, architecture and CRT combination.
function(erlang_aot_build_zlib output)
    if(CMAKE_CROSSCOMPILING)
        message(FATAL_ERROR "Install target zlib and set ZLIB_ROOT for cross-compilation.")
    endif()
    erlang_aot_download_dependency(zlib-1.3.2
        "https://zlib.net/fossils/zlib-1.3.2.tar.gz"
        "bb329a0a2cd0274d05519d61c667c062e06990d72e125ee2dfa8de64f0119d16"
        "CMakeLists.txt" source)
    set(crt "${CMAKE_MSVC_RUNTIME_LIBRARY}")
    if(ERLANG_AOT_DOWNLOADED_LLVM AND ERLANG_AOT_DEFAULT_MSVC_RUNTIME)
        set(crt MultiThreaded)
    endif()
    string(SHA256 identity "${CMAKE_C_COMPILER};${CMAKE_C_COMPILER_VERSION};${CMAKE_C_COMPILER_TARGET};${CMAKE_SIZEOF_VOID_P};${CMAKE_GENERATOR};${CMAKE_GENERATOR_PLATFORM};${CMAKE_GENERATOR_TOOLSET};${crt};${CMAKE_C_FLAGS};${CMAKE_C_FLAGS_DEBUG};${CMAKE_C_FLAGS_RELEASE}")
    string(SUBSTRING "${identity}" 0 16 identity)
    get_filename_component(base "${source}/../zlib-build-${identity}" ABSOLUTE)
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
        set(library zs.lib)
        if(configuration STREQUAL "Debug")
            set(library zsd.lib)
        endif()
        if(EXISTS "${prefix}/.${configuration}-complete" AND EXISTS "${prefix}/lib/${library}"
            AND EXISTS "${prefix}/include/zlib.h" AND EXISTS "${prefix}/include/zconf.h")
            continue()
        endif()
        message(STATUS "Building zlib 1.3.2 (${configuration}, CRT ${crt}) in ${base}")
        execute_process(COMMAND "${CMAKE_COMMAND}" -S "${source}" -B "${base}/${configuration}"
            -G "${CMAKE_GENERATOR}" ${generator_args}
            "-DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}"
            "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}"
            "-DCMAKE_C_COMPILER_TARGET=${CMAKE_C_COMPILER_TARGET}"
            "-DCMAKE_C_FLAGS=${CMAKE_C_FLAGS}"
            "-DCMAKE_C_FLAGS_DEBUG=${CMAKE_C_FLAGS_DEBUG}"
            "-DCMAKE_C_FLAGS_RELEASE=${CMAKE_C_FLAGS_RELEASE}"
            "-DCMAKE_MSVC_RUNTIME_LIBRARY=${crt}"
            "-DCMAKE_BUILD_TYPE=${configuration}" "-DCMAKE_INSTALL_PREFIX=${prefix}"
            -DCMAKE_INSTALL_LIBDIR=lib -DZLIB_BUILD_SHARED=OFF
            -DZLIB_BUILD_STATIC=ON -DZLIB_BUILD_TESTING=OFF
            COMMAND_ERROR_IS_FATAL ANY)
        execute_process(COMMAND "${CMAKE_COMMAND}" --build "${base}/${configuration}"
            --config "${configuration}" --target install COMMAND_ERROR_IS_FATAL ANY)
        file(TOUCH "${prefix}/.${configuration}-complete")
    endforeach()
    set(${output} "${prefix}" PARENT_SCOPE)
endfunction()
