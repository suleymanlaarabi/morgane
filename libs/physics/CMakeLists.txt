add_library(sibox2d STATIC
    src/physics.cpp
    src/lifecycle.cpp
    src/motion.cpp
    src/contacts.cpp)
add_library(morgane::physics ALIAS sibox2d)
target_include_directories(sibox2d PUBLIC include)
target_link_libraries(sibox2d PUBLIC siecs::spatial PRIVATE box2d::box2d)

if(BUILD_TESTING)
    add_executable(physics_tests src/physics_test.cpp)
    target_link_libraries(physics_tests PRIVATE sibox2d Catch2::Catch2WithMain)
    catch_discover_tests(physics_tests)
endif()
