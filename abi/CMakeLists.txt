# Shared declarations remain header-only and independent of LLVM/runtime implementation.
add_library(erlang_aot_abi INTERFACE)
target_include_directories(erlang_aot_abi INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")
# All consumers use the same C++23 project API, including standalone generated-program harnesses.
target_compile_features(erlang_aot_abi INTERFACE cxx_std_23)
