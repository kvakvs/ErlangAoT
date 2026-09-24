include(${PROJECT_SOURCE_DIR}/cmake/BoostDependencies.cmake)

add_library(erlang_runtime STATIC src/runtime.cpp src/diagnostics/features.cpp
    src/process/services.cpp src/process/context.cpp src/process/ownership.cpp src/process/storage.cpp
    src/memory/heap.cpp src/memory/copy.cpp
    src/scheduler/services.cpp src/scheduler/registry.cpp src/scheduler/transitions.cpp
    src/terms/factory.cpp src/terms/immediate.cpp src/terms/term.cpp
    src/builtins/registry.cpp src/builtins/invocation.cpp src/builtins/bridge.cpp
    src/modules/services.cpp src/modules/code_server.cpp)
# List review headers for IDE navigation; they are not separate compilation units.
target_sources(erlang_runtime PRIVATE
    include/atom_storage.hpp
    include/base_types.hpp
    include/binary_heap_object.hpp
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
