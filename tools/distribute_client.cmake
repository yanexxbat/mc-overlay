# Nur Launcher und optionale Anleitung weitergeben; Entwickler-Ausgaben bleiben separat.
if(NOT EXISTS "${CLIENT_EXE}" OR NOT EXISTS "${GUIDE}")
    message(FATAL_ERROR "Client executable or start guide is missing")
endif()
get_filename_component(client_dir "${CLIENT_EXE}" DIRECTORY)
get_filename_component(client_name "${CLIENT_EXE}" NAME)
configure_file("${GUIDE}" "${client_dir}/START.txt" COPYONLY)
# Relative Namen verhindern Build-Pfade und Unterordner in der ZIP.
execute_process(COMMAND "${CMAKE_COMMAND}" -E tar cf "${ARCHIVE}.part" --format=zip "${client_name}" START.txt
    WORKING_DIRECTORY "${client_dir}" RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    file(REMOVE "${ARCHIVE}.part")
    message(FATAL_ERROR "Could not package the client")
endif()
file(RENAME "${ARCHIVE}.part" "${ARCHIVE}")
