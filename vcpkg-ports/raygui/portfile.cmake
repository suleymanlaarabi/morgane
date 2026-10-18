# Header-only: raygui.h plus the embeddable styles (include/raygui/styles/style_*.h).
set(VCPKG_BUILD_TYPE release)
vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO raysan5/raygui
    REF ca17a68d1a32abede1155c9826427482aa72d43f
    SHA512 5ef10217981c046e85959975cf920cbf020c0374524a67b0fa8f2fe76bd85e0cc1b3228c2821187e3c86f8d1485f2368d3672a06bfc0b5ce6ed5353360ccf028
)

file(INSTALL "${SOURCE_PATH}/src/raygui.h" DESTINATION "${CURRENT_PACKAGES_DIR}/include")
file(GLOB styles "${SOURCE_PATH}/styles/*/style_*.h")
file(INSTALL ${styles} DESTINATION "${CURRENT_PACKAGES_DIR}/include/raygui/styles")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
