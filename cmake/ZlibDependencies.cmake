# Build the Windows fallback before LLVM's configure-time link probe needs it.
include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/ThirdPartyDependencies.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/WindowsDependencyBuild.cmake")
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
    erlang_aot_build_windows_dependency(prefix NAME zlib-1.3.2 SOURCE "${source}"
        HEADER zconf.h RELEASE_LIBRARY zs.lib DEBUG_LIBRARY zsd.lib
        OPTIONS -DZLIB_BUILD_SHARED=OFF -DZLIB_BUILD_STATIC=ON -DZLIB_BUILD_TESTING=OFF)
    set(${output} "${prefix}" PARENT_SCOPE)
endfunction()
