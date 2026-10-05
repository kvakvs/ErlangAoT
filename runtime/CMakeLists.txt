include(${PROJECT_SOURCE_DIR}/cmake/BoostDependencies.cmake)

add_library(erlang_runtime STATIC src/runtime.cpp src/diagnostics/features.cpp
    src/terms/bit_factory.cpp src/terms/bit_access.cpp src/terms/bit_float.cpp src/terms/bit_numeric.cpp src/terms/bit_utf.cpp src/terms/bit_services.cpp
    src/terms/map_factory.cpp src/terms/map_access.cpp src/terms/map_services.cpp
    src/terms/float_factory.cpp src/terms/float_literal.cpp src/terms/float_operations.cpp src/terms/numeric_conversions.cpp src/terms/numeric_order.cpp src/terms/numeric_service.cpp
    src/terms/integer_values.cpp src/terms/integer_factory.cpp src/terms/integer_operations.cpp src/terms/integer_access.cpp src/terms/integer_literal.cpp
    src/terms/integer_service.cpp src/terms/integer_decimal.cpp src/terms/integer_sum.cpp src/terms/integer_words.cpp
    src/memory/heap_terms.cpp src/memory/heap_publication.cpp
    src/terms/container_access.cpp src/terms/container_factory.cpp src/terms/container_services.cpp src/terms/structural_order.cpp
    src/process/generated_calls.cpp src/process/exceptions.cpp src/process/roots.cpp src/process/services.cpp src/process/context.cpp src/process/ownership.cpp src/process/storage.cpp
    src/memory/heap.cpp src/memory/heap_storage.cpp src/memory/heap_reservation.cpp src/memory/off_heap.cpp src/memory/heap_collect.cpp src/memory/heap_walk.cpp src/memory/heap_verify.cpp src/memory/copy.cpp
    src/scheduler/services.cpp src/scheduler/registry.cpp src/scheduler/transitions.cpp
    src/terms/term_text.cpp src/terms/term_text_scalars.cpp
    src/terms/atoms.cpp src/terms/atom_spelling.cpp src/terms/factory.cpp src/terms/immediate.cpp src/terms/term.cpp src/terms/equality.cpp src/terms/immediate_order.cpp src/terms/immediate_services.cpp
    src/builtins/registry.cpp src/builtins/invocation.cpp src/builtins/bridge.cpp src/builtins/output.cpp
    src/startup/startup.cpp src/startup/arguments.cpp src/startup/halt.cpp
    src/modules/atoms.cpp src/modules/services.cpp src/modules/code_server.cpp src/modules/registration.cpp)
# List review headers for IDE navigation; they are not separate compilation units.
target_sources(erlang_runtime PRIVATE
    include/atom_storage.hpp
    include/base_types.hpp
    include/callable.hpp
    include/code_server.hpp
    include/mailbox.hpp
    include/process.hpp
    include/process_heap.hpp
    include/scheduler.hpp
    src/terms/term_layout.hpp
    include/erlang_aot/runtime/base_types.hpp
    include/erlang_aot/runtime/terms.hpp
    include/erlang_aot/runtime/process_state.hpp
    include/erlang_aot/runtime/scheduler.hpp
    include/terms.hpp
)
set_target_properties(erlang_runtime PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)
erlang_aot_project_options(erlang_runtime)
# One section per function/object lets -Os executables drop unused runtime code at link time.
if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    target_compile_options(erlang_runtime PRIVATE /Gy /Gw)
else()
    target_compile_options(erlang_runtime PRIVATE -ffunction-sections -fdata-sections)
endif()
# Runtime headers use Multiprecision, so consumers need the same header-only dependency.
target_include_directories(erlang_runtime PUBLIC include)
target_link_libraries(erlang_runtime
    PUBLIC erlang_aot_multiprecision_dependency erlang_aot_abi
)

# Every generated-program consumer links one matching target runtime through this boundary.
# Current lifecycle has no additional OS libraries; future platform dependencies belong here/runtime.
add_library(erlang_generated_program INTERFACE)
target_link_libraries(erlang_generated_program INTERFACE erlang_runtime)
add_library(ErlangAoT::generated_program ALIAS erlang_generated_program)
