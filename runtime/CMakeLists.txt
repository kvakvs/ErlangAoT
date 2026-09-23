include(${PROJECT_SOURCE_DIR}/cmake/BoostDependencies.cmake)

add_library(erlang_runtime STATIC src/runtime.cpp)
# List review headers for IDE navigation; they are not separate compilation units.
target_sources(erlang_runtime PRIVATE
    include/atom_storage.hpp
    include/base_types.hpp
    include/binary_heap_object.hpp
    include/callable.hpp
    include/code_server.hpp
    include/mailbox.hpp
    include/native_callable.hpp
    include/process.hpp
    include/process_heap.hpp
    include/scheduler.hpp
    include/term_layout.hpp
    include/terms.hpp
)
set_target_properties(erlang_runtime PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)
erlang_aot_project_options(erlang_runtime)
# Runtime headers use Multiprecision, so consumers need the same header-only dependency.
target_include_directories(erlang_runtime PUBLIC include)
target_link_libraries(erlang_runtime
    PUBLIC erlang_aot_multiprecision_dependency
    PRIVATE erlang_aot_abi
)
