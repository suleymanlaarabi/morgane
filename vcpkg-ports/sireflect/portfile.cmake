vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO suleymanlaarabi/sireflect
    REF f746eec167dc911d0103046105d0ffed01e09bc0
    SHA512 b1066c4c2f6727280b4bc525551c28b2e23aca81495bca832ef5a546d644bedc3af3d8e13ee051ba6c64399ee40433d5d637272fe0e9cfcd28fb398b5e0eaeb4
    PATCHES cmake-support.patch
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
)
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/sireflect)
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include" "${CURRENT_PACKAGES_DIR}/debug/share")
vcpkg_install_copyright(FILE_LIST "${CMAKE_CURRENT_LIST_DIR}/copyright")
