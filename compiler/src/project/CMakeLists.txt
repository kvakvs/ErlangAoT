include(cmake/Dependencies.cmake)

add_library(erlang_project INTERFACE)
target_include_directories(erlang_project INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/..")
