vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO raysan5/raylib
    REF 02566453880e06bec97f5b8160fa6fe1e98397ca
    SHA512 cec365c093c8103d06f0d43a8ed44e5c0c5b6ed73a5279628632b07b8a95c44665302a921e59727ed0c38cf62d4151d4a7210153dc21c175cb39f684655f6560
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DBUILD_EXAMPLES=OFF
        -DUSE_EXTERNAL_GLFW=OFF
        -DGLFW_BUILD_WAYLAND=OFF
        -DGLFW_BUILD_X11=ON
)
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/raylib)
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include" "${CURRENT_PACKAGES_DIR}/debug/share")
vcpkg_fixup_pkgconfig()
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
