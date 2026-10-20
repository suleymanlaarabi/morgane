cmake_minimum_required(VERSION 3.25)

# vcpkg evaluates manifest features during project(), before targets exist.
option(MORGANE_BUILD_CLIENT "Build the graphical client" ON)
set(VCPKG_MANIFEST_FEATURES "")
if(MORGANE_BUILD_CLIENT)
    list(APPEND VCPKG_MANIFEST_FEATURES client)
endif()

project(morgane LANGUAGES C CXX)
include(CTest)
find_package(Threads REQUIRED)
find_package(siecs CONFIG REQUIRED)
find_package(si2d CONFIG REQUIRED)
find_package(siecs_editor CONFIG REQUIRED)
find_package(box2d CONFIG REQUIRED)
if(BUILD_TESTING)
    find_package(Catch2 3 CONFIG REQUIRED)
    include(Catch)
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
file(CREATE_LINK "${CMAKE_BINARY_DIR}/compile_commands.json"
    "${CMAKE_SOURCE_DIR}/compile_commands.json" SYMBOLIC RESULT compile_commands_link_result)
if(NOT compile_commands_link_result STREQUAL "0")
    message(WARNING "Could not expose compile_commands.json: ${compile_commands_link_result}")
endif()
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

add_subdirectory(libs/net)
add_subdirectory(libs/physics)
add_subdirectory(server)

set(MORGANE_SCENE_JSON "${PROJECT_SOURCE_DIR}/scenes/main.scene.json")
set(MORGANE_SCENE_COOKED "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/scenes/main.scene.siecsscn")
add_custom_command(
    OUTPUT "${MORGANE_SCENE_COOKED}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/scenes"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${PROJECT_SOURCE_DIR}/scenes" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/scenes"
    COMMAND python3 "${PROJECT_SOURCE_DIR}/scripts/cook_scene.py"
        "${MORGANE_SCENE_JSON}" "${MORGANE_SCENE_COOKED}"
    DEPENDS "${MORGANE_SCENE_JSON}" "${PROJECT_SOURCE_DIR}/scripts/cook_scene.py"
    COMMENT "Cooking scenes to SIECSSCN")
add_custom_target(morgane_scenes ALL DEPENDS "${MORGANE_SCENE_COOKED}")
install(DIRECTORY "${PROJECT_SOURCE_DIR}/scenes" DESTINATION bin)
install(FILES "${MORGANE_SCENE_COOKED}" DESTINATION bin/scenes)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
if(MORGANE_BUILD_CLIENT)
    find_package(raylib CONFIG REQUIRED)
    # raygui is header-only and ships no CMake config; skip /usr/include so a system copy never wins.
    find_path(RAYGUI_INCLUDE_DIR raygui.h REQUIRED NO_CMAKE_SYSTEM_PATH NO_SYSTEM_ENVIRONMENT_PATH)
    add_subdirectory(client)
endif()
