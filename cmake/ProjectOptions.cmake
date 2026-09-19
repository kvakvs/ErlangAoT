function(erlang_aot_project_options target)
    set_target_properties(${target} PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        COMPILE_WARNING_AS_ERROR ON
    )
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()
