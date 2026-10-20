vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
get_filename_component(SOURCE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../../si2d" ABSOLUTE)
if(NOT EXISTS "${SOURCE_PATH}/include/si2d.h")
    message(FATAL_ERROR "si2d checkout not found at ${SOURCE_PATH}")
endif()

vcpkg_check_features(OUT_FEATURE_OPTIONS FEATURE_OPTIONS FEATURES raylib SI2D_BUILD_RAYLIB)
vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS ${FEATURE_OPTIONS}
)
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/si2d)
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include" "${CURRENT_PACKAGES_DIR}/debug/share")
file(WRITE "${CURRENT_PACKAGES_DIR}/share/${PORT}/copyright" "MIT")
