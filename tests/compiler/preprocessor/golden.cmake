# Validate pinned scanner expectations even when Erlang is not installed.
file(GLOB fixtures "${FIXTURES}/lexical/*.erl")
foreach(fixture IN LISTS fixtures)
    get_filename_component(stem "${fixture}" NAME_WE)
    get_filename_component(directory "${fixture}" DIRECTORY)
    file(READ "${directory}/${stem}.tokens" expected)
    execute_process(COMMAND "${DUMP}" "${fixture}"
        RESULT_VARIABLE result OUTPUT_VARIABLE actual ERROR_VARIABLE diagnostic)
    if(NOT result STREQUAL "0" OR NOT actual STREQUAL expected)
        message(FATAL_ERROR "Pinned token mismatch for ${fixture}: ${diagnostic}")
    endif()
endforeach()
