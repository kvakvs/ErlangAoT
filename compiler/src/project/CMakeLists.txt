# Discover and validate installed project dependencies, including Homebrew prefixes.
include("${CMAKE_CURRENT_LIST_DIR}/cmake/Dependencies.cmake")
add_library(erlang_project STATIC diagnostics.cpp loader.cpp)
erlang_aot_project_options(erlang_project)
target_include_directories(erlang_project PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(erlang_project PRIVATE erlang_project_toml)

target_sources(erlang_project PRIVATE schema.cpp decode.cpp)

target_sources(erlang_project PRIVATE decode_options.cpp)

target_sources(erlang_project PRIVATE paths.cpp)

target_sources(erlang_project PRIVATE glob.cpp glob_utf8.cpp)

target_sources(erlang_project PRIVATE discovery.cpp)

target_sources(erlang_project PRIVATE identity.cpp sources.cpp)

target_sources(erlang_project PRIVATE selection.cpp)

target_sources(erlang_project PRIVATE options.cpp)
target_link_libraries(erlang_project PUBLIC erlang_frontend)

target_sources(erlang_project PRIVATE plan.cpp)

target_sources(erlang_project PRIVATE execution.cpp)

target_sources(erlang_project PRIVATE cli.cpp command.cpp)

target_sources(erlang_project PRIVATE template.cpp)
