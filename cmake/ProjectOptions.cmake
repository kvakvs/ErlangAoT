# Apply project diagnostics and source encoding without changing dependency targets.
function(erlang_aot_project_options target)
    set_target_properties(${target} PROPERTIES
        COMPILE_WARNING_AS_ERROR ON
    )
    if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8 /Zc:__cplusplus)
        # Bounded recursive parsing needs the same 8 MiB stack normally available on Unix.
        target_link_options(${target} PRIVATE /STACK:8388608)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    if(MSVC AND NOT ERLANG_AOT_MSVC_ITERATOR_DEBUG_LEVEL STREQUAL "")
        target_compile_definitions(${target} PRIVATE "_ITERATOR_DEBUG_LEVEL=${ERLANG_AOT_MSVC_ITERATOR_DEBUG_LEVEL}")
    endif()
endfunction()
