vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO suleymanlaarabi/siecs
    REF d3933a1b799f149877017fca75dbf4cfa6016299
    SHA512 1f1975c840ad729c861f1b14751e91c7ff14b9ef507015ad7cbbe16a9514b5e3565b037dd38257be8d87c6009b0cff9e15e82e196758c67dc2914308686a2cf0
    PATCHES cmake-support.patch
)

vcpkg_check_features(OUT_FEATURE_OPTIONS FEATURE_OPTIONS FEATURES spatial SIECS_BUILD_SPATIAL)
vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS ${FEATURE_OPTIONS}
)
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/siecs)
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include" "${CURRENT_PACKAGES_DIR}/debug/share")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
