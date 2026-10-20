vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
# Local checkout on scene-editor-morgane (0049ee1 plus the scene addon). origin/main
# dropped the system APIs this game is written against.
get_filename_component(SOURCE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../../siecs" ABSOLUTE)
if(NOT EXISTS "${SOURCE_PATH}/include/siecs.h")
    message(FATAL_ERROR "siecs checkout not found at ${SOURCE_PATH}")
endif()

vcpkg_check_features(OUT_FEATURE_OPTIONS FEATURE_OPTIONS FEATURES
    spatial SIECS_BUILD_SPATIAL
    scene SIECS_BUILD_SCENE
)
vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS ${FEATURE_OPTIONS}
)
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/siecs)
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include" "${CURRENT_PACKAGES_DIR}/debug/share")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
