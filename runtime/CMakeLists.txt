include(${PROJECT_SOURCE_DIR}/cmake/BoostDependencies.cmake)

add_library(erlang_runtime STATIC src/runtime.cpp src/diagnostics/features.cpp
    src/terms/bit_factory.cpp src/terms/bit_access.cpp src/terms/bit_float.cpp src/terms/bit_numeric.cpp src/terms/bit_utf.cpp src/terms/bit_services.cpp
    src/terms/map_factory.cpp src/terms/map_access.cpp src/terms/map_services.cpp
    src/terms/records.cpp src/terms/record_services.cpp src/terms/funs.cpp src/terms/fun_services.cpp src/terms/identities.cpp src/terms/dynamic_calls.cpp
    src/terms/float_factory.cpp src/terms/float_literal.cpp src/terms/float_operations.cpp src/terms/numeric_conversions.cpp src/terms/numeric_order.cpp src/terms/numeric_service.cpp
    src/terms/integer_values.cpp src/terms/integer_factory.cpp src/terms/integer_operations.cpp src/terms/integer_access.cpp src/terms/integer_literal.cpp
    src/terms/integer_service.cpp src/terms/integer_decimal.cpp src/terms/integer_sum.cpp src/terms/integer_words.cpp
    src/memory/heap_terms.cpp src/memory/heap_publication.cpp
    src/terms/container_access.cpp src/terms/container_factory.cpp src/terms/container_services.cpp src/terms/structural_order.cpp
    src/process/generated_calls.cpp src/process/exceptions.cpp src/process/exits.cpp src/process/receive.cpp src/process/stack.cpp src/process/context.cpp src/process/identities.cpp src/process/ownership.cpp src/process/storage.cpp
    src/memory/heap.cpp src/memory/heap_storage.cpp src/memory/heap_reservation.cpp src/memory/off_heap.cpp src/memory/heap_collect.cpp src/memory/heap_walk.cpp src/memory/heap_verify.cpp src/memory/copy.cpp
    src/scheduler/services.cpp src/scheduler/registry.cpp src/scheduler/transitions.cpp src/scheduler/executor.cpp src/scheduler/signals.cpp src/scheduler/ports.cpp src/ports/fd.cpp src/ports/io.cpp src/ports/spawn.cpp src/ports/file.cpp src/ports/value.cpp src/ports/sockets.cpp
    src/terms/term_text.cpp src/terms/term_text_scalars.cpp
    src/terms/atoms.cpp src/terms/atom_spelling.cpp src/terms/factory.cpp src/terms/immediate.cpp src/terms/term.cpp src/terms/equality.cpp src/terms/immediate_order.cpp src/terms/immediate_services.cpp
    src/builtins/registry.cpp src/builtins/invocation.cpp src/builtins/bridge.cpp src/builtins/erlang.cpp src/builtins/term_access.cpp src/builtins/lists.cpp src/builtins/portions.cpp src/builtins/conversions.cpp src/builtins/float_text.cpp src/builtins/text.cpp src/builtins/typed.cpp src/builtins/io.cpp src/builtins/io_format.cpp src/builtins/io_pretty.cpp src/builtins/processes.cpp src/builtins/ports.cpp src/builtins/os.cpp src/builtins/output.cpp
    src/startup/startup.cpp src/startup/arguments.cpp src/startup/options.cpp
    src/startup/halt.cpp
    src/modules/atoms.cpp src/modules/services.cpp src/modules/code_server.cpp src/modules/registration.cpp)
# List review headers for IDE navigation; they are not separate compilation units.
target_sources(erlang_runtime PRIVATE
    include/atom_storage.hpp
    include/base_types.hpp
    include/callable.hpp
    include/code_server.hpp
    include/mailbox.hpp
    include/signals.hpp
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
# The I/O service of ports is platform specific (docs/ports.md#io-thread).
if(WIN32)
    target_sources(erlang_runtime PRIVATE src/ports/io_windows.cpp src/ports/spawn_windows.cpp)
    # Windows 10 is the oldest supported version; Asio asks for it explicitly.
    target_compile_definitions(erlang_runtime PRIVATE _WIN32_WINNT=0x0A00)
else()
    target_sources(erlang_runtime PRIVATE src/ports/io_posix.cpp src/ports/spawn_posix.cpp)
endif()
# Scheduler workers are std::threads; glibc before 2.34 needs -pthread (a plain flag keeps the export self-contained).
# Sockets use Boost.Asio (header-only, from the same Boost), which needs Winsock on Windows.
target_link_libraries(erlang_runtime
    PUBLIC erlang_aot_multiprecision_dependency erlang_aot_abi $<$<PLATFORM_ID:Linux>:-pthread>
    $<$<PLATFORM_ID:Windows>:ws2_32> $<$<PLATFORM_ID:Windows>:mswsock>
)

# Every generated-program consumer links one matching target runtime through this boundary.
# Platform dependencies (threads) come with erlang_runtime.
add_library(erlang_generated_program INTERFACE)
target_link_libraries(erlang_generated_program INTERFACE erlang_runtime)
add_library(ErlangAoT::generated_program ALIAS erlang_generated_program)
# Exported for nested test programs as ErlangAoT::generated_program and ErlangAoT::runtime.
set_target_properties(erlang_generated_program PROPERTIES EXPORT_NAME generated_program)
set_target_properties(erlang_runtime PROPERTIES EXPORT_NAME runtime)
