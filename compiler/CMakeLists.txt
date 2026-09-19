include(${PROJECT_SOURCE_DIR}/cmake/CompilerDependencies.cmake)
add_library(erlang_frontend STATIC src/parsing/probe.cpp src/parsing/token_cursor.cpp
    src/parsing/token_syntax.cpp src/parsing/operator_info.cpp
    src/ast/module.cpp src/ast/builder.cpp src/ast/children.cpp src/ast/clauses.cpp
    src/ast/control.cpp src/parser/control.cpp src/printing/tree_control.cpp
    src/ast/exceptions.cpp src/parser/funs.cpp src/parser/exceptions.cpp src/printing/tree_exceptions.cpp
    src/parser/parser.cpp src/parser/forms.cpp src/parser/literals.cpp src/parser/aggregates.cpp src/parser/expressions.cpp src/parser/clauses.cpp src/parser/maps.cpp src/parser/records.cpp src/parser/structural.cpp src/parser/binaries.cpp
    src/source/source.cpp src/diagnostics/diagnostic.cpp
    src/printing/source.cpp src/printing/token_text.cpp
    src/printing/tree.cpp src/printing/tree_forms.cpp src/printing/tree_expressions.cpp src/printing/tree_structural.cpp
    src/lexer/lexer.cpp src/lexer/numbers.cpp src/lexer/literals.cpp
    src/preprocessor/directives.cpp src/preprocessor/preprocessor.cpp
    src/preprocessor/token_utils.cpp src/preprocessor/arguments.cpp src/preprocessor/macros.cpp
    src/preprocessor/terms.cpp src/preprocessor/value.cpp src/preprocessor/expression_parse.cpp src/preprocessor/expression.cpp
    src/preprocessor/operators.cpp src/preprocessor/guards.cpp src/preprocessor/bits.cpp
    src/preprocessor/session.cpp src/preprocessor/conditions.cpp src/preprocessor/includes.cpp
    src/preprocessor/builtins.cpp src/preprocessor/features.cpp)
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
