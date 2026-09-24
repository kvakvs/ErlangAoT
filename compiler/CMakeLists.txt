include(${PROJECT_SOURCE_DIR}/cmake/LLVMDependencies.cmake)
include(${PROJECT_SOURCE_DIR}/cmake/CompilerDependencies.cmake)
add_library(erlang_frontend STATIC src/parsing/probe.cpp src/parsing/token_cursor.cpp
    src/parsing/token_syntax.cpp src/parsing/operator_info.cpp
    src/parsing/delimiters.cpp src/parser/diagnostics.cpp
    src/ast/module.cpp src/ast/builder.cpp src/ast/children.cpp src/ast/clauses.cpp
    src/ast/attributes.cpp src/parser/attributes.cpp src/parser/attribute_terms.cpp
    src/parser/term_value.cpp src/parser/term_bits.cpp src/parser/declarations.cpp src/parser/documentation.cpp
    src/printing/tree_attributes.cpp
    src/ast/types.cpp src/parser/types.cpp src/parser/type_primary.cpp src/parser/type_names.cpp src/parser/type_structural.cpp
    src/printing/tree_types.cpp
    src/parser/specifications.cpp src/ast/specifications.cpp src/printing/tree_specifications.cpp
    src/ast/control.cpp src/parser/control.cpp src/printing/tree_control.cpp
    src/ast/exceptions.cpp src/parser/funs.cpp src/parser/exceptions.cpp src/printing/tree_exceptions.cpp
    src/ast/comprehensions.cpp src/parser/comprehensions.cpp src/printing/tree_comprehensions.cpp
    src/parser/parser.cpp src/parser/forms.cpp src/parser/literals.cpp src/parser/aggregates.cpp src/parser/expressions.cpp src/parser/clauses.cpp src/parser/maps.cpp src/parser/records.cpp src/parser/structural.cpp src/parser/binaries.cpp
    src/source/source.cpp src/diagnostics/diagnostic.cpp
    src/printing/source.cpp src/printing/token_text.cpp src/printing/printable.cpp
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

add_executable(erlang_aot src/main.cpp src/driver/options.cpp src/driver/frontend.cpp)
set_target_properties(erlang_aot PROPERTIES
    OUTPUT_NAME erlangaot
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
erlang_aot_project_options(erlang_aot)
target_compile_definitions(erlang_aot PRIVATE ERLANG_AOT_VERSION="${PROJECT_VERSION}")
target_link_libraries(erlang_aot PRIVATE erlang_aot_abi)

add_subdirectory(src/project)

target_link_libraries(erlang_aot PRIVATE erlang_project)

# Keep all LLVM usage private; the CLI continues to use its existing compile placeholder.
add_library(erlang_codegen STATIC src/codegen/sdk.cpp src/codegen/compilation.cpp
    src/codegen/result.cpp src/codegen/diagnostics.cpp src/codegen/target.cpp
    src/codegen/target_backends.cpp src/codegen/verification.cpp src/codegen/emission.cpp)
erlang_aot_project_options(erlang_codegen)
target_link_libraries(erlang_codegen PRIVATE erlang_llvm_sdk erlang_frontend)
get_target_property(codegen_backends erlang_llvm_sdk ERLANG_AOT_LLVM_BACKENDS)
foreach(backend IN LISTS codegen_backends)
    target_compile_definitions(erlang_codegen PRIVATE "ERLANG_AOT_LLVM_${backend}=1")
endforeach()
target_link_libraries(erlang_aot PRIVATE erlang_codegen)
