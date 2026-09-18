include(${PROJECT_SOURCE_DIR}/cmake/CompilerDependencies.cmake)
add_library(erlang_frontend STATIC src/parsing/probe.cpp
    src/source/source.cpp src/diagnostics/diagnostic.cpp
    src/lexer/lexer.cpp src/lexer/numbers.cpp src/lexer/literals.cpp
    src/preprocessor/directives.cpp src/preprocessor/preprocessor.cpp
    src/preprocessor/token_utils.cpp src/preprocessor/arguments.cpp src/preprocessor/macros.cpp
    src/preprocessor/terms.cpp src/preprocessor/value.cpp src/preprocessor/expression_parse.cpp src/preprocessor/expression.cpp
    src/preprocessor/operators.cpp src/preprocessor/guards.cpp src/preprocessor/bits.cpp
    src/preprocessor/session.cpp src/preprocessor/pending.cpp
     )
erlang_aot_project_options(erlang_frontend)
target_include_directories(erlang_frontend PUBLIC include PRIVATE src)
target_link_libraries(erlang_frontend PRIVATE erlang_aot_parser_dependency)

add_executable(erlang_aot src/main.cpp)
set_target_properties(erlang_aot PROPERTIES
    OUTPUT_NAME erlangaot
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
erlang_aot_project_options(erlang_aot)
target_compile_definitions(erlang_aot PRIVATE ERLANG_AOT_VERSION="${PROJECT_VERSION}")
target_link_libraries(erlang_aot PRIVATE erlang_aot_abi erlang_frontend)
