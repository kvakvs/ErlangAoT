add_executable(abi_integers integers.cpp)
clause_project_options(abi_integers)
target_link_libraries(abi_integers PRIVATE clause_abi)
add_test(NAME abi_integers COMMAND abi_integers)

add_executable(abi_features features.cpp)
clause_project_options(abi_features)
target_link_libraries(abi_features PRIVATE clause_abi)
add_test(NAME abi_features COMMAND abi_features)
