vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO suleymanlaarabi/sicore
    REF 9d3aa31ac98207e6ce9b6ce3f2d92e659b5842e8
    SHA512 4881c55789ba78d83d794089c451ac11acced42a4393713e9eb4e38f1b8e7b38697e529711190cf038f4e036ec5f2f9f04633f441f37bde190020d3263c176a3
    PATCHES cmake-support.patch
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
)
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/sicore)
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include" "${CURRENT_PACKAGES_DIR}/debug/share")
vcpkg_install_copyright(FILE_LIST "${CMAKE_CURRENT_LIST_DIR}/copyright")
