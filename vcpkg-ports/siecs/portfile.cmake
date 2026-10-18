vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO suleymanlaarabi/siecs
    REF 0049ee1ec94c9b510a3151870e7d57d1bceea60e
    SHA512 af7144dc20ce029a4d6f4b63e903c09e7536955da8e9fe1be73e39deaa08631134bedc123736698f45d6de4215116cc03cb9e259f7dfb2eb93a3cc6c1ab435e1
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
