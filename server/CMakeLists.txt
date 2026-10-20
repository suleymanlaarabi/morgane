add_library(
    morgane_server_gameplay STATIC src/movement.cpp src/players.cpp src/combat.cpp src/waves.cpp
    src/match.cpp
)
target_include_directories(morgane_server_gameplay PUBLIC "${PROJECT_SOURCE_DIR}")
target_link_libraries(morgane_server_gameplay PUBLIC morgane::net morgane::physics siecs::scene si2d::si2d siecs_editor::siecs_editor)

add_executable(r-type_server src/main.cpp)
target_link_libraries(r-type_server PRIVATE morgane_server_gameplay)
add_dependencies(r-type_server morgane_scenes)
install(TARGETS r-type_server RUNTIME DESTINATION bin)

if(BUILD_TESTING)
    add_executable(server_tests src/movement_test.cpp src/gameplay_test.cpp src/scene_test.cpp)
    target_link_libraries(server_tests PRIVATE morgane_server_gameplay Catch2::Catch2WithMain)
    add_dependencies(server_tests morgane_scenes)
    catch_discover_tests(server_tests WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
endif()
