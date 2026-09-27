# Validate native Windows development builds before discovering third-party dependencies.
include_guard(GLOBAL)
if(NOT CMAKE_HOST_WIN32 OR NOT WIN32 OR CMAKE_CROSSCOMPILING)
    return()
endif()

if(NOT MSVC AND NOT CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC")
    message(FATAL_ERROR "Windows development builds require MSVC or Clang targeting the MSVC ABI. Use the windows preset from a Visual Studio developer shell; MinGW libraries are not interchangeable with the LLVM SDK.")
endif()

# Share the DLL CRT with the LLVM SDK and consumers; explicit toolchain choices take precedence.
if(NOT DEFINED CMAKE_MSVC_RUNTIME_LIBRARY)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    set(ERLANG_AOT_DEFAULT_MSVC_RUNTIME TRUE)
endif()
set(ERLANG_AOT_MSVC_ITERATOR_DEBUG_LEVEL "" CACHE STRING "Explicit MSVC STL iterator ABI (empty uses the toolchain default)")

# Compile and link, without executing, against the Windows SDK and required C++23 library surface.
include(CheckCXXSourceCompiles)
file(READ "${CMAKE_CURRENT_LIST_DIR}/probes/windows.cpp" windows_probe)
check_cxx_source_compiles("${windows_probe}" ERLANG_AOT_WINDOWS_HOST_LINKS)
if(NOT ERLANG_AOT_WINDOWS_HOST_LINKS)
    message(FATAL_ERROR "The Windows SDK and C++23 standard library must link with the selected compiler. Install Visual Studio C++ tools and a Windows SDK, then configure from its developer shell. See CMakeConfigureLog.yaml.")
endif()
message(STATUS "Windows host: ${CMAKE_CXX_COMPILER_ID}, ${CMAKE_SIZEOF_VOID_P}-byte pointers, CRT ${CMAKE_MSVC_RUNTIME_LIBRARY}")
