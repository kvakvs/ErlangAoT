# Keep SDK provenance/version checks reusable without loading LLVM package code.
include_guard(GLOBAL)

# Limit SDK discovery to conventional global installation and package-manager roots.
function(erlang_aot_llvm_roots output)
    if(CMAKE_HOST_WIN32)
        set(roots "$ENV{ProgramFiles}/LLVM" "$ENV{ProgramW6432}/LLVM")
    else()
        set(roots /usr /usr/local /opt/homebrew /opt/local /opt/llvm
            /home/linuxbrew/.linuxbrew /Library/Developer/Toolchains)
    endif()
    set(${output} "${roots}" PARENT_SCOPE)
endfunction()

# Accept installed SDKs (including thirdparty downloads), but never LLVM build trees.
function(erlang_aot_check_llvm_location directory)
    if(NOT IS_DIRECTORY "${directory}")
        message(FATAL_ERROR "LLVM SDK directory does not exist: ${directory}")
    endif()
    file(REAL_PATH "${directory}" resolved)
    set(ancestor "${resolved}")
    while(NOT ancestor STREQUAL "")
        if(EXISTS "${ancestor}/CMakeCache.txt")
            message(FATAL_ERROR "LLVM build-tree SDK is not supported: ${directory}")
        endif()
        cmake_path(GET ancestor PARENT_PATH parent)
        if(parent STREQUAL ancestor)
            break()
        endif()
        set(ancestor "${parent}")
    endwhile()
endfunction()

# Reject development snapshots and other release lines even if LLVM accepts their package version.
function(erlang_aot_check_llvm_version version)
    if(NOT version MATCHES "^23\\.1\\.[0-9]+$" OR version VERSION_LESS "23.1.1")
        message(FATAL_ERROR "LLVM 23.1.x >=23.1.1 (stable) is required; found ${version}.")
    endif()
endfunction()

# List installed layouts only; do not consult repository hints or the CMake package registry.
function(erlang_aot_llvm_search_paths output)
    erlang_aot_llvm_roots(roots)
    set(paths)
    foreach(root IN LISTS roots)
        list(APPEND paths "${root}" "${root}/lib/cmake/llvm" "${root}/lib64/cmake/llvm")
        file(GLOB packages LIST_DIRECTORIES TRUE
            "${root}/lib/llvm-*/lib/cmake/llvm"
            "${root}/opt/llvm*/lib/cmake/llvm"
            "${root}/Cellar/llvm*/*/lib/cmake/llvm"
            "${root}/LLVM*/lib/cmake/llvm"
            "${root}/llvm*/lib/cmake/llvm")
        list(SORT packages COMPARE NATURAL ORDER DESCENDING)
        list(APPEND paths ${packages})
    endforeach()
    set(${output} "${paths}" PARENT_SCOPE)
endfunction()
