# Shared declarations remain header-only and independent of LLVM/runtime implementation.
add_library(erlang_aot_abi INTERFACE)
target_include_directories(erlang_aot_abi INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")
