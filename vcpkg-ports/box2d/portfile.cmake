vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO erincatto/box2d
    REF 56edae79f2949d86142b03450d5d60f63bcf5a6f
    SHA512 65d544d9a9fadf446b35570a3bdef2b604895403d74e79103155ea8ac23dea31cc73833db4fa61d4174723b2c4e26630fe398eca4e9b71dcde37a0bdc3a679ac
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DBOX2D_SAMPLES=OFF
        -DBOX2D_BENCHMARKS=OFF
        -DBOX2D_DOCS=OFF
        -DBOX2D_PROFILE=OFF
        -DBOX2D_VALIDATE=OFF
        -DBOX2D_UNIT_TESTS=OFF
        -DBOX2D_COMPILE_WARNING_AS_ERROR=OFF
)
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/box2d)
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include" "${CURRENT_PACKAGES_DIR}/debug/share")
vcpkg_fixup_pkgconfig()
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
