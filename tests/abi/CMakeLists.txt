add_executable(abi_integers integers.cpp)
erlang_aot_project_options(abi_integers)
target_link_libraries(abi_integers PRIVATE erlang_aot_abi)
add_test(NAME abi_integers COMMAND abi_integers)

add_executable(abi_features features.cpp)
erlang_aot_project_options(abi_features)
target_link_libraries(abi_features PRIVATE erlang_aot_abi)
add_test(NAME abi_features COMMAND abi_features)
