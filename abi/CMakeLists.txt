# Shared declarations remain header-only and independent of LLVM/runtime implementation.
add_library(erlang_aot_abi INTERFACE)
target_include_directories(erlang_aot_abi INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")
# All consumers use the same C++23 project API, including standalone generated-program harnesses.
target_compile_features(erlang_aot_abi INTERFACE cxx_std_23)
set_target_properties(erlang_aot_abi PROPERTIES EXPORT_NAME abi)
if(MSVC AND NOT ERLANG_AOT_MSVC_ITERATOR_DEBUG_LEVEL STREQUAL "")
    target_compile_definitions(erlang_aot_abi INTERFACE "_ITERATOR_DEBUG_LEVEL=${ERLANG_AOT_MSVC_ITERATOR_DEBUG_LEVEL}")
endif()
