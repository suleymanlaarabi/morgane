#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "sijson::sijson" for configuration "Release"
set_property(TARGET sijson::sijson APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(sijson::sijson PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "C"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libsijson.a"
  )

list(APPEND _cmake_import_check_targets sijson::sijson )
list(APPEND _cmake_import_check_files_for_sijson::sijson "${_IMPORT_PREFIX}/lib/libsijson.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
