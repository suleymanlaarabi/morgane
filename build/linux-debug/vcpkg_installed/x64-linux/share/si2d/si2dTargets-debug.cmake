#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "si2d::si2d" for configuration "Debug"
set_property(TARGET si2d::si2d APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(si2d::si2d PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "C"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/debug/lib/libsi2d.a"
  )

list(APPEND _cmake_import_check_targets si2d::si2d )
list(APPEND _cmake_import_check_files_for_si2d::si2d "${_IMPORT_PREFIX}/debug/lib/libsi2d.a" )

# Import target "si2d::raylib" for configuration "Debug"
set_property(TARGET si2d::raylib APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(si2d::raylib PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "C"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/debug/lib/libsi2d_raylib.a"
  )

list(APPEND _cmake_import_check_targets si2d::raylib )
list(APPEND _cmake_import_check_files_for_si2d::raylib "${_IMPORT_PREFIX}/debug/lib/libsi2d_raylib.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
