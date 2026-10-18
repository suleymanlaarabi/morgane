# Process-level checks: a real server and headless bots talking over UDP on localhost.
add_executable(r-type_bot bot.cpp)
target_include_directories(r-type_bot PRIVATE "${PROJECT_SOURCE_DIR}")
target_link_libraries(r-type_bot PRIVATE morgane::net morgane::physics)

find_package(Python3 COMPONENTS Interpreter REQUIRED)
foreach(scenario IN ITEMS crowd crash restart)
    add_test(NAME "match ${scenario}"
        COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/match_test.py" ${scenario}
            $<TARGET_FILE:r-type_server> $<TARGET_FILE:r-type_bot>)
    set_tests_properties("match ${scenario}" PROPERTIES TIMEOUT 120 LABELS network)
endforeach()
