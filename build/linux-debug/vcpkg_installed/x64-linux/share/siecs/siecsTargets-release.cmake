#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "siecs::spatial" for configuration "Release"
set_property(TARGET siecs::spatial APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(siecs::spatial PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "C"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libsiecs_spatial.a"
  )

list(APPEND _cmake_import_check_targets siecs::spatial )
list(APPEND _cmake_import_check_files_for_siecs::spatial "${_IMPORT_PREFIX}/lib/libsiecs_spatial.a" )

# Import target "siecs::scene" for configuration "Release"
set_property(TARGET siecs::scene APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(siecs::scene PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "C"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libsiecs_scene.a"
  )

list(APPEND _cmake_import_check_targets siecs::scene )
list(APPEND _cmake_import_check_files_for_siecs::scene "${_IMPORT_PREFIX}/lib/libsiecs_scene.a" )

# Import target "siecs::siecs" for configuration "Release"
set_property(TARGET siecs::siecs APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(siecs::siecs PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "C"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libsiecs.a"
  )

list(APPEND _cmake_import_check_targets siecs::siecs )
list(APPEND _cmake_import_check_files_for_siecs::siecs "${_IMPORT_PREFIX}/lib/libsiecs.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
