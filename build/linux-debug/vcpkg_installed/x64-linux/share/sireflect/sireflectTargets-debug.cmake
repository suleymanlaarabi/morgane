#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "sireflect::sireflect" for configuration "Debug"
set_property(TARGET sireflect::sireflect APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(sireflect::sireflect PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "C"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/debug/lib/libsireflect.a"
  )

list(APPEND _cmake_import_check_targets sireflect::sireflect )
list(APPEND _cmake_import_check_files_for_sireflect::sireflect "${_IMPORT_PREFIX}/debug/lib/libsireflect.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
