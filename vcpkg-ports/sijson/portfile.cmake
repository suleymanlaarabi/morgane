vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO suleymanlaarabi/sijson
    REF 0fdee96cb5e0c4ab3bf5dc790ca64f46ebd3998b
    SHA512 dbed83c4a5b92785b7003e69cab25ccec295cfc452a3637f5959ce886dac0314d27938763e8219a00d5d80b926d4f8857ca9c603392850e195bd3ef04dfa4e3d
    PATCHES cmake-support.patch
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
)
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/sijson)
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include" "${CURRENT_PACKAGES_DIR}/debug/share")
vcpkg_install_copyright(FILE_LIST "${CMAKE_CURRENT_LIST_DIR}/copyright")
