#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "sicore::sicore" for configuration "Debug"
set_property(TARGET sicore::sicore APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(sicore::sicore PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "C"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/debug/lib/libsicore.a"
  )

list(APPEND _cmake_import_check_targets sicore::sicore )
list(APPEND _cmake_import_check_files_for_sicore::sicore "${_IMPORT_PREFIX}/debug/lib/libsicore.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
