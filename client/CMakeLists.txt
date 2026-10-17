# raygui is header-only: this is its single implementation translation unit.
add_library(morgane_raygui STATIC gui/raygui.cpp)
add_library(morgane::raygui ALIAS morgane_raygui)
target_include_directories(morgane_raygui PUBLIC "${RAYGUI_INCLUDE_DIR}" "${PROJECT_SOURCE_DIR}")
target_link_libraries(morgane_raygui PUBLIC siecs::spatial raylib)

add_library(morgane_graphics STATIC
    raylib/ray.cpp
    raylib/animation.cpp
    scene/scene_manager.cpp
    scene/scenes.cpp
    gui/gui.cpp
    gui/layout.cpp
    gui/draw.cpp
    gui/widgets.cpp)
target_include_directories(morgane_graphics PUBLIC "${PROJECT_SOURCE_DIR}")
target_link_libraries(morgane_graphics PUBLIC siecs::spatial raylib morgane::raygui)

add_executable(r-type_client
    src/main.cpp
    src/movement.cpp
    src/starfield.cpp
    src/visuals.cpp)
target_link_libraries(r-type_client PRIVATE morgane_graphics morgane::net morgane::physics)

# Keep assets beside the executable, including when only assets have changed.
add_custom_target(morgane_assets ALL
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${PROJECT_SOURCE_DIR}/assets" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets"
    COMMENT "Copying client assets")
add_dependencies(r-type_client morgane_assets)
if(BUILD_TESTING)
    add_executable(gui_tests gui/layout_test.cpp)
    target_link_libraries(gui_tests PRIVATE morgane_graphics Catch2::Catch2WithMain)
    catch_discover_tests(gui_tests)
endif()

install(TARGETS r-type_client RUNTIME DESTINATION bin)
install(DIRECTORY "${PROJECT_SOURCE_DIR}/assets" DESTINATION bin)
