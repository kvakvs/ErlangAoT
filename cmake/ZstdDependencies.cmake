# Supply the static zstd target required by the official Windows LLVM SDK.
include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/ThirdPartyDependencies.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/WindowsDependencyBuild.cmake")
option(ERLANG_AOT_DOWNLOAD_ZSTD "Download and build missing Windows zstd in thirdparty" ON)

# Pin the upstream release archive and retain native Debug/Release installations.
function(erlang_aot_build_zstd output)
    erlang_aot_download_dependency(zstd-1.5.7
        "https://github.com/facebook/zstd/releases/download/v1.5.7/zstd-1.5.7.tar.gz"
        "eb33e51f49a15e023950cd7825ca74a4a2b43db8354825ac24fc1b7ee09e6fa3"
        "build/cmake/CMakeLists.txt" source)
    erlang_aot_build_windows_dependency(prefix NAME zstd-1.5.7 SOURCE "${source}/build/cmake"
        HEADER zstd.h RELEASE_LIBRARY zstd_static.lib DEBUG_LIBRARY zstd_staticd.lib
        OPTIONS -DZSTD_BUILD_SHARED=OFF -DZSTD_BUILD_STATIC=ON -DZSTD_BUILD_PROGRAMS=OFF
            -DZSTD_BUILD_TESTS=OFF -DZSTD_BUILD_CONTRIB=OFF -DCMAKE_DEBUG_POSTFIX=d)
    set(${output} "${prefix}" PARENT_SCOPE)
endfunction()
