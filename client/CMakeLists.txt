add_library(morgane_graphics STATIC
    raylib/ray.cpp
    raylib/animation.cpp
    scene/scene_manager.cpp
    scene/scenes.cpp)
target_include_directories(morgane_graphics PUBLIC "${PROJECT_SOURCE_DIR}")
target_link_libraries(morgane_graphics PUBLIC siecs::spatial raylib)

add_executable(r-type_client
    src/main.cpp
    src/movement.cpp)
target_link_libraries(r-type_client PRIVATE morgane_graphics morgane::net morgane::physics)

# Keep assets beside the executable, including when only assets have changed.
add_custom_target(morgane_assets ALL
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${PROJECT_SOURCE_DIR}/assets" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets"
    COMMENT "Copying client assets")
add_dependencies(r-type_client morgane_assets)
install(TARGETS r-type_client RUNTIME DESTINATION bin)
install(DIRECTORY "${PROJECT_SOURCE_DIR}/assets" DESTINATION bin)
