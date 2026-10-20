#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "sicore::sicore" for configuration "Release"
set_property(TARGET sicore::sicore APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(sicore::sicore PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "C"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libsicore.a"
  )

list(APPEND _cmake_import_check_targets sicore::sicore )
list(APPEND _cmake_import_check_files_for_sicore::sicore "${_IMPORT_PREFIX}/lib/libsicore.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
