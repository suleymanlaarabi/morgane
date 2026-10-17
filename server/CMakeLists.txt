add_library(
    morgane_server_gameplay STATIC src/movement.cpp src/players.cpp src/combat.cpp src/waves.cpp
)
target_include_directories(morgane_server_gameplay PUBLIC "${PROJECT_SOURCE_DIR}")
target_link_libraries(morgane_server_gameplay PUBLIC morgane::net morgane::physics)

add_executable(r-type_server src/main.cpp)
target_link_libraries(r-type_server PRIVATE morgane_server_gameplay)
install(TARGETS r-type_server RUNTIME DESTINATION bin)

if(BUILD_TESTING)
    add_executable(server_tests src/movement_test.cpp src/gameplay_test.cpp)
    target_link_libraries(server_tests PRIVATE morgane_server_gameplay Catch2::Catch2WithMain)
    catch_discover_tests(server_tests)
endif()
